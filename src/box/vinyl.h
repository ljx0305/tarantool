#ifndef INCLUDES_TARANTOOL_BOX_VINYL_H
#define INCLUDES_TARANTOOL_BOX_VINYL_H
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

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

struct vy_env;
struct vy_tx;
struct vy_cursor;
struct vy_index;
struct index_def;
struct tuple;
struct tuple_format;
struct vclock;
struct request;
struct space;
struct txn_stmt;
struct xrow_header;
struct xstream;
enum iterator_type;

/*
 * Environment
 */

struct vy_env *
vy_env_new(const char *path, size_t memory, size_t cache, int read_threads,
	   int write_threads, double timeout);

void
vy_env_delete(struct vy_env *e);

/*
 * Recovery
 */

int
vy_bootstrap(struct vy_env *e);

int
vy_begin_initial_recovery(struct vy_env *e,
			  const struct vclock *recovery_vclock);

int
vy_begin_final_recovery(struct vy_env *e);

int
vy_end_recovery(struct vy_env *e);

/*
 * Checkpoint
 */

int
vy_begin_checkpoint(struct vy_env *env);

int
vy_wait_checkpoint(struct vy_env *env, struct vclock *vclock);

void
vy_commit_checkpoint(struct vy_env *env, struct vclock *vclock);

void
vy_abort_checkpoint(struct vy_env *env);

/*
 * Introspection
 */

struct info_handler;

/*
 * Engine introspection (box.info.vinyl())
 *
 * @param env environment
 * @param handler info handler
 */
void
vy_info(struct vy_env *env, struct info_handler *handler);

/**
 * Index introspection (index:info())
 *
 * @param index index
 * @param handler info handler
 */
void
vy_index_info(struct vy_index *index, struct info_handler *handler);

/*
 * Transaction
 */

struct vy_tx *
vy_begin(struct vy_env *e);

/**
 * Get a tuple from the vinyl index.
 * @param env         Vinyl environment.
 * @param tx          Current transaction.
 * @param index       Vinyl index.
 * @param key         MessagePack'ed data, the array without a
 *                    header.
 * @param part_count  Part count of the key
 * @param[out] result Is set to the the found tuple.
 *
 * @retval  0 Success.
 * @retval -1 Memory or read error.
 */
int
vy_get(struct vy_env *env, struct vy_tx *tx, struct vy_index *index,
       const char *key, uint32_t part_count, struct tuple **result);

/**
 * Execute REPLACE in a vinyl space.
 * @param env     Vinyl environment.
 * @param tx      Current transaction.
 * @param stmt    Statement for triggers filled with old
 *                statement.
 * @param space   Vinyl space.
 * @param request Request with the tuple data.
 *
 * @retval  0 Success
 * @retval -1 Memory error OR duplicate key error OR the primary
 *            index is not found OR a tuple reference increment
 *            error.
 */
int
vy_replace(struct vy_env *env, struct vy_tx *tx, struct txn_stmt *stmt,
	   struct space *space, struct request *request);

/**
 * Execute DELETE in a vinyl space.
 * @param env     Vinyl environment.
 * @param tx      Current transaction.
 * @param stmt    Statement for triggers filled with deleted
 *                statement.
 * @param space   Vinyl space.
 * @param request Request with the tuple data.
 *
 * @retval  0 Success
 * @retval -1 Memory error OR the index is not found OR a tuple
 *            reference increment error.
 */
int
vy_delete(struct vy_env *env, struct vy_tx *tx, struct txn_stmt *stmt,
	  struct space *space, struct request *request);

/**
 * Execute UPDATE in a vinyl space.
 * @param env     Vinyl environment.
 * @param tx      Current transaction.
 * @param stmt    Statement for triggers filled with old and new
 *                statements.
 * @param space   Vinyl space.
 * @param request Request with the tuple data.
 *
 * @retval  0 Success
 * @retval -1 Memory error OR the index is not found OR a tuple
 *            reference increment error.
 */
int
vy_update(struct vy_env *env, struct vy_tx *tx, struct txn_stmt *stmt,
	  struct space *space, struct request *request);

/**
 * Execute UPSERT in a vinyl space.
 * @param env     Vinyl environment.
 * @param tx      Current transaction.
 * @param stmt    Statement for triggers filled with old and new
 *                statements.
 * @param space   Vinyl space.
 * @param request Request with the tuple data and update
 *                operations.
 *
 * @retval  0 Success
 * @retval -1 Memory error OR the index is not found OR a tuple
 *            reference increment error.
 */
int
vy_upsert(struct vy_env *env, struct vy_tx *tx, struct txn_stmt *stmt,
	  struct space *space, struct request *request);

int
vy_prepare(struct vy_env *env, struct vy_tx *tx);

void
vy_commit(struct vy_env *env, struct vy_tx *tx, int64_t lsn);

void
vy_rollback(struct vy_env *env, struct vy_tx *tx);

void *
vy_savepoint(struct vy_env *env, struct vy_tx *tx);

void
vy_rollback_to_savepoint(struct vy_env *env, struct vy_tx *tx, void *svp);

/*
 * Index
 */

struct Index;
/**
 * Extract vy_index from a VinylIndex object.
 * Defined in vinyl_index.cc
 */
struct vy_index *
vy_index(struct Index *index);

struct field_def;
/**
 * Create a new vinyl index object without opening it.
 * @param env Vinyl environment.
 * @param index_def Index definition.
 * @param format Space format.
 * @param pk Primary index.
 */
struct vy_index *
vy_new_index(struct vy_env *env, struct index_def *index_def,
	     struct tuple_format *format, struct vy_index *pk);

/**
 * Delete a vinyl index object.
 * @param env    Vinyl environment.
 * @param index  Index object.
 */
void
vy_delete_index(struct vy_env *env, struct vy_index *index);

/**
 * Handle vinyl space truncation.
 *
 * This function initializes indexes of the new space
 * so that it can replace the old space on truncation.
 */
int
vy_prepare_truncate_space(struct vy_env *env, struct space *old_space,
			  struct space *new_space);

/**
 * Commit space truncation in the metadata log.
 */
void
vy_commit_truncate_space(struct vy_env *env, struct space *old_space,
			 struct space *new_space);

/**
 * Hook on an preparation of space alter event.
 * @param env       Vinyl environment.
 * @param old_space Old space.
 * @param new_space New space.
 *
 * @retval  0 Success.
 * @retval -1 Error.
 */
int
vy_prepare_alter_space(struct vy_env *env, struct space *old_space,
		       struct space *new_space);

/**
 * Hook on an alter space commit event. It is called on each
 * create_index(), drop_index() and is used for update
 * vy_index.space attribute.
 * @param env       Vinyl environment.
 * @param old_space Old space.
 * @param new_space New space.
 *
 * @retval  0 Success.
 * @retval -1 Memory or new format register error.
 */
int
vy_commit_alter_space(struct vy_env *env, struct space *new_space,
		      struct tuple_format *new_format);

/**
 * Open a vinyl index.
 *
 * During recovery, this function loads run files from
 * the index directory. After recovery is complete, it
 * creates the index directory.
 */
int
vy_index_open(struct vy_env *env, struct vy_index *index, bool force_recovery);

/**
 * Commit index creation in the metadata log.
 */
void
vy_index_commit_create(struct vy_env *env, struct vy_index *index, int64_t lsn);

/**
 * Commit index drop in the metadata log.
 */
void
vy_index_commit_drop(struct vy_env *env, struct vy_index *index);

size_t
vy_index_bsize(struct vy_index *index);

/*
 * Index Cursor
 */

/**
 * Create a cursor. If tx is not NULL, the cursor life time is
 * bound by the transaction life time. Otherwise, the cursor
 * allocates its own transaction.
 */
struct vy_cursor *
vy_cursor_new(struct vy_env *env, struct vy_tx *tx, struct vy_index *index,
	      const char *key, uint32_t part_count, enum iterator_type type);

void
vy_cursor_delete(struct vy_env *env, struct vy_cursor *cursor);

int
vy_cursor_next(struct vy_env *env, struct vy_cursor *cursor,
	       struct tuple **result);

/*
 * Replication
 */

int
vy_join(struct vy_env *env, struct vclock *vclock, struct xstream *stream);

/*
 * Garbage collection
 */

void
vy_collect_garbage(struct vy_env *env, int64_t lsn);

/*
 * Backup
 */

int
vy_backup(struct vy_env *env, struct vclock *vclock,
	  int (*cb)(const char *, void *), void *cb_arg);

/*
 * Configuration
 */

/**
 * Update max tuple size.
 */
int
vy_set_max_tuple_size(struct vy_env *env, size_t max_size);

/**
 * Update query timeout.
 */
int
vy_set_timeout(struct vy_env *env, double timeout);

#ifdef __cplusplus
}
#endif

#endif /* INCLUDES_TARANTOOL_BOX_VINYL_H */
