/* vi: set sw=4 ts=4: */
/*
 * Copyright (C) 1999-2004 by Erik Andersen <andersen@codepoet.org>
 * Copyright (C) 2006 Gabriel Somlo <somlo at cmu.edu>
 *
 * Licensed under GPLv2 or later, see file LICENSE in this source tree.
 */
//config:config WHICH
//config:	bool "which (3.8 kb)"
//config:	default y
//config:	help
//config:	which is used to find programs in your PATH and
//config:	print out their pathnames.

//applet:IF_WHICH(APPLET_NOFORK(which, which, BB_DIR_USR_BIN, BB_SUID_DROP, which))

//kbuild:lib-$(CONFIG_WHICH) += which.o

//usage:#define which_trivial_usage
//usage:       "[COMMAND]..."
//usage:#define which_full_usage "\n\n"
//usage:       "Locate a COMMAND"
//usage:
//usage:#define which_example_usage
//usage:       "$ which login\n"
//usage:       "/bin/login\n"

#include "libbb.h"

int which_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int which_main(int argc UNUSED_PARAM, char **argv)
{
	char *env_path;
	int status = 0;
	/* This sizeof(): bb_default_root_path is shorter than BB_PATH_ROOT_PATH */
	char buf[sizeof(BB_PATH_ROOT_PATH)];

	env_path = getenv("PATH");
	if (!env_path)
		/* env_path must be writable, and must not alloc, so... */
		env_path = strcpy(buf, bb_default_root_path);

	getopt32(argv, "^" "a" "\0" "-1"/*at least one arg*/);
	argv += optind;

	do {
		int missing = 1;
		char *p;

#if ENABLE_FEATURE_SH_STANDALONE
		if (strcmp(*argv, "busybox") == 0 &&
				is_suffixed_with(bb_busybox_exec_path, "busybox.exe")) {
			missing = 0;
			puts(bb_busybox_exec_path);
			if (!option_mask32) /* -a not set */
				break;
		}
		else if (find_applet_by_name(*argv) >= 0 ||
				is_prefixed_with(*argv, "busybox")) {
			missing = 0;
			puts(*argv);
			if (!option_mask32) /* -a not set */
				break;
		}
#endif

		/* If file contains a slash don't use PATH */
		if (strchr(*argv, '/') || (ENABLE_PLATFORM_MINGW32 && strchr(*argv, '\\'))) {
#if ENABLE_PLATFORM_MINGW32
			if ((p=auto_win32_extension(*argv)) != NULL) {
				missing = 0;
				bs_to_slash(p);
				puts(p);
			}
			else
#endif
			if (file_is_executable(*argv)) {
				missing = 0;
#if ENABLE_PLATFORM_MINGW32
				p = auto_string(xstrdup(*argv));
				bs_to_slash(p);
				puts(p);
#else
				puts(*argv);
#endif
			}
		} else {
			char *path;

			path = env_path;
			/* NOFORK NB: xmalloc inside find_executable(), must have no allocs above! */
			while ((p = find_executable(*argv, &path)) != NULL) {
				missing = 0;
#if ENABLE_PLATFORM_MINGW32
				bs_to_slash(p);
#endif
				puts(p);
				free(p);
				if (!option_mask32) /* -a not set */
					break;
			}
		}
		status |= missing;
	} while (*++argv);

	return status;
}
