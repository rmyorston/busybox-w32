/* vi: set sw=4 ts=4: */
/*
 * Copyright (C) 1999-2004 by Erik Andersen <andersen@codepoet.org>
 * Copyright (C) 2006 Gabriel Somlo <somlo at cmu.edu>
 *
 * Licensed under GPLv2 or later, see file LICENSE in this source tree.
 */

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
	const char *env_path;
	int status = 0;

	env_path = getenv("PATH");
	if (!env_path)
		env_path = bb_default_root_path;

	opt_complementary = "-1"; /* at least one argument */
	getopt32(argv, "a");
	argv += optind;

	do {
		int missing = 1;
		char *p;

#if ENABLE_FEATURE_PREFER_APPLETS
		if ( find_applet_by_name(*argv) >= 0 ) {
			missing = 0;
			puts(*argv);
			if (!option_mask32) /* -a not set */
				break;
		}
#endif

		/* If file contains a slash don't use PATH */
		if (strchr(*argv, '/') || (ENABLE_PLATFORM_MINGW32 && strchr(*argv, '\\'))) {
			if (file_is_executable(*argv)) {
				missing = 0;
				puts(*argv);
			}
#if ENABLE_PLATFORM_MINGW32
			else if ((p=file_is_win32_executable(*argv)) != NULL) {
				missing = 0;
				puts(p);
				free(p);
			}
#endif
		} else {
			char *path;
			char *tmp;

			path = tmp = xstrdup(env_path);
			while ((p = find_executable(*argv, &tmp)) != NULL) {
				missing = 0;
				puts(p);
				free(p);
				if (!option_mask32) /* -a not set */
					break;
			}
			free(path);
		}
		status |= missing;
	} while (*++argv);

	return status;
}
