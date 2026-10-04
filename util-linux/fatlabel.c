/* vi: set sw=4 ts=4: */
/*
 * Copyright 2026 Denys Vlasenko <vda.linux@googlemail.com>
 *
 * Licensed under GPLv2, see file LICENSE in this source tree.
 */
//config:config FATLABEL
//config:	bool "fatlabel (1 kb)"
//config:	default y
//config:	help
//config:	fatlabel shows or sets FAT33 label.

//applet:IF_FATLABEL(APPLET(fatlabel, BB_DIR_SBIN, BB_SUID_DROP))

//kbuild:lib-$(CONFIG_FATLABEL) += fatlabel.o

//usage:#define fatlabel_trivial_usage
//usage:       "BLOCKDEV [LABEL]"
//usage:#define fatlabel_full_usage "\n\n"
//usage:       "Show or set FAT32 label (11 bytes at offset 0x47) on BLOCKDEV"

#include "libbb.h"
#include "common_bufsiz.h"

#define SECTOR_SIZE     512

#define ATTR_VOLUME     8

#define NUM_FATS        2

// FAT32 filesystem looks like this:
// sector -nn...-1: "hidden" sectors, all sectors before this partition
// (-h hidden-sectors sets it. Useful only for boot loaders,
//  they need to know _disk_ offset in order to be able to correctly
//  address sectors relative to start of disk)
// sector 0: boot sector
// sector 1: info sector
// sector 2: set aside for boot code which didn't fit into sector 0
// ...(zero-filled sectors)...
// sector B: backup copy of sector 0 [B set by -b backup-boot-sector]
// sector B+1: backup copy of sector 1
// sector B+2: backup copy of sector 2
// ...(zero-filled sectors)...
// sector R: FAT#1 [R set by -R reserved-sectors]
// ...(FAT#1)...
// sector R+fat_size: FAT#2
// ...(FAT#2)...
// sector R+fat_size*2: cluster #2
// ...(cluster #2)...
// sector R+fat_size*2+clust_size: cluster #3
// ...(the rest is filled by clusters till the end)...

struct msdos_dir_entry {
	char     name[11];       // 000 name and extension
	uint8_t  attr;           // 00b attribute bits
	uint8_t  lcase;          // 00c case for base and extension
	uint8_t  ctime_cs;       // 00d creation time, centiseconds (0-199)
	uint16_t ctime;          // 00e creation time
	uint16_t cdate;          // 010 creation date
	uint16_t adate;          // 012 last access date
	uint16_t starthi;        // 014 high 16 bits of cluster in FAT32
	uint16_t time;           // 016 time
	uint16_t date;           // 018 date
	uint16_t start;          // 01a first cluster
	uint32_t size;           // 01c file size in bytes
} PACKED;

/* Example of boot sector's beginning:
0000  eb 58 90 4d 53 57 49 4e  34 2e 31 00 02 08 26 00  |...MSWIN4.1...&.|
0010  02 00 00 00 00 f8 00 00  3f 00 ff 00 3f 00 00 00  |........?...?...|
0020  54 9b d0 00 0d 34 00 00  00 00 00 00 02 00 00 00  |T....4..........|
0030  01 00 06 00 00 00 00 00  00 00 00 00 00 00 00 00  |................|
0040  80 00 29 71 df 51 e0 4e  4f 20 4e 41 4d 45 20 20  |..)q.Q.NO NAME  |
0050  20 20 46 41 54 33 32 20  20 20 33 c9 8e d1 bc f4  |  FAT32   3.....|
*/
struct msdos_volume_info { // (offsets are relative to start of boot sector)
	uint8_t  drive_number;    // 040 BIOS drive number
	uint8_t  reserved;        // 041 unused
	uint8_t  ext_boot_sign;	  // 042 0x29 if fields below exist (DOS 3.3+)
	uint32_t volume_id32;     // 043 volume ID number
	char     volume_label[11];// 047 volume label
	char     fs_type[8];      // 052 typically "FATnn"
} PACKED;                         // 05a end. Total size 26 (0x1a) bytes

struct msdos_boot_sector {
	// We use strcpy to fill both, and gcc-4.4.x complains if they are separate
	char     boot_jump_and_sys_id[3+8]; //000 short or near jump instruction
	/*char   system_id[8];*/     // 003 name - can be used to special case partition manager volumes
	uint16_t bytes_per_sect;     // 00b bytes per logical sector
	uint8_t  sect_per_clust;     // 00d sectors/cluster
	uint16_t reserved_sect;      // 00e reserved sectors (sector offset of 1st FAT relative to volume start)
	uint8_t  fats;               // 010 number of FATs
	uint16_t dir_entries;        // 011 root directory entries
	uint16_t volume_size_sect;   // 013 volume size in sectors
	uint8_t  media_byte;         // 015 media code
	uint16_t fat16_sect_per_fat; // 016 sectors/FAT, must be 0 for FAT32
	uint16_t sect_per_track;     // 018 sectors per track
	uint16_t heads;              // 01a number of heads
	uint32_t hidden;             // 01c hidden sectors (sector offset of volume within physical disk)
	uint32_t fat32_volume_size_sect; // 020 volume size in sectors (if volume_size_sect == 0)
	uint32_t fat32_sect_per_fat; // 024 sectors/FAT
	uint16_t fat32_flags;        // 028 bit 8: fat mirroring, low 4: active fat
	uint8_t  fat32_version[2];   // 02a major, minor filesystem version (I see 0,0)
	uint32_t fat32_root_cluster; // 02c first cluster in root directory
	uint16_t fat32_info_sector;  // 030 filesystem info sector (usually 1)
	uint16_t fat32_backup_boot;  // 032 backup boot sector (usually 6)
	uint32_t reserved2[3];       // 034 unused
	struct msdos_volume_info vi; // 040
	char     boot_code[0x200 - 0x5a - 2]; // 05a
#define BOOT_SIGN 0xAA55
	uint16_t boot_sign;          // 1fe
} PACKED;

#define FAT_FSINFO_SIG1 0x41615252
#define FAT_FSINFO_SIG2 0x61417272
struct fat32_fsinfo {
	uint32_t signature1;         // 0x52,0x52,0x41,0x61, "RRaA"
	uint32_t reserved1[128 - 8];
	uint32_t signature2;         // 0x72,0x72,0x61,0x41, "rrAa"
	uint32_t free_clusters;      // free cluster count.  -1 if unknown
	uint32_t next_cluster;       // most recently allocated cluster
	uint32_t reserved2[3];
	uint16_t reserved3;          // 1fc
	uint16_t boot_sign;          // 1fe
} PACKED;

struct bug_check {
	char BUG1[sizeof(struct msdos_dir_entry  ) == 0x20 ? 1 : -1];
	char BUG2[sizeof(struct msdos_volume_info) == 0x1a ? 1 : -1];
	char BUG3[sizeof(struct msdos_boot_sector) == 0x200 ? 1 : -1];
	char BUG4[sizeof(struct fat32_fsinfo     ) == 0x200 ? 1 : -1];
};

// compat: dosfstools-4.2
// $ fatlabel --version
// fatlabel 4.2 (2021-01-31)
// $ fatlabel --help
// Usage: fatlabel [OPTIONS] DEVICE [NEW]
// Change the FAT filesystem label or serial on DEVICE to NEW or display the
// existing label or serial if NEW is not given.
// Options:
//   -i, --volume-id     Work on serial number instead of label
//   -r, --reset         Remove label or generate new serial number
//   -c N, --codepage=N  use DOS codepage N to encode/decode label (default: 850)
//   -V, --version       Show version number and terminate
//   -h, --help          Print this message and terminate
static int bad_fat32(const struct msdos_boot_sector *boot_blk)
{
	uint16_t bytes_per_sect16;

	move_from_unaligned16(bytes_per_sect16, &boot_blk->bytes_per_sect);
	return ((uint8_t)(boot_blk->fats - 1) > 1 // 0 or >2
	 || boot_blk->fat16_sect_per_fat != 0
	// maybe also check boot_blk->dir_entries == 0? It's only for FAT16/12
	 || boot_blk->vi.ext_boot_sign != 0x29
	 || boot_blk->boot_sign != SWAP_LE16(BOOT_SIGN)
	 || bytes_per_sect16 != SWAP_LE16(SECTOR_SIZE)
	 || boot_blk->fat32_info_sector != SWAP_LE16(0x0001)
	//WRONG: || boot_blk->fat32_backup_boot != SWAP_LE16(0x0006)
	);
}

int fatlabel_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int fatlabel_main(int argc UNUSED_PARAM, char **argv)
{
	char sector1[1 * SECTOR_SIZE];
	char sector2[1 * SECTOR_SIZE];
	struct msdos_boot_sector *boot_blk = (void*)sector1;
	unsigned backup_boot_sect;
	int fd;

	getopt32(argv, "^" "\0" "-1?2"/* 1 or 2 arg2*/);
	argv += optind;

	fd = xopen(argv[0], argv[1] ? O_RDWR : O_RDONLY);

	xread(fd, sector1, sizeof(sector1));
	backup_boot_sect = FETCH_LE16(boot_blk->fat32_backup_boot);
#if 0
	fprintf(stderr,
		"0x003: sys_id:'%.8s'\n"
		"0x00b: bytes_per_sect: 0x%04x\n" // 0
		"0x00d: sect_per_clust: 0x%02x\n" // 0
		"0x010: fats: 0x%02x\n" // 1 or 2
		"0x011: dir_entries: 0x%04x\n" // always 0 for FAT32?
		"0x015: media_byte: 0x%02x\n" // 0xf8 for "hard disk"
		"0x016: fat16_sect_per_fat: 0x%04x\n" // 0
		"0x028: fat32_flags: 0x%04x\n" // 0,0
		"0x02a: fat32_version: 0x%02x,0x%02x\n" // 0,0
		"0x030: fat32_info_sector: 0x%04x\n" // always 1?
		"0x032: fat32_backup_boot: 0x%04x\n" // usually 6 by microsoft, but our own mkfs.vfat uses 3
		"0x041: mounted: 0x%02x\n" // 0/1
		"0x042: ext_boot_sign: 0x%02x\n" // 0x29
		"0x043: volume_id32: 0x%08x\n"
		"0x047: volume_label: '%.11s'\n"
		"0x052: fs_type: '%.8s'\n"
		"0x1fe: boot_sign: 0x%02x,0x%02x\n" // 0x55,0xaa
		, 3 + boot_blk->boot_jump_and_sys_id
		, FETCH_LE16(boot_blk->bytes_per_sect) // FIXME: unaligned fetch
		, boot_blk->sect_per_clust
		, boot_blk->fats
		, FETCH_LE16(boot_blk->dir_entries) // FIXME: unaligned fetch
		, boot_blk->media_byte
		, FETCH_LE16(boot_blk->fat16_sect_per_fat)
		, FETCH_LE16(boot_blk->fat32_flags)
		, boot_blk->fat32_version[0], boot_blk->fat32_version[1]
		, FETCH_LE16(boot_blk->fat32_info_sector)
		, backup_boot_sect
		, boot_blk->vi.reserved // Linux uses this as "mounted now" bit, some WinNT too?
		, boot_blk->vi.ext_boot_sign
		, FETCH_LE32(boot_blk->vi.volume_id32) // FIXME: unaligned fetch
		, boot_blk->vi.volume_label
		, boot_blk->vi.fs_type
		, ((uint8_t*)&boot_blk->boot_sign)[0]
		, ((uint8_t*)&boot_blk->boot_sign)[1]
	);
#endif
	if (bad_fat32(boot_blk))
		bb_error_msg_and_die("'%s' is not FAT32", argv[0]);

	// backup_boot_sect can be 0 or 0xffff for 'disabled'.
	// 1 is almost certainly bogus (we require that fat32_info_sector is 1).
	// Let's also not accept suspiciously large values.
	if (backup_boot_sect > 1 && backup_boot_sect < 0x100) {
		xlseek(fd, backup_boot_sect * SECTOR_SIZE, SEEK_SET);
		xread(fd, sector2, sizeof(sector2));
		if (boot_blk->vi.reserved == 1) // "mounted now"?
			boot_blk->vi.reserved = 0;
		if (memcmp(sector1, sector2, sizeof(sector1)) != 0)
			bb_error_msg_and_die("'%s' backup sector %d mismatch", argv[0], backup_boot_sect);
	} else {
		backup_boot_sect = 0;
	}

	if (argv[1]) {
		// dosfstools-4.2 warns:
		// "fatlabel: warning - lowercase labels might not work properly on some systems"
		sprintf(boot_blk->vi.volume_label, "%-11.11s", argv[1]);
		xlseek(fd, 0x047, SEEK_SET);
		xwrite(fd, boot_blk->vi.volume_label, 11);
		// Redundant is backup_boot_sect == 0, but no harm done:
		xlseek(fd, 0x047 + backup_boot_sect * SECTOR_SIZE, SEEK_SET);
		xwrite(fd, boot_blk->vi.volume_label, 11);
	} else {
		// Mimic dosfstools-4.2
		// But do not special-case "NO NAME" label as 'label not set'.
		int sz = strnlen(boot_blk->vi.volume_label, 11);
		while (sz != 0 && boot_blk->vi.volume_label[sz - 1] == ' ')
			sz--;
		if (sz != 0)
			printf("%.*s\n", sz, boot_blk->vi.volume_label);
	}

	if (ENABLE_FEATURE_CLEAN_UP)
		close(fd);

	fflush_stdout_and_exit_SUCCESS();
}
