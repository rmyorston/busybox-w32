/* vi: set sw=4 ts=4: */
/*
 * lsattr.c		- List file attributes on an ext2 file system
 *
 * Copyright (C) 1993, 1994  Remy Card <card@masi.ibp.fr>
 *                           Laboratoire MASI, Institut Blaise Pascal
 *                           Universite Pierre et Marie Curie (Paris VI)
 *
 * This file can be redistributed under the terms of the GNU General
 * Public License
 */
//config:config LSATTR
//config:	bool "lsattr (5.5 kb)"
//config:	default y
//config:	help
//config:	lsattr lists the file attributes on a second extended file system.

//applet:IF_LSATTR(APPLET_NOEXEC(lsattr, lsattr, BB_DIR_BIN, BB_SUID_DROP, lsattr))
/* ls is NOEXEC, so we should be too! ;) */

//kbuild:lib-$(CONFIG_LSATTR) += lsattr.o e2fs_lib.o

//usage:#define lsattr_trivial_usage
//usage:     IF_NOT_PLATFORM_MINGW32(
//usage:       "[-Radlv] [FILE]..."
//usage:     )
//usage:     IF_PLATFORM_MINGW32(
//usage:       "[-Radl] [FILE]..."
//usage:     )
//usage:#define lsattr_full_usage "\n\n"
//usage:       "List ext2 file attributes\n"
//usage:     "\n	-R	Recurse"
//usage:     "\n	-a	Don't hide entries starting with ."
//usage:     "\n	-d	List directory entries instead of contents"
//usage:     "\n	-l	List long flag names"
//usage:     IF_NOT_PLATFORM_MINGW32(
//usage:     "\n	-v	List version/generation number"
//usage:     )

#include "libbb.h"
#include "e2fs_lib.h"

enum {
	OPT_RECUR      = 0x1,
	OPT_ALL        = 0x2,
	OPT_DIRS_OPT   = 0x4,
	OPT_PF_LONG    = 0x8,
	OPT_GENERATION = 0x10,
};

static void list_attributes(const char *name)
{
	unsigned long fsflags;
#if !ENABLE_PLATFORM_MINGW32
	unsigned long generation;
#endif

	if (fgetflags(name, &fsflags) != 0)
		goto read_err;

#if !ENABLE_PLATFORM_MINGW32
	if (option_mask32 & OPT_GENERATION) {
		if (fgetversion(name, &generation) != 0)
			goto read_err;
		printf("%5lu ", generation);
	}
#endif

	if (option_mask32 & OPT_PF_LONG) {
		printf("%-28s ", name);
		print_e2flags(stdout, fsflags, PFOPT_LONG);
		bb_putchar('\n');
	} else {
		print_e2flags(stdout, fsflags, 0);
		printf(" %s\n", name);
	}

	return;
 read_err:
	bb_perror_msg("reading %s", name);
}

static int FAST_FUNC lsattr_dir_proc(const char *dir_name,
		struct dirent *de,
		void *private UNUSED_PARAM)
{
	struct stat st;
	char *path;

	path = concat_path_file(dir_name, de->d_name);

	if (lstat(path, &st) != 0)
		bb_perror_msg("stat %s", path);
	else if (de->d_name[0] != '.' || (option_mask32 & OPT_ALL)) {
		list_attributes(path);
		if (S_ISDIR(st.st_mode) && (option_mask32 & OPT_RECUR)
		 && !DOT_OR_DOTDOT(de->d_name)
		) {
			printf("\n%s:\n", path);
			iterate_on_dir(path, lsattr_dir_proc, NULL);
			bb_putchar('\n');
		}
	}

	free(path);
	return 0;
}

static void lsattr_args(const char *name)
{
	struct stat st;

	if (lstat(name, &st) == -1) {
		bb_perror_msg("stat %s", name);
	} else if (S_ISDIR(st.st_mode) && !(option_mask32 & OPT_DIRS_OPT)) {
		iterate_on_dir(name, lsattr_dir_proc, NULL);
	} else {
		list_attributes(name);
	}
}

int lsattr_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int lsattr_main(int argc UNUSED_PARAM, char **argv)
{
#if ENABLE_PLATFORM_MINGW32
	getopt32(argv, "Radl");
#else
	getopt32(argv, "Radlv");
#endif
	argv += optind;

	if (!*argv)
		*--argv = (char*)".";
	do lsattr_args(*argv++); while (*argv);

	return EXIT_SUCCESS;
}
