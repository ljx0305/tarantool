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
#include "scoped_guard.h"

struct SysviewSpace: public Handler {
	virtual void destroy(struct space *space) override;
	virtual struct tuple *
	executeReplace(struct txn *, struct space *, struct request *) override;
	virtual struct tuple *
	executeDelete(struct txn *, struct space *, struct request *) override;
	virtual struct tuple *
	executeUpdate(struct txn *, struct space *, struct request *) override;
	virtual void
	executeUpsert(struct txn *, struct space *, struct request *) override;

	virtual Index *createIndex(struct space *space,
				   struct index_def *index_def) override;
	virtual void buildSecondaryKey(struct space *old_space,
				       struct space *new_space,
				       Index *new_index) override;
};

void SysviewSpace::destroy(struct space *space)
{
	delete space->handler;
	free(space);
}

struct tuple *
SysviewSpace::executeReplace(struct txn *, struct space *space,
			      struct request *)
{
	tnt_raise(ClientError, ER_VIEW_IS_RO, space->def->name);
	return NULL;
}

struct tuple *
SysviewSpace::executeDelete(struct txn*, struct space *space, struct request *)
{
	tnt_raise(ClientError, ER_VIEW_IS_RO, space->def->name);
	return NULL;
}

struct tuple *
SysviewSpace::executeUpdate(struct txn*, struct space *space, struct request *)
{
	tnt_raise(ClientError, ER_VIEW_IS_RO, space->def->name);
	return NULL;
}

void
SysviewSpace::executeUpsert(struct txn *, struct space *space, struct request *)
{
	tnt_raise(ClientError, ER_VIEW_IS_RO, space->def->name);
}

Index *
SysviewSpace::createIndex(struct space *space, struct index_def *index_def)
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
		return NULL;
	}
}

void
SysviewSpace::buildSecondaryKey(struct space *, struct space *, Index *)
{}

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
	auto space_guard = make_scoped_guard([=] { free(space); });
	space->handler = new SysviewSpace();
	space_guard.is_active = false;
	return space;
}
