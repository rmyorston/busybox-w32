/* vi: set sw=4 ts=4: */
/*
 * See README for additional information
 *
 * Licensed under GPLv2, see file LICENSE in this source tree.
 */

#include "libbb.h"
#include "e2fs_lib.h"

#if !ENABLE_PLATFORM_MINGW32
#define HAVE_EXT2_IOCTLS 1

#if INT_MAX == LONG_MAX
#define IF_LONG_IS_SAME(...) __VA_ARGS__
#define IF_LONG_IS_WIDER(...)
#else
#define IF_LONG_IS_SAME(...)
#define IF_LONG_IS_WIDER(...) __VA_ARGS__
#endif

static void close_silently(int fd)
{
	int e = errno;
	close(fd);
	errno = e;
}
#endif


/* Iterate a function on each entry of a directory */
int iterate_on_dir(const char *dir_name,
		int FAST_FUNC (*func)(const char *, struct dirent *, void *),
		void *private)
{
	DIR *dir;
	struct dirent *de;

	dir = opendir(dir_name);
	if (dir == NULL) {
		return -1;
	}
	while ((de = readdir(dir)) != NULL) {
		func(dir_name, de, private);
	}
	closedir(dir);
	return 0;
}


#if !ENABLE_PLATFORM_MINGW32
/* Get/set a file version on an ext2 file system */
int fgetsetversion(const char *name, unsigned long *get_version, unsigned long set_version)
{
#if HAVE_EXT2_IOCTLS
	int fd, r;
	IF_LONG_IS_WIDER(int ver;)

	fd = open(name, O_RDONLY | O_NONBLOCK);
	if (fd == -1)
		return -1;
	if (!get_version) {
		IF_LONG_IS_WIDER(
			ver = (int) set_version;
			r = ioctl(fd, EXT2_IOC_SETVERSION, &ver);
		)
		IF_LONG_IS_SAME(
			r = ioctl(fd, EXT2_IOC_SETVERSION, (void*)&set_version);
		)
	} else {
		IF_LONG_IS_WIDER(
			r = ioctl(fd, EXT2_IOC_GETVERSION, &ver);
			*get_version = ver;
		)
		IF_LONG_IS_SAME(
			r = ioctl(fd, EXT2_IOC_GETVERSION, (void*)get_version);
		)
	}
	close_silently(fd);
	return r;
#else /* ! HAVE_EXT2_IOCTLS */
	errno = EOPNOTSUPP;
	return -1;
#endif /* ! HAVE_EXT2_IOCTLS */
}

/* Get/set a file flags on an ext2 file system */
int fgetsetflags(const char *name, unsigned long *get_flags, unsigned long set_flags)
{
#if HAVE_EXT2_IOCTLS
	struct stat buf;
	int fd, r;
	IF_LONG_IS_WIDER(int f;)

	if (stat(name, &buf) == 0 /* stat is ok */
	 && !S_ISREG(buf.st_mode) && !S_ISDIR(buf.st_mode)
	) {
		goto notsupp;
	}
	fd = open(name, O_RDONLY | O_NONBLOCK); /* neither read nor write asked for */
	if (fd == -1)
		return -1;

	if (!get_flags) {
		IF_LONG_IS_WIDER(
			f = (int) set_flags;
			r = ioctl(fd, EXT2_IOC_SETFLAGS, &f);
		)
		IF_LONG_IS_SAME(
			r = ioctl(fd, EXT2_IOC_SETFLAGS, (void*)&set_flags);
		)
	} else {
		IF_LONG_IS_WIDER(
			r = ioctl(fd, EXT2_IOC_GETFLAGS, &f);
			*get_flags = f;
		)
		IF_LONG_IS_SAME(
			r = ioctl(fd, EXT2_IOC_GETFLAGS, (void*)get_flags);
		)
	}

	close_silently(fd);
	return r;
 notsupp:
#endif /* HAVE_EXT2_IOCTLS */
	errno = EOPNOTSUPP;
	return -1;
}
#else /* ENABLE_PLATFORM_MINGW32 */
/* Only certain attributes can be set using SetFileAttributes() */
#define CHATTR_MASK (FILE_ATTRIBUTE_READONLY | FILE_ATTRIBUTE_HIDDEN | \
			FILE_ATTRIBUTE_SYSTEM | FILE_ATTRIBUTE_ARCHIVE | \
			FILE_ATTRIBUTE_TEMPORARY | FILE_ATTRIBUTE_NOT_CONTENT_INDEXED | \
			FILE_ATTRIBUTE_OFFLINE)

/* Get/set file attributes on a Windows file system */
int fgetsetflags(const char *name, unsigned long *get_flags, unsigned long set_flags)
{
	struct stat buf;

	if (stat(name, &buf) == 0 /* stat is ok */
	 && !S_ISREG(buf.st_mode) && !S_ISDIR(buf.st_mode)
	) {
		errno = EOPNOTSUPP;
		return -1;
	}

	if (get_flags) {
		*get_flags = (unsigned long)buf.st_attr;
	}
	else if (!SetFileAttributes(name, set_flags & CHATTR_MASK)) {
		errno = err_win_to_posix();
		return -1;
	}
	return 0;
}
#endif


#if !ENABLE_PLATFORM_MINGW32
/* Print file attributes on an ext2 file system */
const uint32_t e2attr_flags_value[] ALIGN4 = {
#ifdef ENABLE_COMPRESSION
	EXT2_COMPRBLK_FL,
	EXT2_DIRTY_FL,
	EXT2_NOCOMPR_FL,
	EXT2_ECOMPR_FL,
#endif
	EXT2_INDEX_FL,
	EXT2_SECRM_FL,
	EXT2_UNRM_FL,
	EXT2_SYNC_FL,
	EXT2_DIRSYNC_FL,
	EXT2_IMMUTABLE_FL,
	EXT2_APPEND_FL,
	EXT2_NODUMP_FL,
	EXT2_NOATIME_FL,
	EXT2_COMPR_FL,
	EXT3_JOURNAL_DATA_FL,
	EXT2_NOTAIL_FL,
	EXT2_TOPDIR_FL
};

const char e2attr_flags_sname[] ALIGN1 =
#ifdef ENABLE_COMPRESSION
	"BZXE"
#endif
	"I"
	"suSDiadAcjtT";

static const char e2attr_flags_lname[] ALIGN1 =
#ifdef ENABLE_COMPRESSION
	"Compressed_File" "\0"
	"Compressed_Dirty_File" "\0"
	"Compression_Raw_Access" "\0"
	"Compression_Error" "\0"
#endif
	"Indexed_directory" "\0"
	"Secure_Deletion" "\0"
	"Undelete" "\0"
	"Synchronous_Updates" "\0"
	"Synchronous_Directory_Updates" "\0"
	"Immutable" "\0"
	"Append_Only" "\0"
	"No_Dump" "\0"
	"No_Atime" "\0"
	"Compression_Requested" "\0"
	"Journaled_Data" "\0"
	"No_Tailmerging" "\0"
	"Top_of_Directory_Hierarchies" "\0"
	/* Another trailing NUL is added by compiler */;
#else /* ENABLE_PLATFORM_MINGW32 */
/* Print file attributes on a Windows file system */
const uint32_t e2attr_flags_value[] = {
	FILE_ATTRIBUTE_REPARSE_POINT,
	FILE_ATTRIBUTE_OFFLINE,
	FILE_ATTRIBUTE_ENCRYPTED,
	FILE_ATTRIBUTE_COMPRESSED,
	FILE_ATTRIBUTE_SPARSE_FILE,
	FILE_ATTRIBUTE_READONLY,
	FILE_ATTRIBUTE_HIDDEN,
	FILE_ATTRIBUTE_SYSTEM,
	FILE_ATTRIBUTE_ARCHIVE,
	FILE_ATTRIBUTE_TEMPORARY,
	FILE_ATTRIBUTE_NOT_CONTENT_INDEXED,
};

const char e2attr_flags_sname[] ALIGN1 =
	"roecSrhsatn";

static const char e2attr_flags_lname[] ALIGN1 =
	"Reparse" "\0"
	"Offline" "\0"
	"Encrypted" "\0"
	"Compressed" "\0"
	"Sparse" "\0"
	"Readonly" "\0"
	"Hidden" "\0"
	"System" "\0"
	"Archive" "\0"
	"Temporary" "\0"
	"Notindexed" "\0"
	/* Another trailing NUL is added by compiler */;
#endif

void print_e2flags(FILE *f, unsigned long flags, unsigned options)
{
	const uint32_t *fv;
	const char *fn;

	fv = e2attr_flags_value;
	if (options & PFOPT_LONG) {
		int first = 1;
		fn = e2attr_flags_lname;
		do {
			if (flags & *fv) {
				if (!first)
					fputs(", ", f);
				fputs(fn, f);
				first = 0;
			}
			fv++;
			fn += strlen(fn) + 1;
		} while (*fn);
		if (first)
			fputs("---", f);
	} else {
		fn = e2attr_flags_sname;
		do  {
			char c = '-';
			if (flags & *fv)
				c = *fn;
			fputc(c, f);
			fv++;
			fn++;
		} while (*fn);
	}
}
