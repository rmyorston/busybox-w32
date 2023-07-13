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
//config:	bool "lsattr (5.7 kb)"
//config:	default y
//config:	help
//config:	lsattr lists the file attributes on a second extended file system.

//applet:IF_LSATTR(APPLET_NOEXEC(lsattr, lsattr, BB_DIR_BIN, BB_SUID_DROP, lsattr))
/* ls is NOEXEC, so we should be too! ;) */

//kbuild:lib-$(CONFIG_LSATTR) += lsattr.o e2fs_lib.o

//usage:#define lsattr_trivial_usage
//usage:     IF_NOT_PLATFORM_MINGW32(
//usage:       "[-Radlpv] [FILE]..."
//usage:     )
//usage:     IF_PLATFORM_MINGW32(
//usage:       "[-Radl] [FILE]..."
//usage:     )
//usage:#define lsattr_full_usage "\n\n"
//usage:     IF_NOT_PLATFORM_MINGW32(
//usage:       "List ext2 file attributes\n"
//usage:     )
//usage:     IF_PLATFORM_MINGW32(
//usage:       "List file attributes\n"
//usage:     )
//usage:     "\n	-R	Recurse"
//usage:     "\n	-a	Include names starting with ."
//usage:     "\n	-d	List directory names, not contents"
// -a,-d text should match ls --help
//usage:     "\n	-l	List long flag names"
//usage:     IF_NOT_PLATFORM_MINGW32(
//usage:     "\n	-p	List project ID"
//usage:     "\n	-v	List version/generation number"
//usage:     )
//usage:     IF_PLATFORM_MINGW32(
//usage:     "\n\nAttributes:\n"
//usage:     "\n	j	Junction"
//usage:     "\n	l	Symbolic link"
//usage:     "\n	A	App exec link"
//usage:     "\n	R	Reparse point"
//usage:     "\n	o	Offline"
//usage:     "\n	e	Encrypted"
//usage:     "\n	c	Compressed"
//usage:     "\n	S	Sparse"
//usage:     "\n	r	Read only"
//usage:     "\n	h	Hidden"
//usage:     "\n	s	System"
//usage:     "\n	a	Archive"
//usage:     "\n	t	Temporary"
//usage:     "\n	n	Not indexed"
//usage:     )

#include "libbb.h"
#include "e2fs_lib.h"

enum {
	OPT_RECUR      = 1 << 0,
	OPT_ALL        = 1 << 1,
	OPT_DIRS_OPT   = 1 << 2,
	OPT_PF_LONG    = 1 << 3,
	OPT_GENERATION = 1 << 4,
	OPT_PROJID     = 1 << 5,
};

static void list_attributes(const char *name)
{
#if !ENABLE_PLATFORM_MINGW32
	unsigned fsflags;
	int fd, r;

	/* There is no way to run needed ioctls on a symlink.
	 * open(O_PATH | O_NOFOLLOW) _can_ be used to get a fd referring to the symlink,
	 * but ioctls fail on such a fd (tried on 4.12.0 kernel).
	 * e2fsprogs-1.46.2 uses open(O_NOFOLLOW), it fails on symlinks.
	 */
	fd = open_or_warn(name, O_RDONLY | O_NONBLOCK | O_NOCTTY | O_NOFOLLOW);
	if (fd < 0)
		return;

	if (option_mask32 & OPT_PROJID) {
		struct ext2_fsxattr fsxattr;
		r = ioctl(fd, EXT2_IOC_FSGETXATTR, &fsxattr);
		/* note: ^^^ may fail in 32-bit userspace on 64-bit kernel (seen on 4.12.0) */
		if (r != 0)
			goto read_err;
		printf("%5u ", (unsigned)fsxattr.fsx_projid);
	}

	if (option_mask32 & OPT_GENERATION) {
		unsigned generation;
		r = ioctl(fd, EXT2_IOC_GETVERSION, &generation);
		if (r != 0)
			goto read_err;
		printf("%-10u ", generation);
	}

	r = ioctl(fd, EXT2_IOC_GETFLAGS, &fsflags);
	if (r != 0)
		goto read_err;

	close(fd);
#else /* ENABLE_PLATFORM_MINGW32 */
	struct stat st;

	if (lstat(name, &st) == 0 && !(S_ISREG(st.st_mode) ||
				S_ISDIR(st.st_mode) || S_ISLNK(st.st_mode)))
		goto read_err;

#define fsflags &st
#endif

	if (option_mask32 & OPT_PF_LONG) {
		printf("%-28s ", name);
		print_e2flags_long(fsflags);
		bb_putchar('\n');
	} else {
		print_e2flags(fsflags);
		printf(" %s\n", name);
	}

	return;
 read_err:
	bb_perror_msg("reading %s", name);
#if !ENABLE_PLATFORM_MINGW32
	close(fd);
#endif
}

static int FAST_FUNC lsattr_dir_proc(const char *dir_name,
		struct dirent *de,
		void *private UNUSED_PARAM)
{
	struct stat st;
	char *path;

	path = concat_path_file(dir_name, de->d_name);

	if (lstat(path, &st) != 0)
		bb_perror_msg("can't stat '%s'", path);

	else if (de->d_name[0] != '.' || (option_mask32 & OPT_ALL)) {
	        /* Don't try to open device files, fifos etc */
		if (S_ISREG(st.st_mode) || S_ISLNK(st.st_mode) || S_ISDIR(st.st_mode))
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
		bb_perror_msg("can't stat '%s'", name);
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
	getopt32(argv, "Radlvp");
#endif
	argv += optind;

	if (!*argv)
		*--argv = (char*)".";
	do lsattr_args(*argv++); while (*argv);

	return EXIT_SUCCESS;
}
