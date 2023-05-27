/* vi: set sw=4 ts=4: */
/*
 * See README for additional information
 *
 * Licensed under GPLv2, see file LICENSE in this source tree.
 */

#include "libbb.h"
#include "e2fs_lib.h"

#if !ENABLE_PLATFORM_MINGW32
/* Print file attributes on an ext2 file system */
const uint32_t e2attr_flags_value[] ALIGN4 = {
#ifdef ENABLE_COMPRESSION
	EXT2_COMPRBLK_FL,
	EXT2_DIRTY_FL,
	EXT2_NOCOMPR_FL,
#endif
	EXT2_SECRM_FL,
	EXT2_UNRM_FL,
	EXT2_SYNC_FL,
	EXT2_DIRSYNC_FL,
	EXT2_IMMUTABLE_FL,
	EXT2_APPEND_FL,
	EXT2_NODUMP_FL,
	EXT2_NOATIME_FL,
	EXT2_COMPR_FL,
	EXT2_ECOMPR_FL,
	EXT3_JOURNAL_DATA_FL,
	EXT2_INDEX_FL,
	EXT2_NOTAIL_FL,
	EXT2_TOPDIR_FL,
	EXT2_EXTENT_FL,
	EXT2_NOCOW_FL,
	EXT2_CASEFOLD_FL,
	EXT2_INLINE_DATA_FL,
	EXT2_PROJINHERIT_FL,
	EXT2_VERITY_FL,
};

const char e2attr_flags_sname[] ALIGN1 =
#ifdef ENABLE_COMPRESSION
	"BZX"
#endif
	"suSDiadAcEjItTeCFNPV";

static const char e2attr_flags_lname[] ALIGN1 =
#ifdef ENABLE_COMPRESSION
	"Compressed_File" "\0"
	"Compressed_Dirty_File" "\0"
	"Compression_Raw_Access" "\0"
#endif
	"Secure_Deletion" "\0"
	"Undelete" "\0"
	"Synchronous_Updates" "\0"
	"Synchronous_Directory_Updates" "\0"
	"Immutable" "\0"
	"Append_Only" "\0"
	"No_Dump" "\0"
	"No_Atime" "\0"
	"Compression_Requested" "\0"
	"Encrypted" "\0"
	"Journaled_Data" "\0"
	"Indexed_directory" "\0"
	"No_Tailmerging" "\0"
	"Top_of_Directory_Hierarchies" "\0"
	"Extents" "\0"
	"No_COW" "\0"
	"Casefold" "\0"
	"Inline_Data" "\0"
	"Project_Hierarchy" "\0"
	"Verity" "\0"
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
	"RoecSrhsatn";

static const char e2attr_flags_lname[] ALIGN1 =
	"Reparse_Point" "\0"
	"Offline" "\0"
	"Encrypted" "\0"
	"Compressed" "\0"
	"Sparse" "\0"
	"Read_Only" "\0"
	"Hidden" "\0"
	"System" "\0"
	"Archive" "\0"
	"Temporary" "\0"
	"Not_Indexed" "\0"
	/* Another trailing NUL is added by compiler */;
#endif

#if !ENABLE_PLATFORM_MINGW32
void print_e2flags_long(unsigned flags)
#else
#define flags sb->st_attr
void print_e2flags_long(struct stat *sb)
#endif
{
	const uint32_t *fv;
	const char *fn;
#if ENABLE_PLATFORM_MINGW32
	const char *ln;
#endif
	int first = 1;

	fv = e2attr_flags_value;
	fn = e2attr_flags_lname;
	do {
		if (flags & *fv) {
			if (!first)
				fputs(", ", stdout);
#if ENABLE_PLATFORM_MINGW32
			ln = fn;
			if (*fv == FILE_ATTRIBUTE_REPARSE_POINT) {
				switch (sb->st_tag) {
				case IO_REPARSE_TAG_SYMLINK:
					ln = "Symbolic_Link";
					break;
				case IO_REPARSE_TAG_MOUNT_POINT:
					ln = "Junction";
					break;
				case IO_REPARSE_TAG_APPEXECLINK:
					ln = "App_Exec_Link";
					break;
				}
			}
			fputs(ln, stdout);
#else
			fputs(fn, stdout);
#endif
			first = 0;
		}
		fv++;
		fn += strlen(fn) + 1;
	} while (*fn);
	if (first)
		fputs("---", stdout);
}

#if !ENABLE_PLATFORM_MINGW32
void print_e2flags(unsigned flags)
#else
void print_e2flags(struct stat *sb)
#endif
{
	const uint32_t *fv;
	const char *fn;

	fv = e2attr_flags_value;
	fn = e2attr_flags_sname;
	do  {
		char c = '-';
		if (flags & *fv)
			c = *fn;
#if ENABLE_PLATFORM_MINGW32
		if (*fv == FILE_ATTRIBUTE_REPARSE_POINT) {
			switch (sb->st_tag) {
			case IO_REPARSE_TAG_SYMLINK:
				c = 'l';
				break;
			case IO_REPARSE_TAG_MOUNT_POINT:
				c = 'j';
				break;
			case IO_REPARSE_TAG_APPEXECLINK:
				c = 'A';
				break;
			}
		}
#endif
		putchar(c);
		fv++;
		fn++;
	} while (*fn);
}
