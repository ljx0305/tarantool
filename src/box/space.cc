/*
 * Copyright 2010-2016, Tarantool AUTHORS, please see AUTHORS file.
 *
 * Redistribution and use in source and binary forms, with or
 * without modification, are permitted provided that the following
 * conditions are met:
 *
 * 1. Redistributions of source code must retain the above
 *    copyright notice, this list of conditions and the
 *    following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials
 *    provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY <COPYRIGHT HOLDER> ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
 * <COPYRIGHT HOLDER> OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
#include "space.h"
#include <stdlib.h>
#include <string.h>
#include "tuple.h"
#include "tuple_compare.h"
#include "scoped_guard.h"
#include "trigger.h"
#include "user.h"
#include "session.h"
#include "port.h"

void
access_check_space(struct space *space, uint8_t access)
{
	struct credentials *cr = current_user();
	/*
	 * If a user has a global permission, clear the respective
	 * privilege from the list of privileges required
	 * to execute the request.
	 * No special check for ADMIN user is necessary
	 * since ADMIN has universal access.
	 */
	access &= ~cr->universal_access;
	if (access && space->def->uid != cr->uid &&
	    access & ~space->access[cr->auth_token].effective) {
		/*
		 * Report access violation. Throw "no such user"
		 * error if there is  no user with this id.
		 * It is possible that the user was dropped
		 * from a different connection.
		 */
		struct user *user = user_find_xc(cr->uid);
		tnt_raise(ClientError, ER_SPACE_ACCESS_DENIED,
			  priv_name(access), user->def->name, space->def->name);
	}
}

void
space_fill_index_map(struct space *space)
{
	uint32_t index_count = 0;
	for (uint32_t j = 0; j <= space->index_id_max; j++) {
		Index *index = space->index_map[j];
		if (index) {
			assert(index_count < space->index_count);
			space->index[index_count++] = index;
		}
	}
}

struct space *
space_new(struct space_def *def, struct rlist *key_list)
{
	uint32_t index_id_max = 0;
	uint32_t index_count = 0;
	struct index_def *index_def;
	MAYBE_UNUSED struct index_def *pk = rlist_empty(key_list) ? NULL :
		rlist_first_entry(key_list, struct index_def, link);
	rlist_foreach_entry(index_def, key_list, link) {
		index_count++;
		index_id_max = MAX(index_id_max, index_def->iid);
	}
	/* A space with a secondary key without a primary is illegal. */
	assert(index_count == 0 || pk->iid == 0);
	/* Allocate a space engine instance. */
	Engine *engine = engine_find(def->engine_name);
	struct space *space = engine->createSpace();
	auto scoped_guard = make_scoped_guard([=] { space_delete(space); });
	space->engine = engine;
	space->index_count = index_count;
	space->index_id_max = index_id_max;
	rlist_create(&space->on_replace);
	rlist_create(&space->on_stmt_begin);
	space->def = space_def_dup_xc(def);
	/* Construct a tuple format for the new space. */
	uint32_t key_no = 0;
	struct key_def **keys = (struct key_def **)
		region_alloc_xc(&fiber()->gc, sizeof(*keys) * index_count);
	rlist_foreach_entry(index_def, key_list, link)
		keys[key_no++] = index_def->key_def;
	space->format = engine->createFormat(keys, index_count,
		def->fields, def->field_count, def->exact_field_count);
	if (space->format != NULL)
		tuple_format_ref(space->format);
	/** Initialize index map. */
	space->index_map = (Index **)calloc(index_count + index_id_max + 1,
					    sizeof(Index *));
	if (space->index_map == NULL) {
		tnt_raise(OutOfMemory, (index_id_max + 1) * sizeof(Index *),
			  "malloc", "index_map");
	}
	space->index = space->index_map + index_id_max + 1;
	rlist_foreach_entry(index_def, key_list, link) {
		space->index_map[index_def->iid] =
			space->vtab->create_index(space, index_def);
	}
	space_fill_index_map(space);
	space->run_triggers = true;
	scoped_guard.is_active = false;
	return space;
}

void
space_delete(struct space *space)
{
	for (uint32_t j = 0; space->index_map != NULL &&
			     j <= space->index_id_max; j++) {
		Index *index = space->index_map[j];
		if (index)
			delete index;
	}
	free(space->index_map);
	if (space->format != NULL)
		tuple_format_unref(space->format);
	trigger_destroy(&space->on_replace);
	trigger_destroy(&space->on_stmt_begin);
	space_def_delete(space->def);
	space->vtab->destroy(space);
}

/** Do nothing if the space is already recovered. */
void
space_noop(struct space * /* space */)
{}

uint32_t
space_size(struct space *space)
{
	return space_index(space, 0)->size();
}

void
space_dump_def(const struct space *space, struct rlist *key_list)
{
	rlist_create(key_list);

	/** Ensure the primary key is added first. */
	for (unsigned j = 0; j < space->index_count; j++)
		rlist_add_tail_entry(key_list, space->index[j]->index_def,
				     link);
}

struct key_def *
space_index_key_def(struct space *space, uint32_t id)
{
	if (id <= space->index_id_max && space->index_map[id])
		return space->index_map[id]->index_def->key_def;
	return NULL;
}

void
space_swap_index(struct space *lhs, struct space *rhs,
		 uint32_t lhs_id, uint32_t rhs_id)
{
	Index *tmp = lhs->index_map[lhs_id];
	lhs->index_map[lhs_id] = rhs->index_map[rhs_id];
	rhs->index_map[rhs_id] = tmp;
}

extern "C" void
space_run_triggers(struct space *space, bool yesno)
{
	space->run_triggers = yesno;
}

size_t
space_bsize(struct space *space)
{
	return space->vtab->bsize(space);
}

struct index_def *
space_index_def(struct space *space, int n)
{
	return space->index[n]->index_def;
}

const char *
index_name_by_id(struct space *space, uint32_t id)
{
	struct Index *index = space_index(space, id);
	if (index != NULL)
		return index->index_def->name;
	return NULL;
}

void
generic_space_execute_select(struct space *space, struct txn *txn,
			     uint32_t index_id, uint32_t iterator,
			     uint32_t offset, uint32_t limit,
			     const char *key, const char *key_end,
			     struct port *port)
{
	(void)txn;
	(void)key_end;

	Index *index = index_find_xc(space, index_id);

	uint32_t found = 0;
	if (iterator >= iterator_type_MAX)
		tnt_raise(IllegalParams, "Invalid iterator type");
	enum iterator_type type = (enum iterator_type) iterator;

	uint32_t part_count = key ? mp_decode_array(&key) : 0;
	if (key_validate(index->index_def, type, key, part_count))
		diag_raise();

	struct iterator *it = index->allocIterator();
	IteratorGuard guard(it);
	index->initIterator(it, type, key, part_count);

	struct tuple *tuple;
	while (found < limit && (tuple = it->next(it)) != NULL) {
		if (offset > 0) {
			offset--;
			continue;
		}
		port_add_tuple_xc(port, tuple);
		found++;
	}
}
