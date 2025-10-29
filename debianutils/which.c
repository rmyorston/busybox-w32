/* vi: set sw=4 ts=4: */
/*
 * Copyright (C) 1999-2004 by Erik Andersen <andersen@codepoet.org>
 * Copyright (C) 2006 Gabriel Somlo <somlo at cmu.edu>
 *
 * Licensed under GPLv2 or later, see file LICENSE in this source tree.
 */
//config:config WHICH
//config:	bool "which (4 kb)"
//config:	default y
//config:	help
//config:	which is used to find programs in your PATH and
//config:	print out their pathnames.

// NOTE: For WIN32 this applet is NOEXEC as file_is_win32_exe() and
//       find_executable() both allocate memory.

//applet:IF_PLATFORM_MINGW32(
//applet:IF_WHICH(APPLET_NOEXEC(which, which, BB_DIR_USR_BIN, BB_SUID_DROP, which))
//applet:)
//applet:IF_PLATFORM_POSIX(
//applet:IF_WHICH(APPLET_NOFORK(which, which, BB_DIR_USR_BIN, BB_SUID_DROP, which))
//applet:)

//kbuild:lib-$(CONFIG_WHICH) += which.o

//usage:#define which_trivial_usage
//usage:       "[-a] COMMAND..."
//usage:#define which_full_usage "\n\n"
//usage:       "Locate COMMAND\n"
//usage:     "\n	-a	Show all matches"
//usage:
//usage:#define which_example_usage
//usage:       "$ which login\n"
//usage:       "/bin/login\n"

#include "libbb.h"

int which_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int which_main(int argc UNUSED_PARAM, char **argv)
{
	const char *env_path;
	int status = 0;
#if ENABLE_PLATFORM_MINGW32 && ENABLE_FEATURE_SH_STANDALONE
	/* 'Which' in argv[0] indicates we were run from a standalone shell */
	int sh_standalone = argv[0][0] == 'W';
#endif

	env_path = getenv("PATH");
	if (!env_path)
		env_path = bb_default_root_path;

	getopt32(argv, "^" "a" "\0" "-1"/*at least one arg*/);
	argv += optind;

	do {
		int missing = 1;

#if ENABLE_PLATFORM_MINGW32 && ENABLE_FEATURE_SH_STANDALONE
		if (sh_standalone && find_applet_by_name(*argv) >= 0) {
			missing = 0;
			puts(applet_to_exe(*argv));
			if (!option_mask32) /* -a not set */
				break;
		}
#endif

#if !ENABLE_PLATFORM_MINGW32
		/* If file contains a slash don't use PATH */
		if (strchr(*argv, '/')) {
			if (file_is_executable(*argv)) {
				missing = 0;
				puts(*argv);
			}
#else
		if (has_path(*argv)) {
			char *path = file_is_win32_exe(*argv);

			if (path) {
				missing = 0;
				puts(bs_to_slash(path));
			}
			else if (unix_path(*argv)) {
				const char *name = bb_basename(*argv);
# if ENABLE_FEATURE_SH_STANDALONE
				if (sh_standalone && find_applet_by_name(name) >= 0) {
					missing = 0;
					puts(name);
				} else
# endif
				{
					argv[0] = (char *)name;
					goto try_PATH;
				}
			}
#endif
		} else {
			const char *path;
			char *p;

#if ENABLE_PLATFORM_MINGW32
 try_PATH:
#endif
			path = env_path;
			/* NOFORK NB: xmalloc inside find_executable(), must have no allocs above! */
			while ((p = find_executable(*argv, &path)) != NULL) {
				missing = 0;
#if ENABLE_PLATFORM_MINGW32
				puts(bs_to_slash(p));
#else
				puts(p);
#endif
				free(p);
				if (!option_mask32) /* -a not set */
					break;
			}
		}
		status |= missing;
	} while (*++argv);

	return status;
}
