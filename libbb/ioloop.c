/* vi: set sw=4 ts=4: */
/*
 * Copyright (C) 2026 Denys Vlasenko <vda.linux@googlemail.com>
 *
 * Licensed under GPLv2, see file LICENSE in this source tree.
 */
//kbuild:lib-$(CONFIG_TELNETD) += ioloop.o

#include "libbb.h"

#define DEBUG 0

#if DEBUG
# define dbg(...) bb_error_msg(__VA_ARGS__)
static char *bin_to_hex(const void *hash_value, unsigned hash_length)
{
	/* xzalloc zero-terminates */
	char *hex_value = xzalloc((hash_length * 2) + 1);
	bin2hex(hex_value, (char*)hash_value, hash_length);
	return auto_string(hex_value);
}
#else
# define dbg(...) ((void)0)
#endif

void FAST_FUNC ioloop_insert_conn(ioloop_state_t *io, connection_t *conn)
{
	conn->io = io;
	conn->next = io->conns;
	io->conns = conn;
}

void FAST_FUNC ioloop_remove_conn(ioloop_state_t *io, connection_t *conn)
{
	connection_t **pp = &io->conns;
	while (*pp) {
		if (*pp == conn) {
			*pp = conn->next;
			return;
		}
		pp = &(*pp)->next;
	}
}

void FAST_FUNC conn_close_fds(connection_t *conn)
{
	if (conn->read_fd >= 0)
		close(conn->read_fd);
	if (conn->write_fd >= 0 && conn->write_fd != conn->read_fd)
		close(conn->write_fd);
	conn->write_fd = -1;
	conn->read_fd = -1;
}

void FAST_FUNC conn_close_fds_remove_and_free(connection_t *conn)
{
	conn_close_fds(conn);
	ioloop_remove_conn(conn->io, conn);
	free_connection(conn);
}

#ifdef UNUSED
void FAST_FUNC ioloop_close_fd_in_all_conns(ioloop_state_t *io, int fd)
{
	connection_t *conn;

	close(fd);
	conn = io->conns;
	while (conn) {
		if (conn->read_fd == fd)
			conn->read_fd = -1;
		if (conn->write_fd == fd)
			conn->write_fd = -1;
		conn = conn->next;
	}
}
#endif

// have_data_to_write() - Do we have data to write?
// May return error if knows that write side is closed, even if it has free buffer space.
// > 0: Has data to write (or can generate such data)
//    In this case, write_fd must be valid! (it's a bug if it is < 0, can crash)
// 0: No data to write currently
// < 0: error, I probably freed myself (do not use my structure in this iteration,
// on next iteration, if I indeed freed myself, you won't find me in the list).
//
// have_buffer_to_read_into() - Is there a buffer to read into?
// May return error if knows that write side is closed, even if it has free buffer space.
// > 0: Healthy, has buffer space, please poll read_fd
//    In this case, read_fd must be valid! (it's a bug if it is < 0)
// 0: One of:
//    Buffer is full (hopefully write() will free some)
//    Got error/EOF, want to drain write buffer first
// < 0: Error/EOF, I probably freed myself (do not use my structure)
//
// Essentially: add your "this connection is dead, close+ioloop_remove_conn(this)+free(this)" code
// to either of the above two functions.
// Either would work equally well from ioloop POV.
//
// write() - Perform write
// Even if poll shows that write_fd become ready, the method will not be called if conn->write_fd become < 0.
// However, changing it to another fd >= 0 is a bug.
// >= 0: No error (ioloop() does not handle 0 specially)
// < 0: Error occurred, I probably freed myself (do not use my structure)
//
// Error returns from read() and write() will skip the opposite op.
// (Which one happens first depends on ioloop_run() code, as I write this, write()
// is done first - therefore read() can't skip write()).
// IOW: if you "remove(this)" in either of them you MUST return < 0.
//
// Write function, if it performs some processing, needs to account for the possibility
// that any "incomplete data, can't write, wait for more" logic should also check whether
// "more" is even possible: your other code may know that the read side is at EOF,
// and arrange for it to not be polled. Thus a hang: read not reading, write waits for more date.
//
// read() - Perform read
// Will not be called if read_fd become < 0.
// >= 0: No error (0 is EOF, ioloop() does not handle 0 specially)
// < 0: Error occurred, I probably freed myself (do not use my structure)
//
// Putting "this connection is dead, close+remove+free" code into read function
// is often inconvenient: if you got EOF/error on read, you still want to poll write side
// and try to write out the buffered data to it. Which means read() can't "remove+free".
// Instead, you can remember EOF/error and make future have_buffer_to_read_into() respond 0.
// One way is to close (if possible) read_fd, set it to -1 and use as a flag.
//
// If need to support one-sided close, such as when HTTP/1.x client sends us
// "GET / HTTP/1.1\r\n\r\n" and closes its writing side with shutdown(SHUT_WR),
// the idiom is that when read() sees EOF, it sets conn->read_fd to -1
// and subsequently have_buffer_to_read_into() always return 0 (no more attempts to read);
// write() flushes all remaining data to conn->write_fd and then signals EOF
// to write_fd: shutdown(SHUT_WR) for sockets, close() for pipes
// (how to do this for ptys!?).
// After this, have_data_to_write() can return 0 if fd has to stay open (socket)
// or can return -1 and free itself if fd is closed.

static ALWAYS_INLINE int have_data_to_write(connection_t *conn)
{
	return conn->have_data_to_write(conn);
}
static ALWAYS_INLINE int have_buffer_to_read_into(connection_t *conn)
{
	return conn->have_buffer_to_read_into(conn);
}
static ALWAYS_INLINE int write_from_buf(connection_t *conn)
{
	return conn->write(conn);
}
static ALWAYS_INLINE int read_to_buf(connection_t *conn)
{
	return conn->read(conn);
}

int FAST_FUNC ioloop_run(ioloop_state_t *io)
{
	connection_t *conn;
	unsigned poll_timeout_us;
	int maxfd;
	int count;
	struct timeval tv;
	struct timeval *tv_ptr;
	fd_set rdfdset, wrfdset;

	io->current_iteration_timeout = io->max_timeout ? io->max_timeout : UINT_MAX;
//bb_error_msg("io->current_iteration_timeout:%d", io->current_iteration_timeout);
 again:
	FD_ZERO(&rdfdset);
	FD_ZERO(&wrfdset);

	conn = io->conns;
	if (!conn)
		return IOLOOP_NO_CONNS;

	maxfd = -1;
	while (conn) {
		connection_t *next;
		int rcw, rcr;

		next = conn->next; /* in case conn is freed */
		rcw = have_data_to_write(conn);
		if (rcw < 0) {
			/* often indicates that conn is gone (freed), do not use it anymore */
			goto next;
		}
		/* Do not add conn->write_fd to waiting set yet,
		 * the *reader* may decide to abort (return rcr < 0)!
		 * Check it first:
		 */
		rcr = have_buffer_to_read_into(conn);
		if (rcr < 0)
			goto next;
		if (rcw > 0) {
			FD_SET(conn->write_fd, &wrfdset);
//FIXME: what if fd > 1023
			if (conn->write_fd > maxfd)
				maxfd = conn->write_fd;
		}
		if (rcr > 0) {
			FD_SET(conn->read_fd, &rdfdset);
			if (conn->read_fd > maxfd)
				maxfd = conn->read_fd;
		}
 next:
		conn = next;
	}
	if (!io->conns)
		return IOLOOP_NO_CONNS; /* all conns removed themselves before we reached polling */

	poll_timeout_us = io->current_iteration_timeout;
	/* May be useful to know after loop exit: */
	io->last_timeout = poll_timeout_us;
	dbg("poll_timeout_us:%d", poll_timeout_us);

	tv_ptr = NULL; /* infinite timeout */
	if (poll_timeout_us != UINT_MAX) {
		tv.tv_sec = poll_timeout_us / 1000000;
		tv.tv_usec = poll_timeout_us % 1000000;
		tv_ptr = &tv;
	}
	maxfd++;
	count = select(maxfd, maxfd ? &rdfdset : NULL, maxfd ? &wrfdset : NULL, NULL, tv_ptr);
	dbg("select:%d", count);

	/* Any function in the loop can change it, modifying next select() timeout */
	io->current_iteration_timeout = io->max_timeout ? io->max_timeout : UINT_MAX;
	dbg("io->current_iteration_timeout:%d", io->current_iteration_timeout);

	if (count <= 0) {
		/* 0: timeout */
		if (count == 0 && (io->flags & IOLOOP_FLAG_EXIT_IF_TIMEOUT))
			return IOLOOP_TIMEOUT;
		/* < 0: EINTR or ENOMEM */
		if (count < 0 && (io->flags & IOLOOP_FLAG_EXIT_IF_EINTR) && errno == EINTR)
			return IOLOOP_EINTR;
		goto again;
	}

	conn = io->conns;
	while (conn) {
		connection_t *next = conn->next; /* in case conn freed */
		int rcw;

		/* We do check for valid fd >=0: this means that conns
		 * may prevent I/O of other conns by setting fds to -1.
		 */
		dbg("conn->write_fd:%d ?", conn->write_fd);
		if (conn->write_fd >= 0 && FD_ISSET(conn->write_fd, &wrfdset)) {
			dbg("conn->write_fd:%d ready", conn->write_fd);
			rcw = write_from_buf(conn);
			if (rcw < 0) {
				/* often indicates that conn is gone (freed), do not use it anymore */
				goto next1;
			}
		}
		dbg("conn->read_fd:%d ?", conn->read_fd);
		if (conn->read_fd >= 0 && FD_ISSET(conn->read_fd, &rdfdset)) {
			dbg("conn->read_fd:%d ready", conn->read_fd);
			read_to_buf(conn);
		}
 next1:
		conn = next;
	}
	goto again;
}
