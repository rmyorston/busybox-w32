/*
 * Copyright (C) 2017 Denys Vlasenko
 *
 * Licensed under GPLv2, see file LICENSE in this source tree.
 */
//config:config SSL_CLIENT
//config:	bool "ssl_client (25 kb)"
//config:	default y
//config:	select TLS
//config:	help
//config:	This tool pipes data to/from a socket, TLS-encrypting it.

//applet:IF_SSL_CLIENT(APPLET(ssl_client, BB_DIR_USR_BIN, BB_SUID_DROP))

//kbuild:lib-$(CONFIG_SSL_CLIENT) += ssl_client.o

//usage:#define ssl_client_trivial_usage
//usage:    IF_NOT_PLATFORM_MINGW32(
//usage:       "[-e] -s FD [-r FD] [-n SNI]"
//usage:    )
//usage:    IF_PLATFORM_MINGW32(
//usage:       "[-e] -h handle [-n SNI]"
//usage:    )
//usage:#define ssl_client_full_usage ""

#include "libbb.h"

int ssl_client_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int ssl_client_main(int argc UNUSED_PARAM, char **argv)
{
	tls_state_t *tls;
	const char *sni = NULL;
	int opt;
#if ENABLE_PLATFORM_MINGW32
	char *hstr = NULL;
	HANDLE h;
#endif

	// INIT_G();

	tls = new_tls_state();
#if !ENABLE_PLATFORM_MINGW32
	opt = getopt32(argv, "es:+r:+n:", &tls->ofd, &tls->ifd, &sni);
	if (!(opt & (1<<2))) {
		/* -r N defaults to -s N */
		tls->ifd = tls->ofd;
	}
#else
	opt = getopt32(argv, "eh:n:", &hstr, &sni);
#endif

	if (!(opt & (3<<1))) {
		if (!argv[1])
			bb_show_usage();
		/* Undocumented debug feature: without -s and -r, takes HOST arg and connects to it */
		//
		// Talk to kernel.org:
		// printf "GET / HTTP/1.1\r\nHost: kernel.org\r\n\r\n" | busybox ssl_client kernel.org
		if (!sni)
			sni = argv[1];
		tls->ifd = tls->ofd = create_and_connect_stream_or_die(argv[1], 443);
	}
#if ENABLE_PLATFORM_MINGW32
	else {
		if (!hstr || sscanf(hstr, "%p", &h) != 1)
			bb_error_msg_and_die("invalid handle");
		tls->ifd = tls->ofd = _open_osfhandle((intptr_t)h, _O_RDWR|_O_BINARY);
	}
#endif

	tls_handshake(tls, sni);

	BUILD_BUG_ON(TLSLOOP_EXIT_ON_LOCAL_EOF != 1);
	tls_run_copy_loop(tls, /*flags*/ opt & 1);

	return EXIT_SUCCESS;
}
