/*
 * directory junction creation for busybox
 *
 * Copyright (C) 2017 Denys Vlasenko <vda.linux@googlemail.com>
 * Copyright (C) 2022 Ron Yorston <rmy@pobox.com>
 *
 * Licensed under GPLv2, see file LICENSE in this source tree.
 */
//config:config JN
//config:	bool "jn (3.2 kb)"
//config:	default y
//config:	depends on PLATFORM_MINGW32
//config:	help
//config:	Creates a directory junction.

//applet:IF_JN(APPLET_NOEXEC(jn, jn, BB_DIR_USR_BIN, BB_SUID_DROP, jn))

//kbuild:lib-$(CONFIG_JN) += jn.o

//usage:#define jn_trivial_usage
//usage:       "DIR JUNC"
//usage:#define jn_full_usage "\n\n"
//usage:       "Create directory junction JUNC to DIR"

#include "libbb.h"

int jn_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int jn_main(int argc UNUSED_PARAM, char **argv)
{
	getopt32(argv, "^" "" "\0" "=2");
	argv += optind;
	if (create_junction(argv[0], argv[1]) != 0) {
		bb_perror_msg_and_die("can't create junction '%s' to '%s'",
			argv[1], argv[0]);
	}
	return EXIT_SUCCESS;
}
