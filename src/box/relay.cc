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
#include "relay.h"

#include "trivia/config.h"
#include "trivia/util.h"
#include "cbus.h"
#include "cfg.h"
#include "errinj.h"
#include "fiber.h"
#include "say.h"

#include "coio.h"
#include "coio_task.h"
#include "engine.h"
#include "gc.h"
#include "iproto_constants.h"
#include "recovery.h"
#include "replication.h"
#include "trigger.h"
#include "vclock.h"
#include "version.h"
#include "xrow.h"
#include "xrow_io.h"
#include "xstream.h"
#include "wal.h"
#include "scoped_guard.h"

/** Network timeout */
double relay_timeout;

/**
 * Cbus message to send status updates from relay to tx thread.
 */
struct relay_status_msg {
	/** Parent */
	struct cmsg msg;
	/** Relay instance */
	struct relay *relay;
	/** Replica vclock. */
	struct vclock vclock;
};

/**
 * Cbus message to update replica gc state in tx thread.
 */
struct relay_gc_msg {
	/** Parent */
	struct cmsg msg;
	/** Relay instance */
	struct relay *relay;
	/** Vclock signature to advance to */
	int64_t signature;
};

/** State of a replication relay. */
struct relay {
	/** The thread in which we relay data to the replica. */
	struct cord cord;
	/** Replica connection */
	struct ev_io io;
	/** Request sync */
	uint64_t sync;
	/** Recovery instance to read xlog from the disk */
	struct recovery *r;
	/** Xstream argument to recovery */
	struct xstream stream;
	/** Vclock to stop playing xlogs */
	struct vclock stop_vclock;
	/** Remote replica */
	struct replica *replica;
	/** WAL event watcher. */
	struct wal_watcher wal_watcher;
	/** Set before exiting the relay loop. */
	bool exiting;
	/** Relay reader cond. */
	struct fiber_cond reader_cond;
	/** Relay diagnostics. */
	struct diag diag;
	/** Vclock recieved from replica. */
	struct vclock recv_vclock;
	/** Replicatoin slave version. */
	uint32_t version_id;
	/** Xrow batch to send to applier. */
	struct xrow_batch batch;
	/**
	 * Offset in ibuf to the end of a previous xrow. This
	 * position is also begin of a next xrow.
	 * After decoding the next xrow, we can determine its
	 * encoded size by current_ibuf_rpos - prev_row_end.
	 */
	off_t prev_row_end;

	/** Relay endpoint */
	struct cbus_endpoint endpoint;
	/** A pipe from 'relay' thread to 'tx' */
	struct cpipe tx_pipe;
	/** A pipe from 'tx' thread to 'relay' */
	struct cpipe relay_pipe;
	/** Status message */
	struct relay_status_msg status_msg;

	struct {
		/* Align to prevent false-sharing with tx thread */
		alignas(CACHELINE_SIZE)
		/** Known relay vclock. */
		struct vclock vclock;
	} tx;
};

const struct vclock *
relay_vclock(const struct relay *relay)
{
	return &relay->tx.vclock;
}

static void
relay_send_initial_join_row(struct xstream *stream, struct xrow_header *row);
static void
relay_send_row(struct xstream *stream, struct xrow_header *row);
static void
relay_add_row(struct xstream *stream, struct xrow_header *row);
static void
relay_begin_tx(struct xstream *stream);
static void
relay_send_tx(struct xstream *stream);

static inline void
relay_create(struct relay *relay, int fd, uint64_t sync,
	     xstream_write_f stream_write, xstream_tx_f stream_begin,
	     xstream_tx_f stream_commit)
{
	memset(relay, 0, sizeof(*relay));
	xstream_create(&relay->stream, stream_write, stream_begin,
		       stream_commit, NULL);
	coio_create(&relay->io, fd);
	relay->sync = sync;
	fiber_cond_create(&relay->reader_cond);
	diag_create(&relay->diag);
	xrow_batch_create(&relay->batch);
}

static inline void
relay_destroy(struct relay *relay)
{
	if (relay->r != NULL)
		recovery_delete(relay->r);
	fiber_cond_destroy(&relay->reader_cond);
	diag_destroy(&relay->diag);
	xrow_batch_destroy(&relay->batch);
}

static inline void
relay_set_cord_name(int fd)
{
	char name[FIBER_NAME_MAX];
	struct sockaddr_storage peer;
	socklen_t addrlen = sizeof(peer);
	if (getpeername(fd, ((struct sockaddr*)&peer), &addrlen) == 0) {
		snprintf(name, sizeof(name), "relay/%s",
			 sio_strfaddr((struct sockaddr *)&peer, addrlen));
	} else {
		snprintf(name, sizeof(name), "relay/<unknown>");
	}
	cord_set_name(name);
}

void
relay_initial_join(int fd, uint64_t sync, struct vclock *vclock)
{
	struct relay relay;
	relay_create(&relay, fd, sync, relay_send_initial_join_row, NULL, NULL);
	auto relay_guard = make_scoped_guard([&] { relay_destroy(&relay); });
	assert(relay.stream.write != NULL);
	engine_join(vclock, &relay.stream);
}

int
relay_final_join_f(va_list ap)
{
	struct relay *relay = va_arg(ap, struct relay *);
	coio_enable();
	relay_set_cord_name(relay->io.fd);

	/* Send all WALs until stop_vclock */
	assert(relay->stream.write != NULL);
	recover_remaining_wals(relay->r, &relay->stream,
			       &relay->stop_vclock, true);
	assert(vclock_compare(&relay->r->vclock, &relay->stop_vclock) == 0);
	return 0;
}

void
relay_final_join(int fd, uint64_t sync, struct vclock *start_vclock,
	         struct vclock *stop_vclock)
{
	struct relay relay;
	relay_create(&relay, fd, sync, relay_send_row, NULL, NULL);
	auto relay_guard = make_scoped_guard([&] { relay_destroy(&relay); });
	relay.r = recovery_new(cfg_gets("wal_dir"),
			       cfg_geti("force_recovery"),
			       start_vclock);
	vclock_copy(&relay.stop_vclock, stop_vclock);

	int rc = cord_costart(&relay.cord, "final_join",
			      relay_final_join_f, &relay);
	if (rc == 0)
		rc = cord_cojoin(&relay.cord);
	if (rc != 0)
		diag_raise();

	ERROR_INJECT(ERRINJ_RELAY_FINAL_SLEEP, {
		while (vclock_compare(stop_vclock, &replicaset_vclock) == 0)
			fiber_sleep(0.001);
	});
}

/**
 * The message which updated tx thread with a new vclock has returned back
 * to the relay.
 */
static void
relay_status_update(struct cmsg *msg)
{
	msg->route = NULL;
}

/**
 * Deliver a fresh relay vclock to tx thread.
 */
static void
tx_status_update(struct cmsg *msg)
{
	struct relay_status_msg *status = (struct relay_status_msg *)msg;
	vclock_copy(&status->relay->tx.vclock, &status->vclock);
	static const struct cmsg_hop route[] = {
		{relay_status_update, NULL}
	};
	cmsg_init(msg, route);
	cpipe_push(&status->relay->relay_pipe, msg);
}

/**
 * Update replica gc state in tx thread.
 */
static void
tx_gc_advance(struct cmsg *msg)
{
	struct relay_gc_msg *m = (struct relay_gc_msg *)msg;
	gc_consumer_advance(m->relay->replica->gc, m->signature);
	free(m);
}

static void
relay_on_close_log_f(struct trigger *trigger, void * /* event */)
{
	static const struct cmsg_hop route[] = {
		{tx_gc_advance, NULL}
	};
	struct relay *relay = (struct relay *)trigger->data;
	struct relay_gc_msg *m = (struct relay_gc_msg *)malloc(sizeof(*m));
	if (m == NULL) {
		say_warn("failed to allocate relay gc message");
		return;
	}
	cmsg_init(&m->msg, route);
	m->relay = relay;
	m->signature = vclock_sum(&relay->r->vclock);
	cpipe_push(&relay->tx_pipe, &m->msg);
}

static void
relay_process_wal_event(struct wal_watcher *watcher, unsigned events)
{
	struct relay *relay = container_of(watcher, struct relay, wal_watcher);
	if (relay->exiting) {
		/*
		 * Do not try to send anything to the replica
		 * if it already closed its socket.
		 */
		return;
	}
	try {
		recover_remaining_wals(relay->r, &relay->stream, NULL,
				       (events & WAL_EVENT_ROTATE) != 0);
	} catch (Exception *e) {
		e->log();
		diag_move(diag_get(), &relay->diag);
		fiber_cancel(fiber());
	}
}

/* Count of replication_timeout periods to disconnect the client. */
#define TIMEOUT_PERIODS 4

/*
 * Relay reader fiber function.
 * Read xrow encoded vclocks sent by the replica.
 */
int
relay_reader_f(va_list ap)
{
	struct relay *relay = va_arg(ap, struct relay *);
	struct fiber *relay_f = va_arg(ap, struct fiber *);

	struct ibuf ibuf;
	struct ev_io io;
	coio_create(&io, relay->io.fd);
	ibuf_create(&ibuf, &cord()->slabc, 1024);
	try {
		while (!fiber_is_cancelled()) {
			struct xrow_header xrow;
			coio_read_xrow_timeout_xc(&io, &ibuf, &xrow,
						  TIMEOUT_PERIODS * relay_timeout);
			/* vclock is followed while decoding, zeroing it. */
			vclock_create(&relay->recv_vclock);
			xrow_decode_vclock_xc(&xrow, &relay->recv_vclock);
			fiber_cond_signal(&relay->reader_cond);
		}
	} catch (Exception *e) {
		if (diag_is_empty(&relay->diag)) {
			/* Don't override existing error. */
			diag_move(diag_get(), &relay->diag);
			fiber_cancel(relay_f);
		} else if (!fiber_is_cancelled()) {
			/*
			 * There is an relay error and this fiber
			 * fiber has another, log it.
			 */
			e->log();
		}
	}
	ibuf_destroy(&ibuf);
	return 0;
}

/**
 * A libev callback invoked when a relay client socket is ready
 * for read. This currently only happens when the client closes
 * its socket, and we get an EOF.
 */
static int
relay_subscribe_f(va_list ap)
{
	struct relay *relay = va_arg(ap, struct relay *);
	struct recovery *r = relay->r;

	coio_enable();
	cbus_endpoint_create(&relay->endpoint, cord_name(cord()),
			     fiber_schedule_cb, fiber());
	cbus_pair("tx", cord_name(cord()), &relay->tx_pipe, &relay->relay_pipe,
		  NULL, NULL, cbus_process);
	/* Setup garbage collection trigger. */
	struct trigger on_close_log = {
		RLIST_LINK_INITIALIZER, relay_on_close_log_f, relay, NULL
	};
	trigger_add(&r->on_close_log, &on_close_log);
	wal_set_watcher(&relay->wal_watcher, cord_name(cord()),
			relay_process_wal_event, cbus_process);

	relay_set_cord_name(relay->io.fd);

	char name[FIBER_NAME_MAX];
	snprintf(name, sizeof(name), "%s:%s", fiber()->name, "reader");
	struct fiber *reader = fiber_new_xc(name, relay_reader_f);
	fiber_set_joinable(reader, true);
	fiber_start(reader, relay, fiber());

	while (!fiber_is_cancelled()) {
		double timeout = relay_timeout;
		struct errinj *inj = errinj(ERRINJ_RELAY_REPORT_INTERVAL,
					    ERRINJ_DOUBLE);
		if (inj != NULL && inj->dparam != 0)
			timeout = inj->dparam;

		fiber_cond_wait_timeout(&relay->reader_cond, timeout);
		/*
		 * The fiber can be woken by IO cancel, by a timeout of
		 * status messaging or by an acknowledge to status message.
		 * Handle cbus messages first.
		 */
		cbus_process(&relay->endpoint);
		/*
		 * Check that the vclock has been updated and the previous
		 * status message is delivered
		 */
		if (relay->status_msg.msg.route != NULL)
			continue;
		struct vclock *send_vclock;
		if (relay->version_id < version_id(1, 7, 4))
			send_vclock = &r->vclock;
		else
			send_vclock = &relay->recv_vclock;
		if (vclock_sum(&relay->status_msg.vclock) ==
		    vclock_sum(send_vclock))
			continue;
		static const struct cmsg_hop route[] = {
			{tx_status_update, NULL}
		};
		cmsg_init(&relay->status_msg.msg, route);
		vclock_copy(&relay->status_msg.vclock, send_vclock);
		relay->status_msg.relay = relay;
		cpipe_push(&relay->tx_pipe, &relay->status_msg.msg);
	}

	say_crit("exiting the relay loop");
	if (!fiber_is_dead(reader))
		fiber_cancel(reader);
	fiber_join(reader);
	relay->exiting = true;
	trigger_clear(&on_close_log);
	wal_clear_watcher(&relay->wal_watcher, cbus_process);
	cbus_unpair(&relay->tx_pipe, &relay->relay_pipe,
		    NULL, NULL, cbus_process);
	cbus_endpoint_destroy(&relay->endpoint, cbus_process);
	if (!diag_is_empty(&relay->diag)) {
		/* An error has occured while ACKs of xlog reading */
		diag_move(&relay->diag, diag_get());
	}
	struct errinj *inj = errinj(ERRINJ_RELAY_EXIT_DELAY, ERRINJ_DOUBLE);
	if (inj != NULL && inj->dparam > 0)
		fiber_sleep(inj->dparam);

	return diag_is_empty(diag_get()) ? 0: -1;
}

/** Replication acceptor fiber handler. */
void
relay_subscribe(int fd, uint64_t sync, struct replica *replica,
		struct vclock *replica_clock, uint32_t replica_version_id)
{
	assert(replica->id != REPLICA_ID_NIL);
	/* Don't allow multiple relays for the same replica */
	if (replica->relay != NULL) {
		tnt_raise(ClientError, ER_CFG, "replication",
			  "duplicate connection with the same replica UUID");
	}

	/*
	 * Register the replica with the garbage collector
	 * unless it has already been registered by initial
	 * join.
	 */
	if (replica->gc == NULL) {
		replica->gc = gc_consumer_register(
			tt_sprintf("replica %s", tt_uuid_str(&replica->uuid)),
			vclock_sum(replica_clock));
		if (replica->gc == NULL)
			diag_raise();
	}

	struct relay relay;
	relay_create(&relay, fd, sync, relay_add_row, relay_begin_tx,
		     relay_send_tx);
	auto relay_guard = make_scoped_guard([&] { relay_destroy(&relay); });
	relay.r = recovery_new(cfg_gets("wal_dir"),
			       cfg_geti("force_recovery"),
			       replica_clock);
	vclock_copy(&relay.tx.vclock, replica_clock);
	relay.version_id = replica_version_id;
	relay.replica = replica;
	replica_set_relay(replica, &relay);

	int rc = cord_costart(&relay.cord, tt_sprintf("relay_%p", &relay),
			      relay_subscribe_f, &relay);
	if (rc == 0)
		rc = cord_cojoin(&relay.cord);

	replica_clear_relay(replica);
	if (rc != 0)
		diag_raise();
}

static void
relay_send(struct relay *relay, struct xrow_header *packet)
{
	packet->sync = relay->sync;
	coio_write_xrow(&relay->io, packet);
	fiber_gc();

	struct errinj *inj = errinj(ERRINJ_RELAY_TIMEOUT, ERRINJ_DOUBLE);
	if (inj != NULL && inj->dparam > 0)
		fiber_sleep(inj->dparam);
}

static void
relay_send_initial_join_row(struct xstream *stream, struct xrow_header *row)
{
	struct relay *relay = container_of(stream, struct relay, stream);
	relay_send(relay, row);
}

/** Send a single row to the client. */
static void
relay_send_row(struct xstream *stream, struct xrow_header *packet)
{
	struct relay *relay = container_of(stream, struct relay, stream);
	assert(iproto_type_is_dml(packet->type));
	/*
	 * We're feeding a WAL, thus responding to SUBSCRIBE request.
	 * In that case, only send a row if it is not from the same replica
	 * (i.e. don't send replica's own rows back).
	 */
	if (relay->replica == NULL ||
	    packet->replica_id != relay->replica->id) {
		relay_send(relay, packet);
	}
}

/** Add a single row to the current xlog transaction. */
static void
relay_add_row(struct xstream *stream, struct xrow_header *packet)
{
	struct relay *relay = container_of(stream, struct relay, stream);
	assert(iproto_type_is_dml(packet->type));
	struct xlog_tx_cursor *cursor = &relay->r->cursor.tx_cursor;
	off_t row_begin = relay->prev_row_end;
	off_t row_end = xlog_tx_cursor_pos(cursor);
	relay->prev_row_end = row_end;
	/*
	 * We're feeding a WAL, thus responding to SUBSCRIBE request.
	 * In that case, only send a row if it is not from the same replica
	 * (i.e. don't send replica's own rows back).
	 */
	if (relay->replica != NULL && relay->replica->id == packet->replica_id)
		return;
	/*
	 * Copy the original row to avoid its currupting. The
	 * original row is stored on a stack in recover_xlog() and
	 * is not valid after relay_add_row() finished.
	 */
	struct xrow_header *copy = xrow_batch_new_row(&relay->batch);
	*copy = *packet;
	relay->batch.bsize += row_end - row_begin;
}

/** Clear a batch to read a next transaction. */
static void
relay_begin_tx(struct xstream *stream)
{
	struct relay *relay = container_of(stream, struct relay, stream);
	xrow_batch_reset(&relay->batch);
	relay->prev_row_end = 0;
}

/** Send accumulated rows as a monolite iproto packet. */
static void
relay_send_tx(struct xstream *stream)
{
	struct relay *relay = container_of(stream, struct relay, stream);
	if(relay->batch.count == 0)
		return;
	xrow_batch_set_sync(&relay->batch, relay->sync);
	coio_write_xrow_batch(&relay->io, &relay->batch);
}
