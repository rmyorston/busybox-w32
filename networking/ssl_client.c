/*
 * Copyright (C) 2017 Denys Vlasenko
 *
 * Licensed under GPLv2, see file LICENSE in this source tree.
 */
//config:config SSL_CLIENT
//config:	bool "ssl_client (28 kb)"
//config:	default y
//config:	select TLS
//config:	help
//config:	This tool pipes data to/from a socket, TLS-encrypting it.

//applet:IF_SSL_CLIENT(APPLET(ssl_client, BB_DIR_USR_BIN, BB_SUID_DROP))

//kbuild:lib-$(CONFIG_SSL_CLIENT) += ssl_client.o

//usage:#define ssl_client_trivial_usage
//usage:    IF_NOT_PLATFORM_MINGW32(
//usage:       "[-n SNI] { -s FD [-r FD] | HOST | -e PROG ARGS }"
//usage:    )
//usage:    IF_PLATFORM_MINGW32(
//usage:       "[-e] -h handle [-n SNI]"
//usage:    )
//usage:#define ssl_client_full_usage ""

#include "libbb.h"

int ssl_client_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int ssl_client_main(int argc UNUSED_PARAM, char **argv)
{
	int exit_if_stdin_closed;
	tls_state_t *tls;
	const char *sni = NULL;
	int opt;
#if ENABLE_PLATFORM_MINGW32
	char *hstr = NULL;
	HANDLE h;
	enum {
		/* wrong name so exit_if_stdin_closed is set later */
		OPT_s = (1 << 0),
		OPT_h = (1 << 1),
		OPT_n = (1 << 2),
	};
#else
	enum {
		OPT_s = (1 << 0),
		OPT_r = (1 << 1),
		OPT_n = (1 << 2),
		OPT_e = (1 << 3),
	};
#endif

	// INIT_G();
	tls = new_tls_state();
#if ENABLE_PLATFORM_MINGW32
	opt = getopt32(argv, "eh:n:", &hstr, &sni);

	if (!hstr || sscanf(hstr, "%p", &h) != 1)
		bb_error_msg_and_die("invalid handle");
	init_winsock();
	tls->ifd = tls->ofd = _open_osfhandle((intptr_t)h, _O_RDWR|_O_BINARY);
#else
	/* "+": stop on first non-option */
	opt = getopt32(argv, "^+" "s:+r:+n:e" "\0"
		"e--s:e--r:s--e:r--e", &tls->ofd, &tls->ifd, &sni
	);
	argv += optind;

	if (opt & OPT_e) {
		/* -e PROG: run PROG and talk TLS to its stdin/stdout */
		// Talk to local HTTP server behind local TLS server:
		// printf "GET / HTTP/1.1\r\n\r\n" | ssl_client -e ssl_server -d PRIVKEY.der -e httpd -i
		struct fd_pair to_prog;
		struct fd_pair from_prog;

		pid_t pid;

		if (!argv[0])
			bb_show_usage();

		xpiped_pair(to_prog);
		xpiped_pair(from_prog);

		pid = xvfork();
		if (pid == 0) {
			/* Child: run the program */

			/* NB: close _first_, then move fds! */
			close(to_prog.wr);
			close(from_prog.rd);
			xmove_fd(to_prog.rd, STDIN_FILENO);
			xmove_fd(from_prog.wr, STDOUT_FILENO);

			BB_EXECVP_or_die(argv);
		}

		/* Parent: close child ends of pipes */
		close(to_prog.rd);
		close(from_prog.wr);

		tls->ofd = to_prog.wr;   /* write to program's stdin */
		tls->ifd = from_prog.rd; /* read from program's stdout */

	} else if (!(opt & (OPT_s|OPT_r))) {
		/* Not -e/-s/-r: connect to HOST */
		// Talk to kernel.org:
		// printf "GET / HTTP/1.1\r\nHost: kernel.org\r\n\r\n" | ssl_client kernel.org
		if (!argv[0] || argv[1])
			bb_show_usage();
		if (!sni)
			sni = argv[0];
		tls->ifd = tls->ofd = create_and_connect_stream_or_die(argv[0], 443);

	} else {
		/* -s FD [-r FD] */
		if (!(opt & OPT_s) || argv[0])
			bb_show_usage();
		if (!(opt & OPT_r)) {
			/* -r FD defaults to -s FD */
			tls->ifd = tls->ofd;
		}
	}
#endif

	tls_handshake(tls, sni);

	exit_if_stdin_closed = (opt & OPT_s) ? TLSLOOP_EXIT_ON_LOCAL_EOF : 0;
	tls_run_copy_loop(tls, /*flags*/ exit_if_stdin_closed);

	return EXIT_SUCCESS;
}
