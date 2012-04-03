/* vi: set sw=4 ts=4: */
/*
 * Which implementation for busybox
 *
 * Copyright (C) 1999-2004 by Erik Andersen <andersen@codepoet.org>
 * Copyright (C) 2006 Gabriel Somlo <somlo at cmu.edu>
 *
 * Licensed under GPLv2 or later, see file LICENSE in this source tree.
 *
 * Based on which from debianutils
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

#if ENABLE_PLATFORM_MINGW32
static char *win32_execable_file(const char *p)
{
	char *path;
	int len = strlen(p) + 5;

	if ( (path=malloc(len)) != NULL ) {
		memcpy(path, p, len);
		memcpy(path+len, ".exe", 5);
		if (execable_file(path)) {
			return path;
		}
		memcpy(path+len, ".com", 5);
		if (execable_file(path)) {
			return path;
		}
		free(path);
	}

	return NULL;
}
#endif

int which_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int which_main(int argc UNUSED_PARAM, char **argv)
{
	IF_DESKTOP(int opt;)
	int status = EXIT_SUCCESS;
	char *path;
	char *p;

	opt_complementary = "-1"; /* at least one argument */
	IF_DESKTOP(opt =) getopt32(argv, "a");
	argv += optind;

	/* This matches what is seen on e.g. ubuntu.
	 * "which" there is a shell script. */
	path = getenv("PATH");
	if (!path) {
		path = (char*)bb_PATH_root_path;
		putenv(path);
		path += 5; /* skip "PATH=" */
	}

	do {
#if ENABLE_FEATURE_PREFER_APPLETS
		if ( find_applet_by_name(*argv) >= 0 ) {
			puts(*argv);
			IF_DESKTOP(if ( !opt ))
				continue;
		}
#endif

#if ENABLE_DESKTOP
/* Much bloat just to support -a */
		if (strchr(*argv, '/') || (ENABLE_PLATFORM_MINGW32 && strchr(*argv, '\\'))) {
			if (execable_file(*argv)) {
				puts(*argv);
				continue;
			}
#if ENABLE_PLATFORM_MINGW32
			else if ((p=win32_execable_file(*argv)) != NULL) {
				puts(p);
				free(p);
				continue;
			}
#endif
			status = EXIT_FAILURE;
		} else {
			char *path2 = xstrdup(path);
			char *tmp = path2;

			p = find_execable(*argv, &tmp);
			if (!p)
				status = EXIT_FAILURE;
			else {
 print:
				puts(p);
				free(p);
				if (opt) {
					/* -a: show matches in all PATH components */
					if (tmp) {
						p = find_execable(*argv, &tmp);
						if (p)
							goto print;
					}
				}
			}
			free(path2);
		}
#else
/* Just ignoring -a */
		if (strchr(*argv, '/') || (ENABLE_PLATFORM_MINGW32 && strchr(*argv, '\\'))) {
			if (execable_file(*argv)) {
				puts(*argv);
				continue;
			}
#if ENABLE_PLATFORM_MINGW32
			else if ((p=win32_execable_file(*argv)) != NULL) {
				puts(p);
				free(p);
				continue;
			}
#endif
		} else {
			char *path2 = xstrdup(path);
			char *tmp = path2;
			p = find_execable(*argv, &tmp);
			free(path2);
			if (p) {
				puts(p);
				free(p);
				continue;
			}
		}
		status = EXIT_FAILURE;
#endif
	} while (*(++argv) != NULL);

	fflush_stdout_and_exit(status);
}
