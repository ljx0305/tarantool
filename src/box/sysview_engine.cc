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
#include "sysview_engine.h"
#include "sysview_index.h"
#include "schema.h"
#include "space.h"

static void
sysview_space_destroy(struct space *space)
{
	free(space);
}

static size_t
sysview_space_bsize(struct space *)
{
	return 0;
}

static void
sysview_space_apply_initial_join_row(struct space *, struct request *)
{
	unreachable();
}

static struct tuple *
sysview_space_execute_replace(struct space *space, struct txn *,
			      struct request *)
{
	tnt_raise(ClientError, ER_VIEW_IS_RO, space->def->name);
}

static struct tuple *
sysview_space_execute_delete(struct space *space, struct txn *,
			     struct request *)
{
	tnt_raise(ClientError, ER_VIEW_IS_RO, space->def->name);
}

static struct tuple *
sysview_space_execute_update(struct space *space, struct txn *,
			     struct request *)
{
	tnt_raise(ClientError, ER_VIEW_IS_RO, space->def->name);
}

static void
sysview_space_execute_upsert(struct space *space, struct txn *,
			     struct request *)
{
	tnt_raise(ClientError, ER_VIEW_IS_RO, space->def->name);
}

static void
sysview_init_system_space(struct space *)
{
	unreachable();
}

static void
sysview_space_check_index_def(struct space *, struct index_def *)
{
}

static struct Index *
sysview_space_create_index(struct space *space, struct index_def *index_def)
{
	assert(index_def->type == TREE);
	switch (index_def->space_id) {
	case BOX_VSPACE_ID:
		return new SysviewVspaceIndex(index_def);
	case BOX_VINDEX_ID:
		return new SysviewVindexIndex(index_def);
	case BOX_VUSER_ID:
		return new SysviewVuserIndex(index_def);
	case BOX_VFUNC_ID:
		return new SysviewVfuncIndex(index_def);
	case BOX_VPRIV_ID:
		return new SysviewVprivIndex(index_def);
	default:
		tnt_raise(ClientError, ER_MODIFY_INDEX, index_def->name,
			  space_name(space), "unknown space for system view");
	}
}

static void
sysview_space_add_primary_key(struct space *)
{
}

static void
sysview_space_drop_primary_key(struct space *)
{
}

static void
sysview_space_build_secondary_key(struct space *, struct space *,
				  struct Index *)
{
}

static void
sysview_space_prepare_truncate(struct space *, struct space *)
{
}

static void
sysview_space_commit_truncate(struct space *, struct space *)
{
}

static void
sysview_space_prepare_alter(struct space *, struct space *)
{
}

static void
sysview_space_commit_alter(struct space *, struct space *)
{
}

static const struct space_vtab sysview_space_vtab = {
	/* .destroy = */ sysview_space_destroy,
	/* .bsize = */ sysview_space_bsize,
	/* .apply_initial_join_row = */ sysview_space_apply_initial_join_row,
	/* .execute_replace = */ sysview_space_execute_replace,
	/* .execute_delete = */ sysview_space_execute_delete,
	/* .execute_update = */ sysview_space_execute_update,
	/* .execute_upsert = */ sysview_space_execute_upsert,
	/* .execute_select = */ generic_space_execute_select,
	/* .init_system_space = */ sysview_init_system_space,
	/* .check_index_def = */ sysview_space_check_index_def,
	/* .create_index = */ sysview_space_create_index,
	/* .add_primary_key = */ sysview_space_add_primary_key,
	/* .drop_primary_key = */ sysview_space_drop_primary_key,
	/* .build_secondary_key = */ sysview_space_build_secondary_key,
	/* .prepare_truncate = */ sysview_space_prepare_truncate,
	/* .commit_truncate = */ sysview_space_commit_truncate,
	/* .prepare_alter = */ sysview_space_prepare_alter,
	/* .commit_alter = */ sysview_space_commit_alter,
};

SysviewEngine::SysviewEngine()
	:Engine("sysview")
{
}

struct space *SysviewEngine::createSpace()
{
	struct space *space = (struct space *)calloc(1, sizeof(*space));
	if (space == NULL)
		tnt_raise(OutOfMemory, sizeof(*space),
			  "malloc", "struct space");
	space->vtab = &sysview_space_vtab;
	return space;
}
