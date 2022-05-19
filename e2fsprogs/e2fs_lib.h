/* vi: set sw=4 ts=4: */
/*
 * See README for additional information
 *
 * This file can be redistributed under the terms of the GNU Library General
 * Public License
 */

/* Constants and structures */
#include "bb_e2fs_defs.h"

PUSH_AND_SET_FUNCTION_VISIBILITY_TO_HIDDEN

#if ENABLE_PLATFORM_MINGW32
/* Only certain attributes can be set using SetFileAttributes() */
#define CHATTR_MASK (FILE_ATTRIBUTE_READONLY | FILE_ATTRIBUTE_HIDDEN | \
			FILE_ATTRIBUTE_SYSTEM | FILE_ATTRIBUTE_ARCHIVE | \
			FILE_ATTRIBUTE_TEMPORARY | FILE_ATTRIBUTE_NOT_CONTENT_INDEXED)

/* Print file attributes on an NTFS file system */
void print_e2flags_long(struct stat *sb);
void print_e2flags(struct stat *sb);
#else

/* Print file attributes on an ext2 file system */
void print_e2flags_long(unsigned flags);
void print_e2flags(unsigned flags);
#endif

extern const uint32_t e2attr_flags_value[];
extern const char e2attr_flags_sname[];

/* If you plan to ENABLE_COMPRESSION, see e2fs_lib.c and chattr.c - */
/* make sure that chattr doesn't accept bad options! */
#if !ENABLE_PLATFORM_MINGW32
#ifdef ENABLE_COMPRESSION
#define e2attr_flags_value_chattr (&e2attr_flags_value[5])
#define e2attr_flags_sname_chattr (&e2attr_flags_sname[5])
#else
#define e2attr_flags_value_chattr (&e2attr_flags_value[1])
#define e2attr_flags_sname_chattr (&e2attr_flags_sname[1])
#endif
#else
#define e2attr_flags_value_chattr (&e2attr_flags_value[5])
#define e2attr_flags_sname_chattr (&e2attr_flags_sname[5])
#endif

POP_SAVED_FUNCTION_VISIBILITY
