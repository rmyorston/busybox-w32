/* vi: set sw=4 ts=4: */
/*
 * telnet implementation for busybox
 *
 * Author: Tomi Ollila <too@iki.fi>
 * Copyright (C) 1994-2000 by Tomi Ollila
 *
 * Created: Thu Apr  7 13:29:41 1994 too
 * Last modified: Fri Jun  9 14:34:24 2000 too
 *
 * Licensed under GPLv2 or later, see file LICENSE in this source tree.
 *
 * HISTORY
 * 1994/04/17 Initial revision
 * 2000/06/13 for inclusion into BusyBox by Erik Andersen <andersen@codepoet.org>
 * 2001/05/07 add ability to pass TTYPE to remote host by Jim McQuillan <jam@ltsp.org>
 * 2004/02/11 add ability to pass the USER variable to remote host by Fernando Silveira <swrh@gmx.net>
 * 2026/02/20 Almost total rewrite to use ioloop()
 */
//config:config TELNET
//config:	bool "telnet (8.8 kb)"
//config:	default y
//config:	help
//config:	Telnet is an interface to the TELNET protocol, but is also commonly
//config:	used to test other simple protocols.
//config:
//config:config FEATURE_TELNET_TTYPE
//config:	bool "Pass TERM type to remote host"
//config:	default y
//config:	depends on TELNET
//config:	help
//config:	Setting this option will forward the TERM environment variable to the
//config:	remote host you are connecting to. This is useful to make sure that
//config:	things like ANSI colors and other control sequences behave.
//config:
//config:config FEATURE_TELNET_AUTOLOGIN
//config:	bool "Pass USER type to remote host"
//config:	default y
//config:	depends on TELNET
//config:	help
//config:	Setting this option will forward the USER environment variable to the
//config:	remote host you are connecting to. This is useful when you need to
//config:	log into a machine without telling the username (autologin). This
//config:	option enables '-a' and '-l USER' options.
//config:
//config:config FEATURE_TELNET_WIDTH
//config:	bool "Enable window size autodetection"
//config:	default y
//config:	depends on TELNET

//applet:IF_TELNET(APPLET(telnet, BB_DIR_USR_BIN, BB_SUID_DROP))

//kbuild:lib-$(CONFIG_TELNET) += telnet.o

//usage:#define telnet_trivial_usage
//usage:	IF_FEATURE_TELNET_AUTOLOGIN("[-a] [-l USER] ")"HOST [PORT]"
//usage:#define telnet_full_usage "\n\n"
//usage:       "Connect to telnet server"
//usage:	IF_FEATURE_TELNET_AUTOLOGIN("\n"
//usage:     "\n	-a	Automatic login with $USER envvar"
//usage:     "\n	-l USER	Automatic login as USER"
//usage:	)

#include <arpa/telnet.h>
#include <netinet/in.h>
#include "libbb.h"
#include "common_bufsiz.h"

#ifdef __BIONIC__
/* should be in arpa/telnet.h */
# define IAC         255  /* interpret as command: */
# define DONT        254  /* you are not to use option */
# define DO          253  /* please, you use option */
# define WONT        252  /* I won't use option */
# define WILL        251  /* I will use option (if I see your "IAC DO <opt>" confirmation */
# define SB          250  /* begin subnegotiation */
# define SE          240  /* end subnegotiation */
# define TELOPT_ECHO   1  /* echo */
# define TELOPT_SGA    3  /* suppress go ahead */
# define TELOPT_TTYPE 24  /* terminal type */
# define TELOPT_NAWS  31  /* window size */
#endif

#define SUPPORT_FOR_LOCAL_OPTS (0 \
	|| ENABLE_FEATURE_TELNET_WIDTH \
	|| ENABLE_FEATURE_TELNET_TTYPE \
	|| ENABLE_FEATURE_TELNET_AUTOLOGIN \
	)

// -v option
#define VERBOSE 0
// lots of unconditional debug
#define DEBUG   0

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

#if VERBOSE
# define log1(...) do { if (G.verbose) bb_error_msg(__VA_ARGS__); } while (0)
# define IF_VERBOSE(...) __VA_ARGS__
#else
# define log1(...) ((void)0)
# define IF_VERBOSE(...) /* nothing */
#endif

enum {
	TS_NORMAL = 0,
	TS_IAC  = 1,
	TS_OPT  = 2,
	TS_SUB1 = 3,
	TS_SUB2 = 4,
	TS_CR   = 5,

	MAX_NAWS_SIZE = 13, // pathological 65535x65535 case needs full escaping

	netfd   = 3,
};

typedef unsigned char byte;

typedef struct stdin_to_net {
	STRUCT_CONNECTION
	int rdidx, wridx, size;
	//byte buf[BUFSIZE];
} stdin_to_net_t;
typedef struct net_to_stdout {
	STRUCT_CONNECTION
	int rdidx, wridx, size;
	byte input_state;
	byte negotiation_verb;
	//byte buf[BUFSIZE];
} net_to_stdout_t;
#if DEBUG
static void set_input_state(net_to_stdout_t *conn, int new_state, int c)
{
	if (conn->input_state != new_state) {
		conn->input_state = new_state;
		dbg("input_state=%d at char:0x%02x '%c'", new_state, c,
			c >= 32 && c < 127 ? c : '.');
	}
}
# define SET_INPUT_STATE(conn, state, c) set_input_state(conn, state, c)
#else
# define SET_INPUT_STATE(conn, state, c) ((conn)->input_state = (state))
#endif

struct globals {
	unsigned        flags;
// Set when server agreed to use NAWS:
#define FLAGS_NAWS_ON   (1 << 0)
// SGA option seen and responded to, no longer look for it:
#define FLAGS_SGA_SEEN  (1 << 1)
// Seen telnet protocol from server and sent our WILLs:
#define INITIAL_SENT    (1 << 2)
#define DO_TERMIOS      (1 << 3)

	byte                word_aligned_bytes[2];
#define changes_seen        word_aligned_bytes[0]
#define CHANGED_ECHO        (1 << 0)
#define CHANGED_NAWS        (1 << 1)
// These happen only once:
#define CHANGED_SGA         (1 << 2)
#define CHANGED_TTYPE       (1 << 3)
#define CHANGED_NEW_ENVIRON (1 << 4)
// The second byte is changed async, by signal handler:
#define got_SIGWINCH        word_aligned_bytes[1]
#define G_changes_or_WINCH  (*(uint16_t*)G.word_aligned_bytes)
// The "shared word" trick is unsafe on only-word-store arches such as DEC Alpha or SHARC,
// but Alpha retired in 2004 and SHARC does not run linux (it's a DSP).

	byte            optstate_ECHO;
// On program start:
#define OPT_ECHO_UNKNOWN 0xff
#define OPT_ECHO_OFF     0
// We operate tty in rawmode only when echo ON (IOW: we saw server say that)
#define OPT_ECHO_ON      1

	byte            echo_sga_response_size;

	IF_VERBOSE(                 unsigned verbose;)
	IF_FEATURE_TELNET_TTYPE(    const char *ttype;)
	IF_FEATURE_TELNET_AUTOLOGIN(const char *autologin;)
	ioloop_state_t  io;
	stdin_to_net_t  conn_stdin2net;
	net_to_stdout_t conn_net2stdout;
	struct termios  termios_def;
	struct termios  termios_raw;
// buf[] arrays in conn structs are conceptually cleaner, but they
// make G.member offsets larger -> larger code
#define BUF_TTY2NET     ((byte*)bb_common_bufsiz1)
#define BUF_NET2TTY     G.buf2
#define BUFSIZE         1024
// Note: can't just increase BUFSIZE arbitrarily: common bufsiz1 is not guaranteed to be >1k!
#define BUFMASK         (BUFSIZE - 1)
	byte            buf2[BUFSIZE];
} FIX_ALIASING;
#define G (*ptr_to_globals)
#define INIT_G() do { \
	SET_PTR_TO_GLOBALS(xzalloc(sizeof(G))); \
} while (0)

static int remaining_free_bytes(int n)
{
	return BUFSIZE - n;
}

#if ENABLE_FEATURE_TELNET_WIDTH
static void handle_SIGWINCH(int sig UNUSED_PARAM)
{
	// Only if server said DO NAWS and seen our WILL NAWS:
	if ((G.flags & (FLAGS_NAWS_ON|INITIAL_SENT)) == (FLAGS_NAWS_ON|INITIAL_SENT))
		G.got_SIGWINCH = 1;
}
#endif

static void rawmode(void)
{
	if (G.flags & DO_TERMIOS)
		tcsetattr(0, TCSADRAIN, &G.termios_raw);
}

static void cookmode(void)
{
	if (G.flags & DO_TERMIOS)
		tcsetattr(0, TCSADRAIN, &G.termios_def);
}

static void doexit(int ev) NORETURN;
static void doexit(int ev)
{
	cookmode();
	exit(ev);
}

static void put_iac(int c)
{
	stdin_to_net_t *conn = &G.conn_stdin2net;
	/* Write directly to stdin2net buffer */
	BUF_TTY2NET[conn->rdidx] = c; /* "... & 0xff" is implicit */
	conn->rdidx = (conn->rdidx + 1) & BUFMASK;
	conn->size++;
}

static void put_iac2_msb_lsb(unsigned x_y)
{
	put_iac(x_y >> 8);  /* "... & 0xff" is implicit */
	put_iac(x_y);  /* "... & 0xff" is implicit */
}
#define put_iac2_x_y(x,y) put_iac2_msb_lsb(((x)<<8) + (y))

#if SUPPORT_FOR_LOCAL_OPTS
static void put_iac4_msb_lsb(unsigned x_y_z_t)
{
	put_iac2_msb_lsb(x_y_z_t >> 16);
	put_iac2_msb_lsb(x_y_z_t);  /* "... & 0xffff" is implicit */
}
#define put_iac4_x_y_z_t(x,y,z,t) \
	put_iac4_msb_lsb(((x)<<24) + ((y)<<16) + ((z)<<8) + (t))

/* Send a byte in subnegotiation, escaping IAC (0xFF) as IAC IAC */
static void put_iac_byte_escaped(int c)
{
	c = (byte)c;
	put_iac(c);
	if (c == IAC)
		put_iac(IAC);
}
#endif

static void put_iac3_IAC_x_y_merged(unsigned wwdd_and_c)
{
	put_iac(IAC);
	put_iac2_msb_lsb(wwdd_and_c);
}
#define put_iac3_IAC_x_y(wwdd,c) \
	put_iac3_IAC_x_y_merged(((wwdd)<<8) + (c))

#if ENABLE_FEATURE_TELNET_TTYPE
static void put_iac_subopt(byte c, const char *str)
{
	put_iac4_x_y_z_t(IAC, SB, c, 0);

	while (*str) {
		put_iac_byte_escaped(*str);
		str++;
	}

	put_iac2_x_y(IAC, SE);
}
#endif

#if ENABLE_FEATURE_TELNET_AUTOLOGIN
static void put_iac_subopt_autologin(const char *p)
{
	put_iac4_x_y_z_t(IAC, SB, TELOPT_NEW_ENVIRON, TELQUAL_IS);
	put_iac4_x_y_z_t(NEW_ENV_VAR, 'U', 'S', 'E'); /* "USER" */
	put_iac2_x_y('R', NEW_ENV_VALUE);

	while (*p)
		put_iac_byte_escaped(*p++);

	put_iac2_x_y(IAC, SE);
}
#endif

#if ENABLE_FEATURE_TELNET_WIDTH
static void put_iac_naws(void)
{
	unsigned width, height;

	put_iac3_IAC_x_y(SB, TELOPT_NAWS);

	/* Send width and height as 16-bit big-endian, escaping IAC bytes */
	get_terminal_width_height(0, &width, &height);
	log1("C:SB NAWS %dx%d", width, height);
	put_iac_byte_escaped(width >> 8);
	put_iac_byte_escaped(width);
	put_iac_byte_escaped(height >> 8);
	put_iac_byte_escaped(height);

	put_iac2_x_y(IAC, SE);
}
#endif

// Telnet option handling strategy:
//
// We send nothing on startup on our own (think "telnet www.kernel.org 80").
// As soon as we see any DO X or WILL X message known to us, we respond
// to them: send DO/DONT ECHO, DO SGA if server talked about them, then send
// WILL NAWS, WILL TTYPE, WILL NEW_ENVIRON
// without waiting for server to advertise those options.
// (Why? Some servers may decide to not advertise all 123 options they know).
//
// TELOPT_ECHO (1) - remote: server echoes our keystrokes back to us
// - Server says: WILL ECHO or WONT ECHO
// - If we don't see it, or see WONT: line mode (local terminal echoes), aka cooked mode
// - When active: We're in character mode (server echoes), aka raw mode
// - Response logic:
// - Server says WILL ECHO: we send DO ECHO, enter raw mode
//   (this is the most usual case for non-ancient servers)
// - Server says WONT ECHO: we send DONT ECHO, enter cooked mode
// - We mirror server's changes, we do allow it to change during session
//   unlike one-shot options which are negotiated once during startup.
// - If user changes mode from ^] menu, we send corresponding DO/DONT ECHO.
//
// TELOPT_SGA (3) - remote: "Suppress Go Ahead", full-duplex
// - Server says: WILL SGA or WONT SGA
// - Response logic: one-shot.
// - We expect all sane servers to choose WILL SGA at startup.
// - If that happens, we send DO SGA. After that, we never react to any
//   further SGA messages (we expect them to not repeat in practice)
//
// TELOPT_NAWS (31) - Window Size - local: we send our window size
// - If server says: DO NAWS, it understands NAWS. We require this before
// - sending SB NAWS ...
// - We send window size during initial handshake and on window resize
//
// TELOPT_TTYPE (24) - Terminal Type - local: we send our terminal type
// - Response logic: one-shot.
// - If server says DO TTYPE: send "IAC SB TTYPE 0 '$TERM' IAC SE"
//
// TELOPT_NEW_ENVIRON (39) - Environment (Autologin) - local: we send our username
// - Response logic: one-shot.
// - If server says DO NEW_ENVIRON: send "IAC SB NEW_ENVIRON IS VAR 'USER' VALUE 'name' IAC SE"
//
// Unknown options:
// Response: NONE - just ignore them
// By protocol definition, an option is not in effect until BOTH sides agree.
// We don't respond to unknown options - they simply won't be in effect.
//
// Testing at discworld.atuin.net:23 reveal servers often do NOT negotiate ECHO/SGA
// in initial handshake while supporting NAWS and TTYPE:
//telnet: S:DO 24 <--TTYPE
//telnet: TTYPE:'xterm-256color' setting CHANGED_TTYPE
//LPmud version : DW OS v1.02 on port 4242.
//telnet: changed:8 flags:8
//telnet: C:SB TTYPE 'xterm-256color'
//telnet: S:DO 31 <--NAWS
//telnet: S:WILL 86
//telnet: S:DO 91
//telnet: S:WILL 70
//telnet: S:WILL 93
//telnet: S:DO 39
//telnet: S:WILL 201
//......text.....
//telnet: changed:2 flags:8
//telnet: C:SB NAWS 226x53
//...text....
//telnet: S:SB 24 <--TTYPE
//...text....
//Setting your network terminal type to "xterm-256color".
//> cols
//Columns currently set to 79.
//> rows
//Rows currently set to 24.
//^^^ server understood TTYPE but not NAWS. ?!
static void handle_changes_in_options(stdin_to_net_t *conn)
{
	int count;

	log1("changed:%x flags:%x", G.changes_seen, G.flags);

	count = remaining_free_bytes(conn->size);
	// As soon as we see any DO/DONT/WILL/WONT known to us,
	// we assume it *is* a telnet server
	// (we are not in "telnet www.kernel.org 80" scenario),
	// and we respond to them, also expressing our own
	// wishes: WILL NAWS etc. We send our WILLs once, all of them.
	// We send actual SB with option contents after WILLs
	// only if that option was requested.
	// We repeatedly send only NAWS (when our window changes).
	// Repeated DO requests are ignored.
	if (G.changes_seen
	 && (count >= G.echo_sga_response_size)
	) {
		if (G.changes_seen & CHANGED_ECHO) {
			// Server said WILL/WONT ECHO - confirm every time
			log1("C:%s ECHO", G.optstate_ECHO ? "DO" : "DONT");
			put_iac3_IAC_x_y(G.optstate_ECHO ? DO : DONT, TELOPT_ECHO);
		}
		if (G.changes_seen & CHANGED_SGA) {
			// Server said WILL SGA - confirm once
			log1("C:DO SGA");
			put_iac3_IAC_x_y(DO, TELOPT_SGA);
			G.flags |= FLAGS_SGA_SEEN; // remember we did it
			G.changes_seen -= CHANGED_SGA;
		}
		G.changes_seen &= ~(CHANGED_ECHO|CHANGED_SGA);

		if (!(G.flags & INITIAL_SENT)) {
			// From now on, we'll only do DO/DONT ECHO and maybe DO SGA
			// in the "if (G.changes_seen)" block.
			G.flags |= INITIAL_SENT;
			G.echo_sga_response_size = (G.flags & FLAGS_SGA_SEEN) ? 3 : 6;

			// Send initial IAC sequences for local options we want to advertise
			// (there may be servers which send DO X only after we say WILL X)
#if ENABLE_FEATURE_TELNET_WIDTH
			log1("C:WILL %s", "NAWS");
			put_iac3_IAC_x_y(WILL, TELOPT_NAWS);
#endif
#if ENABLE_FEATURE_TELNET_TTYPE
			if (G.ttype) {
				log1("C:WILL %s", "TTYPE");
				put_iac3_IAC_x_y(WILL, TELOPT_TTYPE);
			}
#endif
#if ENABLE_FEATURE_TELNET_AUTOLOGIN
			if (G.autologin) {
				log1("C:WILL %s", "NEW_ENVIRON");
				put_iac3_IAC_x_y(WILL, TELOPT_NEW_ENVIRON);
			}
#endif
		}
	}

	// If any of the checks below are true, then we know we
	// went through INITIAL_SENT code path above.
	// Therefore we always send WILL X before SB X...
#if ENABLE_FEATURE_TELNET_WIDTH
	if (remaining_free_bytes(conn->size) > MAX_NAWS_SIZE) {
		if (G.changes_seen & CHANGED_NAWS) {
			G.flags |= FLAGS_NAWS_ON; // remember we did it
			G.changes_seen -= CHANGED_NAWS;
			goto generate_naws;
		}
		// Handle window resize: send updated NAWS
		if (G.got_SIGWINCH) {
			// we know that INITIAL_SENT is set, signal handler checks that
 generate_naws:
			// Clear the flag before put_iac_naws() to avoid race
			G.got_SIGWINCH = 0;
//log1("C:WILL %s", "NAWS, the second time, I'm telling you it's fine!");
//put_iac3_IAC_x_y(WILL, TELOPT_NAWS);
			put_iac_naws();
		}
	}
#endif
#if ENABLE_FEATURE_TELNET_TTYPE
	if ((G.changes_seen & CHANGED_TTYPE)
	 && remaining_free_bytes(conn->size) > 6 + 2 * strlen(G.ttype)
	) {
		log1("C:SB %s '%s'", "TTYPE", G.ttype);
		put_iac_subopt(TELOPT_TTYPE, G.ttype);
		G.ttype = NULL; // remember we did it
		G.changes_seen -= CHANGED_TTYPE;
	}
#endif
#if ENABLE_FEATURE_TELNET_AUTOLOGIN
	if ((G.changes_seen & CHANGED_NEW_ENVIRON)
	 && remaining_free_bytes(conn->size) > 12 + 2 * strlen(G.autologin)
	) {
		log1("C:SB %s '%s'", "NEW_ENVIRON", G.autologin);
		put_iac_subopt_autologin(G.autologin);
		G.autologin = NULL; // remember we did it
		G.changes_seen -= CHANGED_NEW_ENVIRON;
	}
#endif
}

static void announce_rawmode(void)
{
	printf("\r\nEntering %s mode"
		"\r\nEscape character is '^%c'.\r\n", "character", ']'
	);
	rawmode();
}
static void announce_and_switch_to_rawmode(void)
{
	announce_rawmode();
	rawmode();
}
static void announce_and_switch_to_cookmode(void)
{
	printf("\r\nEntering %s mode"
		"\r\nEscape character is '^%c'.\r\n", "line", 'C'
	);
	cookmode();
}

static void show_menu(void)
{
	char b;

	rawmode();

	full_write1_str("\r\nConsole escape. Commands are:\r\n"
			"l	go to line mode\r\n"
			"c	go to character mode\r\n"
			"z	suspend telnet\r\n"
			"e	exit\r\n");
	if (read(STDIN_FILENO, &b, 1) <= 0
	 || b == 'e'
	) {
		doexit(EXIT_FAILURE);
	}

	switch (b) {
	case 'l':
		if (G.optstate_ECHO == OPT_ECHO_ON) { /* are we in rawmode? */
			G.optstate_ECHO = OPT_ECHO_OFF;
			announce_and_switch_to_cookmode();
			goto echo_changed;
		}
		break;
	case 'c':
		if (G.optstate_ECHO != OPT_ECHO_ON) { /* OFF or UNKNOWN? (both operate as linemode) */
			G.optstate_ECHO = OPT_ECHO_ON;
			announce_rawmode(); /* no "_and_switch_": we are already in rawmode */
 echo_changed:
			if (G.flags & INITIAL_SENT)
				G.changes_seen |= CHANGED_ECHO; /* inform the server at next send */
			return;
		}
		break;
	case 'z':
		cookmode();
		kill(0, SIGTSTP);
		rawmode();
		break;
	}

	full_write1_str("continuing...\r\n");

	if (G.optstate_ECHO != OPT_ECHO_ON)
		cookmode();
}

static int have_buffer_to_read_from_stdin(void *this)
{
	stdin_to_net_t *conn = this;
	if (conn->read_fd < 0)
		return 0;
	return conn->size < BUFSIZE;
}

static int read_from_stdin(void *this)
{
	stdin_to_net_t *conn = this;
	int count, rem, expand_count;
	byte *start, *src, *end;
	byte c;

	//if (conn->read_fd < 0)
	//	return 0; /* Already stopped reading stdin */
	//ioloop_run() guarantees it won't call us with fd < 0

	count = BUFSIZE - conn->size;
	count = MIN(BUFSIZE - conn->rdidx, count);
	count /= 2; /* Reserve room for worst-case expansion */
	if (count == 0)
		return 0;

	start = BUF_TTY2NET + conn->rdidx;
	count = safe_read(conn->read_fd, start, count);
	if (count <= 0) {
		conn->read_fd = -1;
		return 0; /* Error or EOF - didn't read anything */
	}

	/* First pass: scan forward counting characters that need expansion */
	src = start;
	expand_count = 0;
	rem = count; /* Remaining bytes to scan */
	do {
		c = *src++;
		if (c == 0x1d) {
			/* Escape character - process bytes before it, then handle escape */
			count -= rem; /* Drop the remaining tail */
#define found_escape (rem != 0)
			break;
		}
		if (c == IAC) {
			expand_count++; /* IAC -> IAC IAC (one extra byte) */
		} else if (c == '\r' || c == '\n') {
			expand_count++; /* \r or \n -> \r\n (one extra byte) */
		}
	} while (--rem != 0);

	if (expand_count != 0) {
		/* Slow path: expand in place working backwards */
		src = start + count - 1; /* Last read byte */
		end = src + expand_count; /* Last byte after expansion */

		/* As soon as src == end, the remaining bytes do not need processing (think about it!) */
		while (src < end) {
			c = *src--;
			if (c == IAC) {
				*end-- = IAC;
			} else if (c == '\r' || c == '\n') {
				*end-- = '\n';
				c = '\r';
			}
			*end-- = c;
		}
		count += expand_count;
	}

	conn->size += count;
	conn->rdidx = (conn->rdidx + count) & BUFMASK;

	if (found_escape)
		show_menu();
#undef found_escape

	return count;
}

static int have_data_to_write_to_net(void *this)
{
	stdin_to_net_t *conn = this;
	if (conn->size == 0 && conn->read_fd < 0) {
		/* buffer drained and stdin EOF - signal EOF to server */
		shutdown(conn->write_fd, SHUT_WR);
		/* Remove this pipe */
		ioloop_remove_conn(conn->io, (connection_t*)conn);
		return -1;
	}
	return conn->size > 0 || G_changes_or_WINCH != 0;
}

static int write_to_net(void *this)
{
	stdin_to_net_t *conn = this;
	int count;

	/* Do we have option or NAWS changes to handle? */
	if (G_changes_or_WINCH)
		handle_changes_in_options(conn); /* yes */

	count = MIN(BUFSIZE - conn->wridx, conn->size);
	// can be zero due to handle_changes_in_options()
	if (count == 0)
		return 0;
	count = safe_write(conn->write_fd, BUF_TTY2NET + conn->wridx, count);
	if (count <= 0) {
		if (count < 0 && errno == EAGAIN)
			return 0;
		full_write1_str("Error writing to foreign host\r\n");
		ioloop_remove_conn(conn->io, (connection_t*)conn);
		return -1; /* "I'm gone" */
	}

	conn->wridx = (conn->wridx + count) & BUFMASK;
	conn->size -= count;
	if (conn->size == 0) {
		conn->rdidx = 0;
		conn->wridx = 0;
	}
	return count;
}

static int have_buffer_to_read_from_net(void *this)
{
	net_to_stdout_t *conn = this;
	if (conn->read_fd < 0)
		return 0;
	return conn->size < BUFSIZE;
}

static int read_from_net(void *this)
{
	net_to_stdout_t *conn = this;
	int count;
	byte *src, *dst;
	byte c;
	byte oldstate_ECHO;

	count = BUFSIZE - conn->size;
	//if (count == 0)
	//	return 0; /* buffer full */
	//ioloop_run() ensures this does not happen
	count = MIN(BUFSIZE - conn->rdidx, count); /* can't be zero */

	/* Read directly into circular buffer's linear fragment */
	dst = BUF_NET2TTY + conn->rdidx;
	count = safe_read(conn->read_fd, dst, count);
	dbg("read_from_net bytes:%d input_state:%d %s",
		count, conn->input_state, bin_to_hex(dst, count > 0 ? count : 0));
	if (count <= 0) {
		if (count < 0 && errno == EAGAIN)
			return 0;
		full_write1_str("EOF on read from foreign host\r\n");
		conn->read_fd = -1;
		/* Imagine scenario: "cat" is running in the server.
		 * We connect and press ^D. Byte 0x04 is transmitted to "cat",
		 * it sees that as EOF and exits.
		 * We see EOF here (netfd read side is closed).
		 * Our stdin is still open, but we have no buffered data to write to net.
		 * ioloop_run() will not _poll_ the netfd for writing.
		 * We will not realize that netfd write side is also closed (!!!)
		 * until something is typed in our stdin and we poll netfd to write that data.
		 */
//FIXME: send a few IAC NOPs to see whether the peer can still receive?
		/* This is a workaround: pretend that our stdin is also closed: */
		G.conn_stdin2net.read_fd = -1;
		/* Unlike just exiting, this will try to send any buffered data */

		return 0;
	}

	/* Copy option states - will be updated during processing, then compared after */
	oldstate_ECHO = G.optstate_ECHO;

	/* Optimization: do not load/store-in-place if unnecessary */
	if (conn->input_state == TS_NORMAL) {
		while (count != 0 && *dst != IAC && *dst != '\r')
			count--, dst++;
	}
	/* Process IAC sequences in place:
	 * - Update option states (G.oldstate_XYZ)
	 * - Decode/remove IAC sequences, compacting the data
	 * - src = read pointer, dst = write pointer (for compaction)
	 */
	src = dst;
	while (--count >= 0) {
		c = *src++;

		switch (conn->input_state) {
			int will;
		case TS_NORMAL:
 normal:
			if (c == IAC) {
				SET_INPUT_STATE(conn, TS_IAC, c);
				continue;
			}
			if (c == '\r') {
				SET_INPUT_STATE(conn, TS_CR, c);
			}
			*dst++ = c;
			continue;

		case TS_CR:
			SET_INPUT_STATE(conn, TS_NORMAL, c);
			if (c == '\0') /* Skip NUL after CR (telnet EOL: CR NUL) */
				continue;
			goto normal;

		case TS_IAC:
			if (c == IAC) {
				/* IAC IAC -> single IAC */
				*dst++ = c;
				SET_INPUT_STATE(conn, TS_NORMAL, c);
			} else if (c == SB) {
				conn->negotiation_verb = 0xff; /* reuse as counter */
				SET_INPUT_STATE(conn, TS_SUB1, c);
				log1("S:SB %d", count ? *src : 0);
			} else if (c == WILL || c == WONT || c == DO || c == DONT) { /* 251-254 */
				conn->negotiation_verb = c;
				SET_INPUT_STATE(conn, TS_OPT, c);
			} else {
				/* Unknown IAC command, ignore */
				SET_INPUT_STATE(conn, TS_NORMAL, c);
			}
			break;

		case TS_OPT:
#if VERBOSE
			{
				static const char verbs[] ALIGN1 = "WILL\0""WONT\0""DO\0""DONT";
				log1("S:%s %d", nth_string(verbs, conn->negotiation_verb - WILL), c);
			}
#endif
			/* Process option negotiation */
			will = (conn->negotiation_verb == WILL);
			if (will || conn->negotiation_verb == WONT) {
				switch (c) {
				case TELOPT_ECHO: /* Remote option: server echoes our typing */
					G.optstate_ECHO = will;
					break;
				case TELOPT_SGA: /* Remote option: "suppress go ahead" */
					if (will && !(G.flags & FLAGS_SGA_SEEN))
						G.changes_seen |= CHANGED_SGA;
					break;
				}
			} else if (conn->negotiation_verb == DO) {
				switch (c) {
#if ENABLE_FEATURE_TELNET_TTYPE
				case TELOPT_TTYPE: /* Local option: we send terminal type */
					log1("TTYPE:'%s' %ssetting CHANGED_TTYPE", G.ttype, G.ttype ? "" : "not ");
					if (G.ttype)
						G.changes_seen |= CHANGED_TTYPE;
					break;
#endif
#if ENABLE_FEATURE_TELNET_AUTOLOGIN
				case TELOPT_NEW_ENVIRON: /* Local option: we send username */
					if (G.autologin)
						G.changes_seen |= CHANGED_NEW_ENVIRON;
					break;
#endif
#if ENABLE_FEATURE_TELNET_WIDTH
				case TELOPT_NAWS: /* Local option: we send window size */
					if (!(G.flags & FLAGS_NAWS_ON))
						G.changes_seen |= CHANGED_NAWS;
					break;
#endif
				}
			}
			/* else: "DONT <something>": ignore */
			SET_INPUT_STATE(conn, TS_NORMAL, c);
			break;

		case TS_SUB1:
			/* Avoid being stuck in TS_SUB1 forever (with detours into TS_SUB2)
			 * if IAC SE is never seen (buggy server response?).
			 */
			if (--conn->negotiation_verb == 0) {
				log1("unterminated SB (server bug?)");
				SET_INPUT_STATE(conn, TS_NORMAL, c);
			} else
			/* Skip over subnegotiation bytes until we see IAC */
			if (c == IAC) {
				SET_INPUT_STATE(conn, TS_SUB2, c);
			}
			break;

		case TS_SUB2:
			/* After IAC in subnegotiation, check for SE */
			if (c == SE) {
				/* End of subnegotiation */
				SET_INPUT_STATE(conn, TS_NORMAL, c);
			} else {
				/* IAC followed by something other than SE, back to SUB1 */
				SET_INPUT_STATE(conn, TS_SUB1, c);
			}
			break;
		}
	}

	if (oldstate_ECHO != G.optstate_ECHO) {
		/* Tell net writer to generate a confirmation */
		G.changes_seen |= CHANGED_ECHO;
		/* Print the banner and set termios */
		if (G.optstate_ECHO == OPT_ECHO_ON)
			announce_and_switch_to_rawmode();
		else
			announce_and_switch_to_cookmode();
	}

	/* Update circular buffer: only the compacted data */
	count = dst - (BUF_NET2TTY + conn->rdidx);
	conn->size += count;
	conn->rdidx = (conn->rdidx + count) & BUFMASK;

	dbg("read_from_net: user bytes:%d input_state:%d", count, conn->input_state);

	return count;
}

static int have_data_to_write_to_stdout(void *this)
{
	net_to_stdout_t *conn = this;
	if (conn->size == 0 && conn->read_fd < 0) {
		/* buffer drained and network read EOF */
		/* Remove this pipe */
		ioloop_remove_conn(conn->io, (connection_t*)conn);
		return -1;
	}
	return conn->size > 0;
}

static int write_to_stdout(void *this)
{
	net_to_stdout_t *conn = this;
	int wr = MIN(BUFSIZE - conn->wridx, conn->size);
	ssize_t count;

	dbg("write_to_stdout: wr:%d %s", wr, bin_to_hex(BUF_NET2TTY + conn->wridx, wr));
	count = safe_write(conn->write_fd, BUF_NET2TTY + conn->wridx, wr);
	if (count <= 0) {
		if (count < 0 && errno == EAGAIN)
			return 0;
		//full_write1_str("Error writing to stdout\r\n");
		ioloop_remove_conn(conn->io, (connection_t*)conn);
		return -1; /* "I'm gone" */
	}

	conn->wridx = (conn->wridx + count) & BUFMASK;
	conn->size -= count;
	if (conn->size == 0) {
		conn->rdidx = 0;
		conn->wridx = 0;
	}
	return count;
}

int telnet_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int telnet_main(int argc UNUSED_PARAM, char **argv)
{
	char *host;
	int port;
	int opts;

	INIT_G();

#if ENABLE_FEATURE_TELNET_TTYPE
	G.ttype = getenv("TERM");
#endif
	opts = getopt32(argv, ""IF_VERBOSE("^")
		IF_FEATURE_TELNET_AUTOLOGIN("al:")IF_VERBOSE("v")
		IF_VERBOSE("\0" "vv")
		IF_FEATURE_TELNET_AUTOLOGIN(, &G.autologin)
		IF_VERBOSE(, &G.verbose)
	);
#if ENABLE_FEATURE_TELNET_AUTOLOGIN
	if ((opts & 3) == 1) {
		// -a without -l USER picks USER from envvar:
		G.autologin = getenv("USER");
	}
#endif
	argv += optind;
	if (!*argv)
		bb_show_usage();
	host = *argv++;
	port = *argv ? bb_lookup_port(*argv++, "tcp", 23)
		: bb_lookup_std_port("telnet", "tcp", 23);
	if (*argv) // extra params??
		bb_show_usage();

	// Save our termios
	if (tcgetattr(0, &G.termios_def) >= 0) {
		G.flags |= DO_TERMIOS;
		G.termios_raw = G.termios_def;
		cfmakeraw(&G.termios_raw);
	}

	xmove_fd(create_and_connect_stream_or_die(host, port), netfd);
	printf("Connected to %s\n", host);

	setsockopt_keepalive(netfd);
	ndelay_on(netfd);

#if ENABLE_FEATURE_TELNET_WIDTH
	signal(SIGWINCH, handle_SIGWINCH);
#endif
	signal(SIGINT, record_signo);
	// Without this, SIGPIPE was seen on loopback connections:
	signal(SIGPIPE, SIG_IGN);

	// Initialize connections
	G.conn_stdin2net.have_buffer_to_read_into = have_buffer_to_read_from_stdin;
	G.conn_stdin2net.have_data_to_write = have_data_to_write_to_net;
	G.conn_stdin2net.read = read_from_stdin;
	G.conn_stdin2net.write = write_to_net;
	if (STDIN_FILENO != 0)
		G.conn_stdin2net.read_fd = STDIN_FILENO;
	G.conn_stdin2net.write_fd = netfd;

	G.conn_net2stdout.have_buffer_to_read_into = have_buffer_to_read_from_net;
	G.conn_net2stdout.have_data_to_write = have_data_to_write_to_stdout;
	G.conn_net2stdout.read = read_from_net;
	G.conn_net2stdout.write = write_to_stdout;
	G.conn_net2stdout.read_fd = netfd;
	G.conn_net2stdout.write_fd = STDOUT_FILENO;
	if (TS_NORMAL != 0)
		G.conn_net2stdout.input_state = TS_NORMAL;

	ioloop_insert_conn(&G.io, (connection_t*)&G.conn_stdin2net);
	ioloop_insert_conn(&G.io, (connection_t*)&G.conn_net2stdout);

	G.echo_sga_response_size = 3 * (2
		IF_FEATURE_TELNET_WIDTH( + 1)
		IF_FEATURE_TELNET_TTYPE(+ !!G.ttype)
		IF_FEATURE_TELNET_AUTOLOGIN(+ !!G.autologin)
	);
	// Make *any* ECHO negotiation from server, positive or negative,
	// trigger "we have a change" logic:
	G.optstate_ECHO = OPT_ECHO_UNKNOWN;
#if DEBUG || VERBOSE
	// Terminal can change to raw mode, fix line printing
	msg_eol = "\r\n";
#endif
	// EINTR flag and looping is only needed to handle ^C
	// in line mode, otherwise just a call to ioloop_run() would do.
	// TODO: replace primitive line mode with read_line_input()!!!
	G.io.flags |= IOLOOP_FLAG_EXIT_IF_EINTR;
	for (;;) {
		int rc = ioloop_run(&G.io);
		if (rc == IOLOOP_NO_CONNS) {
			dbg("connection is closed");
			break;
		}
		if (bb_got_signal /*&& rc == IOLOOP_EINTR*/) {
			bb_got_signal = 0;
			show_menu();
		}
	}

	doexit(EXIT_SUCCESS);
}
