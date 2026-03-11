/* vi: set sw=4 ts=4: */
/*
 * Simple telnet server
 * Bjorn Wesen, Axis Communications AB (bjornw@axis.com)
 *
 * Licensed under GPLv2 or later, see file LICENSE in this source tree.
 *
 * ---------------------------------------------------------------------------
 * (C) Copyright 2000, Axis Communications AB, LUND, SWEDEN
 ****************************************************************************
 *
 * The telnetd manpage says it all:
 *
 * Telnetd operates by allocating a pseudo-terminal device (see pty(4)) for
 * a client, then creating a login process which has the slave side of the
 * pseudo-terminal as stdin, stdout, and stderr. Telnetd manipulates the
 * master side of the pseudo-terminal, implementing the telnet protocol and
 * passing characters between the remote client and the login process.
 *
 * Vladimir Oleynik <dzo@simtreas.ru> 2001
 * Set process group corrections, initial busybox port
 */
//config:config TELNETD
//config:	bool "telnetd (13 kb)"
//config:	default y
//config:	select FEATURE_SYSLOG
//config:	help
//config:	A daemon for the TELNET protocol, allowing you to log onto the host
//config:	running the daemon. Please keep in mind that the TELNET protocol
//config:	sends passwords in plain text. If you can't afford the space for an
//config:	SSH daemon and you trust your network, you may say 'y' here. As a
//config:	more secure alternative, you should seriously consider installing the
//config:	very small Dropbear SSH daemon instead:
//config:		http://matt.ucc.asn.au/dropbear/dropbear.html
//config:
//config:	Note that for busybox telnetd to work you need several things:
//config:	First of all, your kernel needs:
//config:		  CONFIG_UNIX98_PTYS=y
//config:
//config:	Next, you need a /dev/pts directory on your root filesystem:
//config:
//config:		  $ ls -ld /dev/pts
//config:		  drwxr-xr-x  2 root root 0 Sep 23 13:21 /dev/pts/
//config:
//config:	Next you need the pseudo terminal master multiplexer /dev/ptmx:
//config:
//config:		  $ ls -la /dev/ptmx
//config:		  crw-rw-rw-  1 root tty 5, 2 Sep 23 13:55 /dev/ptmx
//config:
//config:	Any /dev/ttyp[0-9]* files you may have can be removed.
//config:	Next, you need to mount the devpts filesystem on /dev/pts using:
//config:
//config:		  mount -t devpts devpts /dev/pts
//config:
//config:	You need to be sure that busybox has LOGIN and
//config:	FEATURE_SUID enabled. And finally, you should make
//config:	certain that busybox has been installed setuid root:
//config:
//config:		chown root.root /bin/busybox
//config:		chmod 4755 /bin/busybox
//config:
//config:	with all that done, telnetd _should_ work....
//config:
//config:config FEATURE_TELNETD_SELFTEST_DEBUG
//config:	bool "Include self-test (telnetd -@)"
//config:	default n
//config:	depends on TELNETD
//config:	help
//config:	  Include self-test code for pty/vhangup() behavior.
//config:	  Useful for development and validation on new platforms.
//config:
//config:config FEATURE_TELNETD_STANDALONE
//config:	bool "Support standalone telnetd (not inetd only)"
//config:	default y
//config:	depends on TELNETD
//config:	help
//config:	Selecting this will make telnetd able to run standalone.
//config:
//config:config FEATURE_TELNETD_PORT_DEFAULT
//config:	int "Default port"
//config:	default 23
//config:	range 1 65535
//config:	depends on FEATURE_TELNETD_STANDALONE
//config:
//config:config FEATURE_TELNETD_INETD_WAIT
//config:	bool "Support -w SEC option (inetd wait mode)"
//config:	default y
//config:	depends on FEATURE_TELNETD_STANDALONE
//config:	help
//config:	This option allows you to run telnetd in "inet wait" mode.
//config:	Example inetd.conf line (note "wait", not usual "nowait"):
//config:
//config:	telnet stream tcp wait root /bin/telnetd telnetd -w10
//config:
//config:	In this example, inetd passes _listening_ socket_ as fd 0
//config:	to telnetd when connection appears.
//config:	telnetd will wait for connections until all existing
//config:	connections are closed, and no new connections
//config:	appear during 10 seconds. Then it exits, and inetd continues
//config:	to listen for new connections.
//config:
//config:	This option is rarely used. "tcp nowait" is much more usual
//config:	way of running tcp services, including telnetd.
//config:	You most probably want to say N here.

//applet:IF_TELNETD(APPLET(telnetd, BB_DIR_USR_SBIN, BB_SUID_DROP))

//kbuild:lib-$(CONFIG_TELNETD) += telnetd.o

//usage:#define telnetd_trivial_usage
//usage:       "[OPTIONS]"
//usage:#define telnetd_full_usage "\n\n"
//usage:       "Handle incoming telnet connections"
//usage:	IF_NOT_FEATURE_TELNETD_STANDALONE(" via inetd") "\n"
//usage:     "\n	-l LOGIN	Exec LOGIN on connect (default /bin/login)"
//usage:     "\n	-f ISSUE_FILE	Display ISSUE_FILE instead of /etc/issue.net"
//usage:     "\n	-K		Close connection as soon as login exits"
//usage:     "\n			(normally wait until all programs close slave pty)"
//usage:	IF_FEATURE_TELNETD_STANDALONE(
//usage:     "\n	-p PORT		Port to listen on. Default "STR(CONFIG_FEATURE_TELNETD_PORT_DEFAULT)
//usage:     "\n	-b ADDR[:PORT]	Address to bind to"
//usage:     "\n	-F		Run in foreground"
//usage:     "\n	-i		Inetd mode"
//usage:	IF_FEATURE_TELNETD_INETD_WAIT(
//usage:     "\n	-w SEC		Inetd 'wait' mode, linger time SEC"
//usage:     "\n		inetd.conf line: 23 stream tcp wait root telnetd telnetd -w10"
//usage:     "\n	-S		Log to syslog (implied by -i or without -F and -w)"
//usage:	)
//usage:	)
//usage:     "\n	-v		Verbose"

#include "libbb.h"
#include "common_bufsiz.h"
#include <syslog.h>
#include <arpa/telnet.h>

#define DEBUG         0
#define DEBUG_CLOSING 0

#if DEBUG
# define dbg(...) bb_error_msg(__VA_ARGS__)
#else
# define dbg(...) ((void)0)
#endif
#if DEBUG_CLOSING
# define dbg_close(...) bb_error_msg(__VA_ARGS__)
#else
# define dbg_close(...) ((void)0)
#endif

#define SELFTEST_VHANGUP 1
#define SELFTEST_NET2PTY 1
#define SELFTEST_PTY2NET 1

#define SAVE_NET2PTY_EXPECTED 0
#define SAVE_NET2PTY_ACTUAL   0

#define SAVE_PTY2NET_EXPECTED 0
#define SAVE_PTY2NET_ACTUAL   0

/* Must match getopt32 string */
enum {
	OPT_WATCHCHILD = (1 << 2), /* -K */
	OPT_INETD      = (1 << 3) * ENABLE_FEATURE_TELNETD_STANDALONE, /* -i */
	OPT_PORT       = (1 << 4) * ENABLE_FEATURE_TELNETD_STANDALONE, /* -p PORT */
	OPT_FOREGROUND = (1 << 6) * ENABLE_FEATURE_TELNETD_STANDALONE, /* -F */
	OPT_SYSLOG     = (1 << 7) * ENABLE_FEATURE_TELNETD_INETD_WAIT, /* -S */
	OPT_WAIT       = (1 << 8) * ENABLE_FEATURE_TELNETD_INETD_WAIT, /* -w SEC */
};

/* Globals */
struct globals {
	ioloop_state_t io;
	const char *loginpath;
	const char *issuefile;
	unsigned verbose;
	IF_FEATURE_TELNETD_INETD_WAIT(unsigned sec_linger;)
} FIX_ALIASING;
#define G (*(struct globals*)bb_common_bufsiz1)
#define INIT_G() do { \
	setup_common_bufsiz(); \
	G.loginpath = "/bin/login"; \
	G.issuefile = "/etc/issue.net"; \
} while (0)

#define log1(...) do { if (G.verbose) bb_error_msg(__VA_ARGS__); } while (0)

static NOINLINE void fabricate_ctrl_D_on_pty(int fd)
{
	struct termios tio;
	// Master pty can see slave's termios - abuse that
	if (tcgetattr(fd, &tio) != 0) {
		dbg_close("%s: no termios(%d)", __func__, fd);
		return;
	}
	dbg_close("%s: termios(%d) ICANON:%d,VEOF:%02x", __func__,
		fd, !!(tio.c_lflag & ICANON), (uint8_t)tio.c_cc[VEOF]);
	if ((tio.c_lflag & ICANON) && tio.c_cc[VEOF] != 0) {
		// _Should_ look like EOF on input on the slave side...
		write(fd, &tio.c_cc[VEOF], 1); // usually ^D
		// One ^D often won't do. If a program was blocked on read already,
		// ^D makes _that_ read return:
		//  read(0, "some data", 64) = 9
		// but the program will likely read more
		// (it did not interpret above as EOF), block, and die:
		//  read(0, 0x123456, 64) = ...<blocked>... -1 EIO
		//  +++ killed by SIGHUP +++
		// Well, I'm not greedy...
		write(fd, &tio.c_cc[VEOF], 1); // here is another EOF for you
		// If this doesn't work, they can see 0x04 bytes. Tough cookies
	}
}

#define TELNET_CONNECTION \
	STRUCT_CONNECTION \
	pid_t shell_pid;

typedef struct telnet_conn {
	TELNET_CONNECTION
} telnet_conn_t;

struct net_to_pty;

typedef struct pty_to_net {
	TELNET_CONNECTION
	struct net_to_pty *sibling; /* must be directly after TELNET_CONNECTION */
	int rdidx, wridx, size;
	unsigned deadline_us;
	unsigned eio_count;
//TODO: these two can be unified into one:
	smallint respond_to_AYT;
	char extra_byte_to_write;
	/* circular buffer follows */
#define BUF2NET(ts) ((unsigned char*)(ts + 1))
} pty_to_net_t ALIGN8;
/* The buffer is directly after malloced memory, aligned to 64 bits */
enum { TO_NET_BUFSIZE = 2 * 1024 };
enum { TO_NET_BUFMASK = TO_NET_BUFSIZE - 1 };
#define BUF2NET_INC(n, inc) ((n) = ((n) + (inc)) & TO_NET_BUFMASK)

typedef struct net_to_pty {
	TELNET_CONNECTION
	pty_to_net_t *sibling; /* must be directly after TELNET_CONNECTION */
	int rdidx, wridx, size;
	unsigned deadline_us;
	smallint skip_LF_or_NUL;
	/* circular buffer follows */
#define BUF2PTY(ts) ((unsigned char*)(ts + 1))
} net_to_pty_t ALIGN8;
/* The buffer is directly after malloced memory, aligned to 64 bits */
enum { TO_PTY_BUFSIZE = 2 * 1024 };
enum { TO_PTY_BUFMASK = TO_PTY_BUFSIZE - 1 };
#define BUF2PTY_INC(n, inc) ((n) = ((n) + (inc)) & TO_PTY_BUFMASK)
#define BUF2PTY_DEC(n, dec) ((n) = ((n) - (dec)) & TO_PTY_BUFMASK)

/* The mutual pipes are linked by to_pty, to_net.
 * The rules:
 * = always test for !NULL before use.
 * = if you are freeing one of the structs, set
 * "me->sibling->sibling = NULL".
 * Use one of these helpers:
 */
static void remove_and_free_to_pty(net_to_pty_t *ts)
{
	dbg_close("remove_and_free:%p rd:%d wr:%d sibling:%p",
			ts, ts->read_fd, ts->write_fd, ts->sibling);
	if (ts->sibling) {
		/* "you have no sibling now" */
		ts->sibling->sibling = NULL;
	}
	conn_close_fds_remove_and_free((void*)ts);
}
static void ALWAYS_INLINE remove_and_free_to_net(pty_to_net_t *ts)
{
	/* Works because ->sibling have the same offset */
	remove_and_free_to_pty((void*)ts);
}

// Theory of operation
// (AKA "when should I close fds? when should I detach from ioloop?").
// The fds are named read_fd and write_fd, but for clarity let's call them netfd and ptyfd.
// net_to_pty::have_data_to_write
// net_to_pty::have_buffer_to_read_into
//  if ptyfd < 0: //sibling told us ptyfd is down?
//    if sibling && sibling->netfd >= 0: netfd = -1; //do not close netfd, sibling uses it (if sibling exists)!
//    close_and_detach;
//  if netfd < 0: return 0;
// net_to_pty::write
//  if ptyfd write detects error (not EAGAIN):
//    //if (sibling):
//    //  sibling->ptyfd = -1; // let sibling know ptyfd is closed
//    //  if sibling->netfd >= 0: netfd = -1; // do not close netfd, sibling uses it!
//    //close_and_detach;
//    let pty_to_net detect this (ignore the error)
// net_to_pty::read
//  if netfd read detects EOF or error (there is no way to tell pty the difference):
//    if no sibling: close(netfd);
//    netfd = -1; //no longer try to read
//    return 0; //but do not detach yet

static unsigned char read_byte_unescaping_IAC(int *iac_cnt, unsigned char **pp)
{
	unsigned char c = *(*pp)++;
	if (c == IAC && **pp == IAC) {
		(*iac_cnt)++;
		(*pp)++;  /* Skip the second IAC in IAC IAC sequence */
	}
	return c;
}

static int net_to_pty__have_data_to_write(void *this)
{
	//connection_t *conn = this;
	net_to_pty_t *ts = this;
	unsigned char *buf;
	int size, increment;

	if (ts->write_fd < 0) /* did slave close its pty? */
		return 0;
 again:
	size = ts->size;
	if (size == 0)
		return 0;

	buf = BUF2PTY(ts) + ts->wridx;
	if (ts->skip_LF_or_NUL) {
		/* last char we wrote was '\r' */
		ts->skip_LF_or_NUL = 0;
		/* we have the next one: examine */
		if (buf[0] == '\n' || buf[0] == '\0') {
			increment = 1; /* drop it now */
 inc:
			BUF2PTY_INC(ts->wridx, increment);
			ts->size -= increment;
			goto again;
		}
		/* else: not '\r\n' or '\rNUL' */
	}
	if (buf[0] != IAC) /* very likely */
		return size;
	if (size == 1) { /* we only have one char: IAC */
 cant_write:
		if (ts->read_fd < 0)
			ts->size = 0; /* EOF was seen: ignore incomplete IAC, prevent infinite loop */
		//bb_error_msg("cant_write");
		return 0; /* "can't write" */
	}

	/* size is >= 2 and buf[0] is IAC */
	//dbg_iac("size:%d %02x %02x", size, buf[0], buf[1]);

	/* Cannot process IACs which wrap around buffer's end.
	 * Worst case: IAC SB NAWS with all bytes IAC-escaped = 13 bytes */
	while (ts->wridx > TO_PTY_BUFSIZE - 14) {
		uint64_t v64;
		if (ts->wridx < ts->rdidx) {
			/* |.......WRxRD.| */
			/* Buffer is not wrapped */
			break;
		}
		/* Possible situations: */
		/* |xRD.......WRx| wrapped */
		/* |xxxxxxxxRDWRx| wrapped and full! rdidx = wridx */
		/* Rotate entire buffer 8 bytes back */
		v64 = *(uint64_t*)BUF2PTY(ts);
		memmove(BUF2PTY(ts), BUF2PTY(ts) + 8, TO_PTY_BUFSIZE - 8);
		*(uint64_t*)(BUF2PTY(ts) + TO_PTY_BUFSIZE - 8) = v64;
		ts->wridx -= 8; /* can't underflow */
		buf -= 8;
		BUF2PTY_DEC(ts->rdidx, 8); /* can underflow, use DEC() */
	}

	if (buf[1] == IAC) /* IAC-IAC: we have something to write */
		return size;

	if (buf[1] >= 240 && buf[1] <= 249) {
		/* 2-byte commands (240..250 and 255):
		 * IAC IAC (255) Literal 255.
		 * IAC SE  (240) End of subnegotiation. Treated as NOP.
		 * IAC NOP (241) NOP.
		 * IAC BRK (243) Break. Like serial line break. TODO via tcsendbreak()?
		 * IAC AYT (246) Are you there.
		 *  These don't look useful:
		 * IAC DM  (242) Data mark. What is this?
		 * IAC IP  (244) Suspend, interrupt or abort the process. (Ancient cousin of ^C).
		 * IAC AO  (245) Abort output. "You can continue running, but do not send me the output".
		 * IAC EC  (247) Erase character. The receiver should delete the last received char.
		 * IAC EL  (248) Erase line. The receiver should delete everything up to last newline.
		 * IAC GA  (249) Go ahead. For half-duplex lines: "now you talk".
		 */
		if (buf[1] == AYT && ts->sibling) /* notify other pipe that AYT was seen */
			ts->sibling->respond_to_AYT = 1;
		/* NOP (241). Ignore (putty keepalive, etc) */
		/* All other 2-byte commands also treated as NOPs here */
		increment = 2;
		goto inc;
	}
	if (size <= 2) {
		/* only 2 bytes of the longer IAC is in the buffer */
		goto cant_write; /* incomplete, can't process */
	}
	if (buf[1] == SB) {
		if (buf[2] == TELOPT_NAWS) {
			// IAC,SB,TELOPT_NAWS,<4 bytes possibly IAC-escaped>,IAC,SE
			struct {
				struct winsize ws;
				int iac_cnt;
			} s;
			unsigned char *p;
			unsigned char byte45, byte67, byte89;

			// The usual: IAC,SB,TELOPT_NAWS,w,w,h,h (IAC,SE later): 7 + 2 = 9 bytes
			// 255*HH:    IAC,SB,TELOPT_NAWS,0,255,255,h,h: 8 bytes (10 with IAC,SE)
			// The worst: IAC,SB,TELOPT_NAWS,w,w,w,w,h,h,h,h (four IACs): 11 bytes (not counting IAC,SE)
			// We can't simply check for size < 11:
			// will mishandle IAC,SB,TELOPT_NAWS,w,w,h,h,IAC,SE,'A' (10 bytes)
			// the write of 'A' (ordinary visible char) can be delayed!
			// Also mishandles 255*HH case (10 bytes) by delaying its processing
			// until at least one more byte after it is received and size becomes >= 11.
			if (size < 9) // postpone parsing until have 9+ bytes
				goto cant_write;
			memset(&s, 0, sizeof(s)); // pixel sizes are set to 0
			//s.iac_cnt = 0; //done
			p = buf + 3;
			byte45 = read_byte_unescaping_IAC(&s.iac_cnt, &p);
			byte67 = read_byte_unescaping_IAC(&s.iac_cnt, &p);
			byte89 = read_byte_unescaping_IAC(&s.iac_cnt, &p); // fetches at most byte#9 - allowed by size
			// If any one of these is IAC, the NAWS seq must be at least 10 bytes.
			// IOW: it can't be case 'A' above. *Can* postpone if size == 10!
			if (size < 9 + s.iac_cnt) // if IAC was seen: postpone parsing until have 10+ bytes
				goto cant_write;  // if 2+ IACs seen: postpone parsing until have 11+ bytes
			s.ws.ws_col = (byte45 << 8) | byte67;
			s.ws.ws_row = (byte89 << 8) | read_byte_unescaping_IAC(&s.iac_cnt, &p); // fetches _at most_ byte#11 (if four IACs)

			if (s.ws.ws_col != 0 && s.ws.ws_row != 0) // don't provoke bugs elsewhere with "zero-sized screen"
				ioctl(ts->write_fd, TIOCSWINSZ, (char *)&s.ws);
			log1("pfd:%d window size:%dx%d", ts->write_fd, s.ws.ws_row, s.ws.ws_col);
			increment = p - buf;
			goto inc;
			// trailing IAC,SE will be eaten separately, as 2-byte NOP
		}
//fixme: skip them correctly
		/* else: other subnegs not supported yet */
	}

	/* Assume it is a 3-byte WILL/WONT/DO/DONT 251..254 command and skip it */
	dbg("ignoring IAC,%02x,%02x", buf[1], buf[2]);
	increment = 3;
	goto inc;
}

/* Write some buf data to pty, processing IACs.
 * Update wridx and size. Return < 0 on error.
 */
static int net_to_pty__write(void *this)
{
	//connection_t *conn = this;
	net_to_pty_t *ts = this;
	unsigned wr;
	ssize_t rc;
	unsigned char *buf;
	unsigned char *found;

	if (ts->write_fd < 0) /* did slave close its pty? */
		return 0;

	buf = BUF2PTY(ts) + ts->wridx;
	wr = MIN(TO_PTY_BUFSIZE - ts->wridx, ts->size);
	/* wr is at least 1 here */

	found = memchr(buf, IAC, wr);
	if (found == buf) {
		/* The first char is IAC.
		 * have_data_to_write() ensures we are only called this way
		 * if there are two IACs.
		 * It also ensures the buffer is not wrapping within 7 chars.
		 * Write one IAC. If that works, skip both.
		 */
//TODO: advance ptr by one, and memchr up to _next_ IAC (if any):
//this would write more data
		rc = safe_write(ts->write_fd, buf, 1);
		if (rc > 0) {
			//dbg_iac("wrote: IAC");
			ts->wridx += 2; /* this can't overflow */
			ts->size -= 2;
		}
		/* Leave it to pty2net side to deal with closing pty on errors
		 * (it'll see them when it tries to read pty).
		 */
		return 0; /* "didn't write anything" */
	}
	/* No leading IACs: found != buf.
	 * IOW: there is a "prefix" of non-IAC chars.
	 * Write only them, and return.
	 */
	if (found)
		wr = found - buf;

	/* We map \r\n ==> \r for pragmatic reasons:
	 * many client implementations send \r\n when
	 * the user hits the CarriageReturn key.
	 * See RFC 1123 3.3.1 Telnet End-of-Line Convention.
	 */
	found = memchr(buf, '\r', wr);
	if (found)
		wr = found - buf + 1; /* write up to and including \r */
	rc = safe_write(ts->write_fd, buf, wr);
	if (rc <= 0)
		return 0; /* "didn't write anything" */
	//dbg_iac("wrote: %s", bin_to_hex(buf, rc));
	/* check is needed: imagine that write was _short_ */
	if (buf[rc - 1] == '\r') /* last written byte was CR? */
		ts->skip_LF_or_NUL = 1;

	BUF2PTY_INC(ts->wridx, rc);
	ts->size -= rc;
	if (ts->size == 0) { /* very typical */
		/* Avoid buffer wrapping most of the time (-> have bigger writes) */
		//dbg_buffer("zero size");
		ts->rdidx = 0;
		ts->wridx = 0;
	}
	return rc;
}

/* Check if buffer has space to read into */
static int net_to_pty__have_buffer_to_read_into(void *this)
{
	//connection_t *conn = this;
	net_to_pty_t *ts = this;

	/* the sibling may have closed our ptyfd after getting 10 EIO's */
	if (ts->write_fd < 0
	 || ts->shell_pid < 0 /* -K option, and our pty's pid has exited */
	) {
		if (ts->sibling) {
			/* do not close our netfd, it's in use by sibling */
			ts->read_fd = -1;
		}
		dbg_close("%s:%d: remove_and_free_to_pty:%p wr:%d pid:%d",
				__func__, __LINE__, ts, ts->write_fd, ts->shell_pid);
		remove_and_free_to_pty(ts);
		return -1; /* "don't use me anymore, I'm gone" */
	}

	if (ts->read_fd < 0) { /* we've seen EOF/error on netfd */
		dbg_close("EOF on netfd was seen, ts->size:%d", ts->size);
		if (ts->size == 0) { /* we wrote everything */
			unsigned rem;
			/* close ptyfd, but after a 20 ms pause */

			/* pty has a design problem: close(master_ptyfd)
			 * is defined as causing hangup on slave pty.
			 * Which discards all currently buffered unread data
			 * (in our case, all data we just wrote).
			 * usleep(20000) is a crude solution.
			 * A better one (does not block other conns):
			 */
			dbg_close("going to close ptyfd:%d deadline_us:%u",
					ts->write_fd, ts->deadline_us);
			if (!ts->deadline_us) {
				fabricate_ctrl_D_on_pty(ts->write_fd);
				ts->deadline_us = (monotonic_us() + 20000) | 1;
				rem = 20000;
			} else {
				rem = ts->deadline_us - monotonic_us();
			}
			dbg_close("until ptyfd_close:%d", rem);
			if (rem <= 20000) {
				ioloop_decrease_current_timeout(ts->io, rem);
				return 0; /* "do not read" */
			}
			/* rem undeflowed (is "negative"): we waited at least 1000us */
			dbg_close("EOF on netfd, closing ptyfd:%d", ts->write_fd);
			if (ts->sibling)
				ts->sibling->read_fd = -1;
			remove_and_free_to_pty(ts); /* closes ts->write_fd (ptyfd) */
			return -1; /* "don't use me anymore, I'm gone" */
		}
		return 0; /* "don't want to read" */
	}

	return ts->size < TO_PTY_BUFSIZE;
}

/* Read from socket to buffer, stripping trailing NUL if present */
static int net_to_pty__read(void *this)
{
	net_to_pty_t *ts = this;
	ssize_t count;
	int avail;

	/* Calculate available space and contiguous segment */
	avail = TO_PTY_BUFSIZE - ts->size;
	//if (avail <= 0)
	//	return 0; /* buffer full */
	// ioloop logic ensures this is false

	/* Read into contiguous segment from rdidx */
	count = MIN(TO_PTY_BUFSIZE - ts->rdidx, avail);
	count = safe_read(ts->read_fd, BUF2PTY(ts) + ts->rdidx, count);
	if (count <= 0) {
		dbg_close("EOF/error on netfd:%d sibling:%p", ts->read_fd, ts->sibling);
		/* There is no way to signal to pty that input is in "error state",
		 * therefore treat both EOF and error the same.
		 */
		if (!ts->sibling)
			close(ts->read_fd); /* close netfd if not in use by sibling */
		ts->read_fd = -1;
		return 0; /* "didn't read anything" */
	}

	ts->size += count;
	BUF2PTY_INC(ts->rdidx, count);

	return count;
}

static net_to_pty_t *new_net_to_pty(int from, int to)
{
	net_to_pty_t *this = xzalloc(sizeof(*this) + TO_PTY_BUFSIZE);
	this->have_buffer_to_read_into = net_to_pty__have_buffer_to_read_into;
	this->have_data_to_write       = net_to_pty__have_data_to_write;
	this->read                     = net_to_pty__read;
	this->write                    = net_to_pty__write;
	this->read_fd = from;
	this->write_fd = to;
	/* indexes and size are all 0 */
	return this;
}

static int pty_to_net__have_buffer_to_read_into(void *this)
{
	//connection_t *conn = this;
	pty_to_net_t *ts = this;

	if (ts->read_fd < 0 /* we saw fatal read error before */
	 || ts->shell_pid < 0 /* -K option, and our pty's pid has exited */
	) {
		if (ts->size == 0) { /* output flushed? */
			remove_and_free_to_net(ts);
			return -1; /* "don't use me anymore, I'm gone" */
		}
		return 0; /* "don't read" */
	}
	if (ts->eio_count != 0) {
		unsigned rem;

		//if (ts->eio_count >= 10)
		//	return 0; /* "do not read" */
		//^^^ can't reach, caught by "ts->read_fd < 0" above.

		/* last read was EIO. Still waiting before retry? */
		rem = ts->deadline_us - monotonic_us();
		if (rem <= 1000) {
			/* tell io loop that poll timeout should now be "rem" us, not infinity */
			ioloop_decrease_current_timeout(ts->io, rem);
			return 0; /* "do not read me (yet)" */
		}
		/* else: rem underflowed: 1000us passed, can try reading again */
	}
	return ts->size < TO_NET_BUFSIZE;
}

/* Read from pty to buffer, handling vhangup() EIO retries */
static int pty_to_net__read(void *this)
{
	//connection_t *conn = this;
	pty_to_net_t *ts = this;
	ssize_t count;
	int avail;

	/* Calculate available space and contiguous segment */
	avail = TO_NET_BUFSIZE - ts->size;
	//if (avail <= 0)
	//	return 0; /* buffer full */
	// io loop logic ensures this is false

	/* Read into contiguous segment from rdidx */
	count = MIN(TO_NET_BUFSIZE - ts->rdidx, avail);
	count = safe_read(ts->read_fd, BUF2NET(ts) + ts->rdidx, count);
	if (count <= 0) {
		if (count < 0) {
			if (errno == EAGAIN)
				return 0;
			/* login process might call vhangup(),
			 * which causes intermittent EIOs on read above
			 * (observed on kernel 4.12.0, not seen on 5.18.0).
			 * Try up to 10*1000 us.
			 */
			if (errno == EIO) {
				ts->eio_count++;
				dbg_close("EIO ptyfd:%d ts->eio_count:%d %06u",
						ts->read_fd, ts->eio_count,
						(unsigned)(monotonic_us() % 1000000)
				);
				if (ts->eio_count >= 10)
					goto close_ptyfd;
				ts->deadline_us = monotonic_us() + 1000;
				return 0; /* "read nothing" */
			}
			dbg_close("error on ptyfd:%d %m", ts->read_fd);
			/* (what other read errors from pty are possible, apart from EIO?) */
			ts->eio_count = 10; /* "fatally bad error" */
			goto close_ptyfd;
		}
		/* EOF. (Doesn't happen with master ptys on Linux when slave closes - always EIO instead?) */
		dbg_close("EOF on ptyfd:%d", ts->read_fd);
		ts->eio_count = 0;
 close_ptyfd:
		dbg_close("closing ptyfd:%d", ts->read_fd);
		close(ts->read_fd);
		if (ts->sibling)
			/* net2pty pipe needs to close (nowhere to write its data) */
			ts->sibling->write_fd = -1;
		ts->read_fd = -1;
		/* Returning -1 here would mean "do not try writing to net", which is wrong */
		return 0; /* EOF or error, but we do not prevent draining to net */
	}
	ts->eio_count = 0;

	ts->size += count;
	BUF2NET_INC(ts->rdidx, count);

	return count;
}

static int pty_to_net__have_data_to_write(void *this)
{
	pty_to_net_t *ts = this;
	if (ts->size == 0) {
		/* Wrote everything and pty is dead? */
		if (ts->read_fd < 0) {
			dbg_close("%s:%d: remove_and_free_to_net:%p rd:%d eio_count:%d",
					__func__, __LINE__, ts, ts->read_fd, ts->eio_count);
			if (ts->sibling)
				ts->write_fd = -1;
			remove_and_free_to_net(ts);
			return -1; /* "I'm gone" */
		}
	}
	return ts->size > 0 || ts->respond_to_AYT || ts->extra_byte_to_write;
}

/* Write to socket from buffer, escaping IAC bytes */
static int pty_to_net__write(void *this)
{
	pty_to_net_t *ts = this;
	ssize_t count;
	unsigned char *buf;
	unsigned char *iac_ptr;
	size_t wr;

	/* Did we fail to send a byte last time? */
	if (ts->extra_byte_to_write) {
		count = safe_write(ts->write_fd, &ts->extra_byte_to_write, 1);
		if (count == 1)
			ts->extra_byte_to_write = 0;
		goto ret;
	}

	if (ts->size == 0) {
		if (ts->respond_to_AYT) { /* other side pinged us? */
			/* Send back evidence that AYT was seen: IAC NOP */
			static const char ayt_response[2] = { IAC, NOP };
			count = safe_write(ts->write_fd, ayt_response, 2);
			if (count >= 1) {
				if (count == 1) /* goddamit */
					ts->extra_byte_to_write = NOP;
				ts->respond_to_AYT = 0;
			}
			goto ret;
		}
		return 0;
	}

	buf = BUF2NET(ts) + ts->wridx;
	wr = MIN(TO_NET_BUFSIZE - ts->wridx, ts->size);
	dbg("rem:%d sz:%d ch:0x%02x", TO_NET_BUFSIZE - ts->wridx, ts->size, *buf);

	if (*buf == IAC) {
		static const char IACIAC[2] ALIGN1 = { IAC, IAC };
		count = safe_write(ts->write_fd, IACIAC, 2);
		if (count <= 1) {
			if (count < 1) /* wrote nothing? */
				goto ret;
			/* Wrote only one byte of two. Amazing... */
			ts->extra_byte_to_write = IAC;
		}
		/* Consume one byte from buffer */
		count = 1;
	} else {
		/* Find next IAC or write up to end of segment */
		iac_ptr = memchr(buf, IAC, wr);
		if (iac_ptr)
			wr = iac_ptr - buf;
		count = safe_write(ts->write_fd, buf, wr);
		if (count <= 0)
			goto ret;
	}

	BUF2NET_INC(ts->wridx, count);
	ts->size -= count;
	if (ts->size == 0) { /* very typical */
		/* Avoid buffer wrapping most of the time (-> have bigger writes) */
		//dbg_buffer("zero size");
		ts->rdidx = 0;
		ts->wridx = 0;
	}
 ret:
	if (count >= 0)
		return count; /* "not error" */
	if (errno == EAGAIN)
		return 0; /* "not error" */

	/* Assuming fatal write error */
	/* If peer closes its read side with
	 * shutdown(SHUT_RD), we'll see EPIPE
	 * or ECONNRESET.
	 * (both were seen to occur in practice)
	 */
	if (ts->sibling) {
		/* Sibling exists, don't close yet */
		ts->read_fd = -1;
		ts->write_fd = -1;
	}
	dbg_close("%s:%d: remove_and_free_to_net:%p", __func__, __LINE__, ts);
	remove_and_free_to_net(ts);
	return -1; /* "I'm gone" */
}

static pty_to_net_t *new_pty_to_net(int from, int to)
{
	pty_to_net_t *this = xzalloc(sizeof(*this) + TO_NET_BUFSIZE);
	this->have_buffer_to_read_into = pty_to_net__have_buffer_to_read_into;
	this->have_data_to_write       = pty_to_net__have_data_to_write;
	this->read                     = pty_to_net__read;
	this->write                    = pty_to_net__write;
	this->read_fd = from;
	this->write_fd = to;
	/* indexes and size are all 0 */
	return this;
}

#if !ENABLE_FEATURE_TELNETD_STANDALONE
#define make_new_session(io, sockrd) \
	make_new_session(io)
#endif
static void make_new_session(ioloop_state_t *io, int sockrd)
{
	IF_NOT_FEATURE_TELNETD_STANDALONE(enum { sockrd = 0 };)
	const int sockwr = sockrd != 0 ? sockrd : 1;
	const char *login_argv[2];
	struct termios termbuf;
	int ptyfd, pid;
	char tty_name[GETPTY_BUFSIZE];

	/* Got a new connection, set up a pty */
	ptyfd = xgetpty(tty_name);
	ndelay_on(ptyfd);
	close_on_exec_on(ptyfd);

	/* SO_KEEPALIVE by popular demand */
	setsockopt_keepalive(sockrd);
	ndelay_on(sockrd);
	if (sockrd == 0) /* called with fd 0 (inetd mode?) */
		ndelay_on(sockwr);

	/* Make the telnet client understand we will echo characters so it
	 * should not do it locally. We don't tell the client to run linemode,
	 * because we want to handle line editing and tab completion and other
	 * stuff that requires char-by-char support. */
	{
		static const char iacs_to_send[] ALIGN1 = {
			IAC, WILL, TELOPT_ECHO, // "I will echo your chars"
			// (Not really, _we_ won't - our programs in pty usually
			// handle that. In practice, this says: "do not echo
			// your user's input chars back to him _on your side_".)
			IAC, WILL, TELOPT_SGA,  // "I assume full-duplex, won't send GA's"
			//IAC, DO, TELOPT_ECHO, //WRONG: "can you echo my chars to me"??
			IAC, DO, TELOPT_NAWS,   // "can you send me terminal size data?"
			// This requires telnetd.ctrlSQ.patch (incomplete):
			//IAC, DO, TELOPT_LFLOW,
		};
//Theoretically, our "WILL X" are requests and should only activate when client responds with "DO X".
//However, we do not wait/check for "DO"s. Why?
//There is nothing to "activate" on our side for TELOPT_ECHO.
//For TELOPT_SGA, we don't even have code to support sending/understanding GAs,
//so SGA is "always activated".
		/* Just stuff it into TCP stream! (no error check...) */
		safe_write(sockwr, iacs_to_send, sizeof(iacs_to_send));
	}

	fflush_all();
	pid = vfork(); /* NOMMU-friendly */
	if (pid < 0) {
		close(ptyfd);
		close(sockrd);
		bb_simple_perror_msg("vfork");
		return;
	}
	if (pid > 0) {
		/* Parent */
		pty_to_net_t *to_net;
		net_to_pty_t *to_pty;

		to_net = new_pty_to_net(ptyfd, sockwr);
		to_pty = new_net_to_pty(sockrd, ptyfd);
		dbg("to_net:%p to_pty:%p", to_net, to_pty);
		to_pty->sibling = to_net;
		to_net->sibling = to_pty;
		to_net->shell_pid = pid;
		to_pty->shell_pid = pid;
		ioloop_insert_conn(io, (connection_t *)to_net);
		ioloop_insert_conn(io, (connection_t *)to_pty);
		return;
	}

	/* Child */
	/* Careful - we are after vfork! */

	/* Restore default signal handling ASAP */
	bb_signals((1 << SIGCHLD) + (1 << SIGPIPE), SIG_DFL);

	/* Make new session and process group */
	//pid = getpid(); // redundant, setsid gives us our pid
	pid = setsid();

	/* Open the child's side of the pty */
	/* NB: setsid() disconnects from any previous ctty's. Therefore
	 * we must open child's side of the tty AFTER setsid! */
	close(0);
	xopen(tty_name, O_RDWR); /* fd 0: becomes our ctty */
	xdup2(0, 1);
	xdup2(0, 2);
	tcsetpgrp(0, pid); /* switch this tty's process group to us */

	if (ENABLE_FEATURE_UTMP) {
		len_and_sockaddr *lsa = get_peer_lsa(sockrd);
		char *hostname = NULL;
		if (lsa) {
			hostname = xmalloc_sockaddr2dotted(&lsa->u.sa);
			free(lsa);
		}
		write_new_utmp(pid, LOGIN_PROCESS, tty_name, /*username:*/ "LOGIN", hostname);
		IF_FEATURE_CLEAN_UP(free(hostname);)
	}

	/* The pseudo-terminal allocated to the client is configured to operate
	 * in cooked mode, and with XTABS CRMOD enabled (see tty(4)) */
	tcgetattr(0, &termbuf);
	termbuf.c_lflag |= ECHO; /* if we use readline we dont want this */
	termbuf.c_oflag |= ONLCR | XTABS;
	termbuf.c_iflag |= ICRNL;
	termbuf.c_iflag &= ~IXOFF;
	/*termbuf.c_lflag &= ~ICANON;*/
	tcsetattr_stdin_TCSANOW(&termbuf);

	/* Uses FILE-based I/O to stdout, but does fflush_all(),
	 * so should be safe with vfork.
	 * I fear, though, that some users will have ridiculously big
	 * issue files, and they may block writing to fd 1,
	 * (parent is supposed to read it, but parent waits
	 * for vforked child to exec!) */
	print_login_issue(G.issuefile, tty_name);

	/* Exec shell / login / whatever */
	login_argv[0] = G.loginpath;
	login_argv[1] = NULL;
	/* exec busybox applet (if PREFER_APPLETS=y), if that fails,
	 * exec external program.
	 * NB: sockrd is either 0 or has CLOEXEC set on it.
	 * ptyfd has CLOEXEC set on it too. These two fds will be closed here.
	 */
	BB_EXECVP(G.loginpath, (char **)login_argv);
	/* _exit is safer with vfork, and we shouldn't send message
	 * to remote clients anyway */
	_exit_FAILURE(); /*bb_perror_msg_and_die("execv %s", G.loginpath);*/
}


#if ENABLE_FEATURE_TELNETD_STANDALONE
struct accept_conn {
	TELNET_CONNECTION
};

static int accept_conn__can_accept(void *this UNUSED_PARAM)
{
	return 1;
}

static int accept_conn__accept(void *this)
{
	struct accept_conn *ts = this;
	int fd;

	dbg("%s:%d", __func__, __LINE__);
	fd = accept(ts->read_fd, NULL, NULL);
//TODO: accept4(SOCK_NONBLOCK) - can remove ndelay_on() in make_new_session
	if (fd >= 0) {
		close_on_exec_on(fd);
		/* Create two new pipes and insert them into ioloop */
		make_new_session(ts->io, fd);
	}
	return 0;
}

static int accept_conn__return_zero(void *this UNUSED_PARAM)
{
	return 0;
}

static struct accept_conn *new_accept_conn(int fd)
{
	struct accept_conn *this = xzalloc(sizeof(*this));
	this->have_buffer_to_read_into = accept_conn__can_accept;
	this->have_data_to_write       = accept_conn__return_zero;
	this->read                     = accept_conn__accept;
	//this->write                    = accept_conn__return_zero; //never called
	this->read_fd = fd;
	this->write_fd = -1;
	return this;
}
#endif

static void handle_sigchld(int sig UNUSED_PARAM)
{
	int save_errno = errno;

	/* Looping: more than one child may have exited */
	while (1) {
		pid_t pid;
		struct telnet_conn *conn;

		pid = wait_any_nohang(NULL);
		if (pid <= 0)
			break;
		update_utmp_DEAD_PROCESS(pid);
		conn = (void*)G.io.conns;
		while (conn) {
			if (conn->shell_pid == pid) {
				/* mark the conn to close soon */
				conn->shell_pid = -1;
			}
			conn = (void*)conn->next;
		}
	}

	errno = save_errno;
}

#if ENABLE_FEATURE_TELNETD_SELFTEST_DEBUG

static char *bin_to_hex(const void *hash_value, unsigned hash_length)
{
	/* xzalloc zero-terminates */
	char *hex_value = xzalloc((hash_length * 2) + 1);
	bin2hex(hex_value, (char*)hash_value, hash_length);
	return auto_string(hex_value);
}

static int test_vhangup(void)
{
	char tty_name[GETPTY_BUFSIZE];
	int master_fd;
	int last_rc = 999;
	int last_errno = 0;
	unsigned long long last_change_ns, base_ns;

	bb_simple_info_msg("selftest: pty vhangup() behavior");

	master_fd = xgetpty(tty_name);
	ndelay_on(master_fd);

	fflush_all();
	if (xfork() == 0) {
		/* Child */
		//int slave_fd;
		int fd;

		/* vhangup() sends SIGHUP to the session, ignore it */
		signal(SIGHUP, SIG_IGN);

		applet_name = "vhangup child";

		close(master_fd);
		setsid();
		/*slave_fd =*/ xopen(tty_name, O_RDWR);

		fd = open("/dev/tty", O_RDWR);
		if (fd >= 0) {
			bb_simple_info_msg("/dev/tty opened OK - we have a ctty");
			close(fd);
		} else {
			bb_simple_perror_msg("/dev/tty");
		}

		bb_simple_info_msg("sleeping 0.2 sec before vhangup()");
		usleep(200*1000);

		bb_simple_info_msg("calling vhangup()");
		vhangup();

		fd = open("/dev/tty", O_RDWR);
		if (fd >= 0) {
			bb_simple_info_msg("/dev/tty opened OK - we still have a ctty");
			close(fd);
		} else {
			bb_simple_perror_msg("/dev/tty");
		}

		bb_simple_info_msg("sleeping 0.2 sec after vhangup()");
		usleep(200*1000);

		bb_simple_error_msg_and_die("exiting");
	}

	/* Parent */
	bb_simple_info_msg("nonblocking read() from pty master in a loop...");
	last_change_ns = monotonic_ns();

	base_ns = monotonic_ns();
	for (;;) {
		int rc;
		char buf[1];
		unsigned long long now_ns;

		errno = 0;
		rc = read(master_fd, buf, 1);
		now_ns = monotonic_ns() - base_ns;

		if (rc != last_rc || errno != last_errno) {
			bb_error_msg("t=%lluns: read()=%d errno=%d(%s)",
				now_ns,
				rc, errno, strerror(errno));
			last_rc = rc;
			last_errno = errno;
			last_change_ns = now_ns;
		}

		/* Exit if no change for a few seconds */
		if ((now_ns - last_change_ns) > 1000*1000*1000) {
			bb_simple_info_msg("no change for 1 second, loop stopped");
			break;
		}

		usleep(100); /* 100us polling - fast enough to catch transitions */
	}

	close(master_fd);
	waitpid(-1, NULL, 0);

	bb_simple_error_msg("vhangup test completed");
	return 0;
}
static int test_net_to_pty_data_integrity(void)
{
	enum { TESTDATA_SIZE = 16 * 1024 * 1024 };
	unsigned char *input_data;
	unsigned char *expected_data;
	int input_size;
	int expected_size;
	uint32_t randbits;
	uint8_t expected_hash32[32];
	uint8_t hash_in_pty32[32];
	uint8_t dummy_byte;
	int sock_pair[2];
	char tty_name[GETPTY_BUFSIZE];
	int ptyfd;
	int i;
	int waitstatus, exitcode;

	bb_simple_info_msg("selftest: net_to_pty");

	/* Allocate buffer with paranoia space */
	input_data = xzalloc(TESTDATA_SIZE + 64);

	/* Generate nightmare input data:
	 * - Mostly random bytes
	 * - IAC bytes sprinkled in (1 in 16 probability)
	 * - IAC IAC sequences (1 in 32)
	 * - IAC SB TELOPT_NAWS sequences(1 in 32)
	 * - All sorts of IAC commands with random parameters
	 * - Malformed sequences
	 */
	randbits = 0;
	for (i = 0; i < TESTDATA_SIZE;) {
		unsigned _32possibilities;
		if (randbits == 0)
			randbits = random();
		_32possibilities = (randbits & 0x1f);
		randbits >>= 5;
		if (_32possibilities == 3 && (randbits & 0x7f) == 0) { /* 1 in 32*127=4*1k prob: long run of printables */
			if (i < TESTDATA_SIZE - 4*1024) {
				int end = i + ((4*1024 - 1) & random());
				while (i < end)
					input_data[i++] = (random() & 0x3f) + 0x20;
				continue;
			}
		}
		if (_32possibilities == 3) { /* CR,LF/NUL: 1 in 32 prob */
			input_data[i++] = '\r';
			input_data[i++] = randbits & 1 ? '\n' : '\0';
		} else
		if (_32possibilities == 2) { /* NAWS: 1 in 32 prob */
			input_data[i++] = IAC;
			input_data[i++] = SB;
			input_data[i++] = TELOPT_NAWS;
		} else
		if (_32possibilities == 1) { /* IAC: 1 in 32 prob */
			input_data[i++] = IAC;
			if (random() & 1) /* IAC IAC: 1 in 64 prob */
				input_data[i++] = IAC;
		} else {
			input_data[i++] = random();
		}
	}
	input_size = TESTDATA_SIZE;
	bb_error_msg("generated %d bytes of test data", input_size);

	/* Allocate expected output buffer */
	expected_data = xmalloc(TESTDATA_SIZE);

	/* Process input_data simulating net_to_pty transformations:
	 * - IAC IAC → single IAC
	 * - IAC SB TELOPT_NAWS [4 bytes] IAC SE → stripped
	 * - Other IAC sequences → stripped
	 * - \r\n → \r (skip the \n)
	 * - \r\0 → \r (skip the \0)
	 */
	memset(input_data + input_size, 0, 64);
	expected_size = 0;
	i = 0;
	while (i < input_size) {
		/* Tail of 64 extra zero bytes allows to safely read beyond the end */
		if (input_data[i] == IAC) {
			if (input_data[i + 1] == IAC) {
				/* IAC IAC → single IAC */
				expected_data[expected_size++] = IAC;
				i += 2;
				continue;
			}
			if (input_data[i + 1] >= 240 && input_data[i + 1] <= 249) {
				/* 2-byte IAC command (NOP, etc.) - skip */
				i += 2;
				continue;
			}
			if (input_data[i + 1] == SB) {
				/* IAC SB ... skip subnegotiation */
				if (input_data[i + 2] == TELOPT_NAWS) {
//TODO: simulate unusual NAWS
					/* IAC SB TELOPT_NAWS [4 bytes] */
					i += 3; /* skip IAC SB TELOPT_NAWS */
					/* Skip 4 bytes of window size */
					i += 4;
					continue;
//				} else {
//should do: /* Other subneg, skip to IAC SE */ but that's not what telnetd code is doing yet...
				}
			}
			/* Assumed 3-byte IAC WILL/WONT/DO/DONT - skip */
			i += 3;
			continue;
		}
		if (input_data[i] == '\r') {
			/* Write \r, check if followed by \n or \0 */
			expected_data[expected_size++] = '\r';
			i++;
			if (input_data[i] == '\n' || input_data[i] == '\0')
				i++; /* skip \n or \0 after \r */
			continue;
		}
		/* Regular byte */
		expected_data[expected_size++] = input_data[i++];
	}
	sha256_block(expected_data, expected_size, expected_hash32);
	bb_error_msg("expected %d bytes after net_to_pty processing. hash:%s", expected_size, bin_to_hex(expected_hash32, 32));
#if SAVE_NET2PTY_EXPECTED
	{
		int fd = xopen("expected.bin", O_WRONLY | O_CREAT | O_TRUNC);
		xwrite(fd, expected_data, expected_size);
		close(fd);
		bb_error_msg("wrote expected.bin");
	}
#endif
	free(expected_data);
	expected_data = NULL; /* to catch use-after-free */

	/* Create socketpair for network side */
	if (socketpair(AF_UNIX, SOCK_STREAM, 0, sock_pair) < 0)
		bb_simple_perror_msg_and_die("socketpair");

	fflush_all();
	if (xfork() == 0) {
		/* Fork writer child: writes telnet data to socket in random chunks */
		int written = 0;
		close(sock_pair[0]); /* close read end */

		applet_name = "tty input";

		while (written < input_size) {
			int chunk = (random() & 4095) + 1; /* 1 to 4096 bytes */
			if (written + chunk > input_size)
				chunk = input_size - written;

			chunk = safe_write(sock_pair[1], input_data + written, chunk);
			if (chunk > 0)
				written += chunk;

			/* Small random delay to vary timing */
			if ((random() & 0xf) == 0)
				usleep(random() & 0x3ff);
		}
		_exit(0);
	}
	close(sock_pair[1]); /* close write end */

	free(input_data);
	input_data = NULL; /* to catch use-after-free */

	/* Create pty pair */
	ptyfd = xgetpty(tty_name);

	if (xfork() == 0) {
		/* Fork reader child: reads from PTY slave */
		int slave_fd;
		int retry;
		int n;
		unsigned char read_buf[8192];
		int total_read = 0;
		struct termios termbuf;
		sha256_ctx_t ctx;
#if SAVE_NET2PTY_ACTUAL
		int out_fd = xopen("actual.bin", O_WRONLY | O_CREAT | O_TRUNC);
#endif
		applet_name = "pty reader";

		close(sock_pair[0]);
		close(ptyfd);

		slave_fd = xopen(tty_name, O_RDWR);
		setsid(); /* this loses ctty (no tty signals ^C, ^Z, ^\; no SIGHUP on master close (probably?)) */

		tcgetattr(slave_fd, &termbuf);
		cfmakeraw(&termbuf);
		tcsetattr(slave_fd, TCSANOW, &termbuf);
		//bb_error_msg("tty_name:'%s'", tty_name);

		/* Synchronize with master: "raw set, you can start writing" */
		dummy_byte = '!';
		safe_write(slave_fd, &dummy_byte, 1);

		sha256_begin(&ctx);
		/* Retry logic shows how master pty close looks on slave side:
		 * on kernel 5.18.0, I see EIO once, then (if I retry reads),
		 * I see EOFs (zero reads).
		 * What is not visible (but does happen) is that any
		 * unread-by-me buffered data is lost - something that
		 * wouldn't happen on pipes/socketpairs!
		 */
		retry = 0;
		for (;;) {
			n = safe_read(slave_fd, read_buf, sizeof(read_buf));
			if (n == 0) {
				retry++;
				bb_error_msg("EOF on read(%d)", retry);
				if (retry < 3) continue;
				break; /* EOF (parent closed master) - expected */
			}
			if (n < 0) {
				retry++;
				bb_perror_msg("read(%d)", retry);
				if (retry < 3) continue;
				break; /* error (parent closed master) - expected */
			}
			/* If happens, probably kernel bug! */
			if (retry != 0) bb_error_msg_and_die("READ DATA AFTER EOF/ERROR RETRY!!!");
#if SAVE_NET2PTY_ACTUAL
			xwrite(out_fd, read_buf, n);
#endif
			sha256_hash(&ctx, read_buf, n);
			total_read += n;
			// Test premature close on pty read side:
			//if (total_read >= 128*1024) _exit(0);
		}

		sha256_end(&ctx, hash_in_pty32);
		n = memcmp(hash_in_pty32, expected_hash32, 32);
		bb_error_msg("received %d bytes, hash:%s%s", total_read, bin_to_hex(hash_in_pty32, 32), n ? " MISMATCH" : "");
		_exit(!!n);
	}

	/* Parent: run the net_to_pty logic */
	{
		net_to_pty_t *conn;
		ioloop_state_t *io;
		int rc;

		io = new_ioloop_state();
		conn = new_net_to_pty(sock_pair[0], ptyfd);
		ioloop_insert_conn(io, (void*)conn);

		/* We must allow the pty child to change its tty to RAW
		 * before we start writing. Otherwise (it was observed),
		 * it can see a few \r -> \n remapped.
		 */
		safe_read(ptyfd, &dummy_byte, 1);

		rc = ioloop_run(io);
		bb_error_msg("ioloop_run:%d", rc);

		/* This should be done inside ioloop. Check where possible: */
		//free_connection(conn);
		if (close(sock_pair[0]) == 0) /* should be EBADF */
			bb_error_msg_and_die("BUG: net read side wasn't closed");
		if (close(ptyfd) == 0)
			bb_error_msg_and_die("BUG: pty write side wasn't closed");

		free_ioloop_state(io);
	}

	/* Wait for all children */
	exitcode = 0;
	while (safe_waitpid(-1, &waitstatus, 0) > 0)
		exitcode |= (waitstatus != 0);

	bb_error_msg("pty_to_net test completed: %d", exitcode);
	return exitcode;
}

static int test_pty_to_net_data_integrity(void)
{
	enum { TESTDATA_SIZE = 16 * 1024 * 1024 };
	unsigned char *input_data;
	unsigned char *expected_data;
	int input_size;
	int expected_size;
	uint32_t randbits;
	uint8_t expected_hash32[32];
	uint8_t hash_from_net32[32];
	int sock_pair[2];
	char tty_name[GETPTY_BUFSIZE];
	int ptyfd;
	int i;
	int waitstatus, exitcode;

	bb_simple_info_msg("selftest: pty_to_net");

	/* Allocate buffer */
	input_data = xzalloc(TESTDATA_SIZE);

	/* Generate random data - this time we want lots of 0xFF (IAC) bytes
	 * to test IAC escaping (IAC → IAC IAC)
	 */
	randbits = 0;
	for (i = 0; i < TESTDATA_SIZE;) {
		unsigned _32possibilities;
		if (randbits == 0)
			randbits = random();
		_32possibilities = (randbits & 0x1f);
		randbits >>= 5;

		if (_32possibilities == 1 && (randbits & 0x7f) == 0) {
			/* Long run of printables (1 in ~4k prob) */
			if (i < TESTDATA_SIZE - 4*1024) {
				int end = i + ((4*1024 - 1) & random());
				while (i < end)
					input_data[i++] = (random() & 0x3f) + 0x20;
				continue;
			}
		}
		if (_32possibilities == 1) {
			/* IAC byte: 1 in 32 prob */
			input_data[i++] = IAC;
		} else {
			input_data[i++] = random();
		}
	}
	input_size = TESTDATA_SIZE;
	bb_error_msg("generated %d bytes of test data", input_size);

	/* Allocate expected output buffer (worst case: all IACs doubled) */
	expected_data = xmalloc(TESTDATA_SIZE * 2);

	/* Process input_data simulating pty_to_net transformations:
	 * - IAC (0xFF) → IAC IAC (doubled)
	 * - All other bytes pass through unchanged
	 */
	expected_size = 0;
	for (i = 0; i < input_size; i++) {
		if (input_data[i] == IAC) {
			expected_data[expected_size++] = IAC;
			expected_data[expected_size++] = IAC;
		} else {
			expected_data[expected_size++] = input_data[i];
		}
	}

	sha256_block(expected_data, expected_size, expected_hash32);
	bb_error_msg("expected %d bytes after pty_to_net processing. hash:%s",
		expected_size, bin_to_hex(expected_hash32, 32));

#if SAVE_PTY2NET_EXPECTED
	{
		int fd = xopen("expected.bin", O_WRONLY | O_CREAT | O_TRUNC);
		xwrite(fd, expected_data, expected_size);
		close(fd);
		bb_error_msg("wrote expected.bin");
	}
#endif

	free(expected_data);
	expected_data = NULL;

	/* Create socketpair for network side */
	if (socketpair(AF_UNIX, SOCK_STREAM, 0, sock_pair) < 0)
		bb_simple_perror_msg_and_die("socketpair");

	/* Create pty pair */
	ptyfd = xgetpty(tty_name);

	fflush_all();
	if (xfork() == 0) {
		/* Fork writer child: writes data to PTY slave */
		int slave_fd;
		int written = 0;
		struct termios termbuf;

		applet_name = "pty writer";

		close(sock_pair[0]);
		close(sock_pair[1]);
		close(ptyfd);

		slave_fd = xopen(tty_name, O_RDWR);
		//setsid(); /* lose ctty */

		/* Put PTY in raw mode */
		tcgetattr(slave_fd, &termbuf);
		cfmakeraw(&termbuf);
		tcsetattr(slave_fd, TCSANOW, &termbuf);

		while (written < input_size) {
			int n = (random() & 4095) + 1; /* 1 to 4096 bytes */
			if (n > input_size - written)
				n = input_size - written;
			n = safe_write(slave_fd, input_data + written, n);
			if (n < 0)
				bb_perror_msg_and_die("%s:%d: write error", __func__, __LINE__);
			written += n;
			/* Small random delay */
			if ((random() & 0xf) == 0)
				usleep(random() & 0x3ff);
		}
		_exit(0);
	}

	free(input_data);
	input_data = NULL;

	if (xfork() == 0) {
		/* Fork reader child: reads from network socket */
		int n;
		unsigned char read_buf[8192];
		int total_read = 0;
		sha256_ctx_t ctx;
#if SAVE_PTY2NET_ACTUAL
		int out_fd = xopen("actual.bin", O_WRONLY | O_CREAT | O_TRUNC);
#endif
		applet_name = "net reader";

		close(sock_pair[1]); /* close write end */
		close(ptyfd);

		sha256_begin(&ctx);
		for (;;) {
			n = safe_read(sock_pair[0], read_buf, sizeof(read_buf));
			if (n <= 0) {
				if (n < 0)
					bb_simple_perror_msg("read");
				break; /* EOF or error */
			}
			// To test what writer sees if we close read end:
			//shutdown(sock_pair[0], SHUT_RD);
			//sleep(2);
			//_exit(1);
#if SAVE_PTY2NET_ACTUAL
			xwrite(out_fd, read_buf, n);
#endif
			sha256_hash(&ctx, read_buf, n);
			total_read += n;
		}

		sha256_end(&ctx, hash_from_net32);
		n = memcmp(hash_from_net32, expected_hash32, 32);
		bb_error_msg("received %d bytes, hash:%s%s",
			total_read, bin_to_hex(hash_from_net32, 32), n ? " MISMATCH" : "");
		_exit(!!n);
	}

	close(sock_pair[0]); /* close read end */

	/* Parent: run the pty_to_net logic */
	{
		pty_to_net_t *conn;
		ioloop_state_t *io;
		int rc;

		io = new_ioloop_state();

		conn = new_pty_to_net(ptyfd, sock_pair[1]);
		ioloop_insert_conn(io, (void*)conn);

		rc = ioloop_run(io);
		bb_error_msg("ioloop_run:%d", rc);

		/* This should be done inside ioloop. Check where possible: */
		//free_connection(conn);
		if (close(sock_pair[1]) == 0) /* should be EBADF */
			bb_error_msg_and_die("BUG: net write side wasn't closed");
		if (close(ptyfd) == 0)
			bb_error_msg_and_die("BUG: pty read side wasn't closed");

		free_ioloop_state(io);
	}

	/* Wait for all children */
	exitcode = 0;
	while (safe_waitpid(-1, &waitstatus, 0) > 0)
		exitcode |= (waitstatus != 0);

	bb_error_msg("pty_to_net test completed: %d", exitcode);
	return exitcode;
}

static void selftest(void)
{
	int exitcode = 0;
	srandom(12345);
	if (SELFTEST_VHANGUP) exitcode |= test_vhangup();
	if (SELFTEST_NET2PTY) exitcode |= test_net_to_pty_data_integrity();
	if (SELFTEST_PTY2NET) exitcode |= test_pty_to_net_data_integrity();
	bb_error_msg("selftest completed: %d", exitcode);
	_exit(exitcode);
}
#endif

int telnetd_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int telnetd_main(int argc UNUSED_PARAM, char **argv)
{
	unsigned opt;
#if ENABLE_FEATURE_TELNETD_STANDALONE
#define IS_INETD (opt & OPT_INETD)
	char *opt_bindaddr = NULL;
	char *opt_portnbr;
#else
	enum { IS_INETD = 1 };
#endif
	INIT_G();

	/* Even if !STANDALONE, we accept (and ignore) -i, thus people
	 * don't need to guess whether it's ok to pass -i to us */
	opt = getopt32(argv, "^"
			IF_FEATURE_TELNETD_SELFTEST_DEBUG("@")
			"f:l:Ki"
			IF_FEATURE_TELNETD_STANDALONE("p:b:F")
			IF_FEATURE_TELNETD_INETD_WAIT("Sw:+v") /* -w NUM */
			"\0"
			/* -w implies -F. -w and -i don't mix. -v counter */
			IF_FEATURE_TELNETD_INETD_WAIT("wF:i--w:w--i:vv")
			, &G.issuefile, &G.loginpath
			IF_FEATURE_TELNETD_STANDALONE(, &opt_portnbr, &opt_bindaddr)
			IF_FEATURE_TELNETD_INETD_WAIT(, &G.io.max_timeout)
			, &G.verbose
	);

#if ENABLE_FEATURE_TELNETD_SELFTEST_DEBUG
	if (opt & 1) { /* -@ is first option */
		selftest(); /* does not return */
	}
	opt >>= 1; /* Shift away the selftest bit */
#endif
	if (!IS_INETD /*&& !re_execed*/) {
		/* inform that we start in standalone mode?
		 * May be useful when people forget to give -i */
		/*bb_error_msg("listening for connections");*/
		if (!(opt & OPT_FOREGROUND)) {
			/* DAEMON_CHDIR_ROOT was giving inconsistent
			 * behavior with/without -F, -i */
			bb_daemonize_or_rexec(0 /*was DAEMON_CHDIR_ROOT*/, argv);
		}
	}
	/* Redirect log to syslog early, if needed */
	if (IS_INETD || (opt & OPT_SYSLOG) || !(opt & OPT_FOREGROUND)) {
		openlog(applet_name, LOG_PID, LOG_DAEMON);
		logmode = LOGMODE_SYSLOG;
	}

	if (IS_INETD) {
		make_new_session(&G.io, 0);
	}
#if ENABLE_FEATURE_TELNETD_STANDALONE
	else {
		int master_fd = 0;
		/* For -w SEC, listening socket is 0. Otherwise: */
		if (!(opt & OPT_WAIT)) {
			unsigned portnbr = CONFIG_FEATURE_TELNETD_PORT_DEFAULT;
			if (opt & OPT_PORT)
				portnbr = xatou16(opt_portnbr);
			/* If someone would run "telnetd 0>&-", we can get terribly confused. */
			/* Nake sure master_fd, or accept() result fd, etc, cant become 0 or 1: */
			bb_sanitize_stdio();
			master_fd = create_and_bind_stream_or_die(opt_bindaddr, portnbr);
			xlisten(master_fd, 1);
		}
		close_on_exec_on(master_fd);
		ioloop_insert_conn(&G.io, (void *)new_accept_conn(master_fd));
	}
#endif
	/* We don't want to die if just one session is broken */
	signal(SIGPIPE, SIG_IGN);

	if (opt & OPT_WATCHCHILD)
		signal(SIGCHLD, handle_sigchld);
	else /* prevent dead children from becoming zombies */
		signal(SIGCHLD, SIG_IGN);

#if ENABLE_FEATURE_TELNETD_INETD_WAIT
	if (G.io.max_timeout > UINT_MAX / 1000000) /* -w TMOUT capped at 4294 sec */
		G.io.max_timeout = (UINT_MAX / 1000000);
	G.io.max_timeout *= 1000000;
#endif
	G.io.flags |= IOLOOP_FLAG_EXIT_IF_TIMEOUT;
	for (;;) {
		int rc = ioloop_run(&G.io);
		if (rc == IOLOOP_NO_CONNS) {
			/* inetd mode (not -w), and it's closed now */
			dbg_close("connection is closed");
			break;
		}
		/* else: timeout */
#if ENABLE_FEATURE_TELNETD_INETD_WAIT
		if (1 /* rc == IOLOOP_TIMEOUT */
		 && (!G.io.conns || !G.io.conns->next) /* only accept conn exists */
		) {
			dbg_close("-w TIMEOUT:%dus", G.io.max_timeout);
			break;
		}
#endif
		dbg("timeout was due to temporary EIO: continue looping");
	}
	dbg_close("terminating");
	return EXIT_SUCCESS;
}
