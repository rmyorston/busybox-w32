/* vi: set sw=4 ts=4: */
/*
 * mkfs_vfat: utility to create FAT32 filesystem
 * inspired by dosfstools
 *
 * Busybox'ed (2009) by Vladimir Dronnikov <dronnikov@gmail.com>
 *
 * Licensed under GPLv2, see file LICENSE in this source tree.
 */
//config:config MKDOSFS
//config:	bool "mkdosfs (7.6 kb)"
//config:	default y
//config:	help
//config:	Utility to create FAT filesystems.
//config:
//config:config MKFS_VFAT
//config:	bool "mkfs.vfat (7.6 kb)"
//config:	default y
//config:	help
//config:	Alias to "mkdosfs".

//                    APPLET_ODDNAME:name       main       location     suid_type     help
//applet:IF_MKDOSFS(  APPLET_ODDNAME(mkdosfs,   mkfs_vfat, BB_DIR_SBIN, BB_SUID_DROP, mkfs_vfat))
//applet:IF_MKFS_VFAT(APPLET_ODDNAME(mkfs.vfat, mkfs_vfat, BB_DIR_SBIN, BB_SUID_DROP, mkfs_vfat))

//kbuild:lib-$(CONFIG_MKDOSFS) += mkfs_vfat.o
//kbuild:lib-$(CONFIG_MKFS_VFAT) += mkfs_vfat.o

#include "libbb.h"

#include <linux/hdreg.h> // HDIO_GETGEO
#include <linux/fd.h>    // FDGETPRM
#include <sys/mount.h>   // BLKSSZGET
#if !defined(BLKSSZGET)
# define BLKSSZGET _IO(0x12, 104)
#endif

// sys/mount.h and linux/fs.h define these, but let's not depend on that
#undef BLOCK_SIZE_BITS
#undef BLOCK_SIZE
#define BLOCK_SIZE_BITS 10
#define BLOCK_SIZE      (1<<BLOCK_SIZE_BITS)

#if 0
# define dbg(...) bb_error_msg(__VA_ARGS__)
#else
# define dbg(...) ((void)0)
#endif

#define ULL(x) ((unsigned long long)(x))

// M$ says the high 4 bits of FAT32 FAT entries are reserved

#define MAX_CLUST_CNT12 0xFF4
#define MAX_CLUST_CNT16 0xFFF4
#define MAX_CLUST_CNT32 0x0FFFFFF5
// The cluster numbers on largest possible FAT32 are 2..MAX_CLUST_CNT32+1.
// General rules for cluster numbers in FAT table (for all sizes: 12,16,32):
// 0x0: is "free clister"
// 0x1: reserved
// from 0x2 to 0xF..F5 are valid cluster numbers.
// 0xF..F6 reserved - but we consider it as valid too.
// 0xF..F7 is "has bad sector(s)"
// from 0xF..F8 to 0xF..FF are "no next cluster in this chain".
// NB: Linux kernel uses 0x0FFFFFF6, making it treat 0x0FFFFFF7 as valid cluster# too.
// These large cluster numbers can only occur on 129GB+ disks
// if cluster size is 512 bytes, 1TB+ if 4k clusters.
// Example of command creating filesystem with 0x0ffffff5 clusters:
// mkfs.vfat -F32 -b0 -R3 -s1 -C IMAGE.bin $((130*1024*1024 - 4))

#define BAD_FAT32       0x0FFFFFF7
#define EOF_FAT32       0x0FFFFFF8

#define ATTR_VOLUME     8

#define NUM_FATS        2

#define SECTOR_SIZE     512

#define SECTORS_PER_BLOCK	(BLOCK_SIZE / SECTOR_SIZE)

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

// how many blocks we try to read while testing
#define TEST_BUFFER_BLOCKS      16

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

// Example of boot sector's beginning:
//0000  eb 58 90 4d 53 57 49 4e  34 2e 31 00 02 08 26 00  |...MSWIN4.1...&.|
//0010  02 00 00 00 00 f8 00 00  3f 00 ff 00 3f 00 00 00  |........?...?...|
//0020  54 9b d0 00 0d 34 00 00  00 00 00 00 02 00 00 00  |T....4..........|
//0030  01 00 06 00 00 00 00 00  00 00 00 00 00 00 00 00  |................|
//0040  80 00 29 71 df 51 e0 4e  4f 20 4e 41 4d 45 20 20  |..)q.Q.NO NAME  |
//0050  20 20 46 41 54 33 32 20  20 20 33 c9 8e d1 bc f4  |  FAT32   3.....|
struct msdos_boot_sector {
	// We use strcpy to fill both, and gcc-4.4.x complains if they are separate
	char     boot_jump_and_sys_id[3+8]; // short or near jump instruction
	//char   system_id[8];          // 003 name - can be used to special case partition manager volumes
	uint16_t bytes_per_sect;        // 00b bytes per logical sector
	uint8_t  sect_per_clust;        // 00d sectors/cluster
	uint16_t reserved_sect;         // 00e reserved sectors (sector offset of 1st FAT relative to volume start)
	uint8_t  fats;                  // 010 number of FATs
	uint16_t dir_entries;           // 011 root directory entries, must be 0 for FAT32
	uint16_t volume_size_sct16;     // 013 volume size in sectors, must be 0 for FAT32
	uint8_t  media_byte;            // 015 media code
	uint16_t fat_size_sct16;        // 016 sectors/FAT, must be 0 for FAT32
	uint16_t sect_per_track;        // 018 sectors per track
	uint16_t heads;                 // 01a number of heads
	uint32_t hidden_sct;            // 01c hidden sectors (sector offset of volume within physical disk)
	uint32_t volume_size_sct32;     // 020 volume size in sectors (if volume_size_sct16 == 0)
	// FAT32 inserts additional fields here - see below
	uint8_t  drive_number;          // 024 BIOS drive number
	uint8_t  boot_flags;            // 025
	uint8_t  ext_boot_sign;         // 026 0x28 - DOS 3.4 EBPB; 0x29 - DOS 4.0 EBPB
	uint32_t volume_id32;           // 027 volume serial number
	char     volume_label[11];      // 02b volume label (exists if ext_boot_sign=0x29)
	char     fs_type[8];            // 036 typically "FATnn" (exists if ext_boot_sign=0x29)
	uint8_t  boot_code[0x1fe - 0x3e];//03e
#define BOOT_SIGN 0xAA55
	uint16_t boot_sign;             // 1fe
} PACKED;
struct msdos_boot_sector32 {
	// We use strcpy to fill both, and gcc-4.4.x complains if they are separate
	char     boot_jump_and_sys_id[3+8]; // short or near jump instruction
	//char   system_id[8];          // 003 name - can be used to special case partition manager volumes
	uint16_t bytes_per_sect;        // 00b bytes per logical sector
	uint8_t  sect_per_clust;        // 00d sectors/cluster
	uint16_t reserved_sect;         // 00e reserved sectors (sector offset of 1st FAT relative to volume start)
	uint8_t  fats;                  // 010 number of FATs
	uint16_t dir_entries;           // 011 root directory entries, must be 0 for FAT32
	uint16_t volume_size_sct16;     // 013 volume size in sectors, must be 0 for FAT32
	uint8_t  media_byte;            // 015 media code
	uint16_t fat_size_sct16;        // 016 sectors/FAT, must be 0 for FAT32
	uint16_t sect_per_track;        // 018 sectors per track
	uint16_t heads;                 // 01a number of heads
	uint32_t hidden_sct;            // 01c hidden sectors (sector offset of volume within physical disk)
	uint32_t volume_size_sct32;     // 020 volume size in sectors (if volume_size_sct16 == 0)
	// start of FAT32-specific fields
	uint32_t fat32_fat_size_sct32;  // 024 sectors/FAT
	uint16_t fat32_flags;           // 028 bit 8: FAT mirroring, low 4: active FAT
	uint8_t  fat32_version[2];      // 02a major, minor filesystem version (I see 0,0)
	uint32_t fat32_root_cluster;    // 02c first cluster in root directory
	uint16_t fat32_info_sector;     // 030 filesystem info sector (usually 1)
	uint16_t fat32_backup_boot;     // 032 backup boot sector (usually 6, 0: none)
	uint8_t	 fat32_boot_file[12];   // 034 Boot file name (boot code may search root dir for this name)
	// end of FAT32-specific fields
	uint8_t  fat32_drive_number;    // 040 BIOS drive number
	uint8_t  fat32_boot_flags;      // 041
	uint8_t  fat32_ext_boot_sign;	// 042 0x28 - DOS 3.4 EBPB; 0x29 - DOS 4.0 EBPB
	uint32_t fat32_volume_id32;     // 043 volume serial number
	char     fat32_volume_label[11];// 047 volume label (exists if ext_boot_sign=0x29)
	char     fat32_fs_type[8];      // 052 typically "FATnn" (exists if ext_boot_sign=0x29)
	char     boot_code[0x1fe - 0x5a];//05a
	uint16_t boot_sign;             // 1fe
} PACKED;

#define FAT_FSINFO_SIG1 0x41615252
#define FAT_FSINFO_SIG2 0x61417272
struct fat32_fsinfo {
	uint32_t signature1;            // 0x52,0x52,0x41,0x61, "RRaA"
	uint32_t reserved1[128 - 8];
	uint32_t signature2;            // 0x72,0x72,0x61,0x41, "rrAa"
	uint32_t free_clusters;         // free cluster count. -1 if unknown
	uint32_t next_cluster;          // most recently allocated cluster
	uint32_t reserved2[3];
	uint16_t reserved3;             // 1fc
	uint16_t boot_sign;             // 1fe
} PACKED;

struct bug_check {
	char BUG1[sizeof(struct msdos_dir_entry    ) == 0x20 ? 1 : -1];
	char BUG2[sizeof(struct msdos_boot_sector  ) == 0x200 ? 1 : -1];
	char BUG3[sizeof(struct msdos_boot_sector32) == 0x200 ? 1 : -1];
	char BUG4[sizeof(struct fat32_fsinfo       ) == 0x200 ? 1 : -1];
};

static const char boot_code[] ALIGN1 =
	"\x0e"          // 05a:         push  cs
	"\x1f"          // 05b:         pop   ds
	"\xbe\x77\x7c"  //  write_msg:  mov   si, offset message_txt
	"\xac"          // 05f:         lodsb
	"\x22\xc0"      // 060:         and   al, al
	"\x74\x0b"      // 062:         jz    key_press
	"\x56"          // 064:         push  si
	"\xb4\x0e"      // 065:         mov   ah, 0eh
	"\xbb\x07\x00"  // 067:         mov   bx, 0007h
	"\xcd\x10"      // 06a:         int   10h
	"\x5e"          // 06c:         pop   si
	"\xeb\xf0"      // 06d:         jmp   write_msg
	"\x32\xe4"      //  key_press:  xor   ah, ah
	"\xcd\x16"      // 071:         int   16h
	"\xcd\x19"      // 073:         int   19h
	"\xeb\xfe"      //  foo:        jmp   foo
	// 077: message_txt:
	"This is not a bootable disk\r\n";

//compat:
//mkdosfs 2.11 (12 Mar 2005)
//Usage: mkdosfs [-A] [-c] [-C] [-v] [-I] [-l bad-block-file]
//        [-b backup-boot-sector]
//        [-m boot-msg-file] [-n volume-name] [-i volume-id]
//        [-s sectors-per-cluster] [-S logical-sector-size]
//        [-f number-of-FATs]
//        [-h hidden-sectors] [-F fat-size] [-r root-dir-entries]
//        [-R reserved-sectors]
//        /dev/name [blocks]

//usage:#define mkfs_vfat_trivial_usage
//usage:       "[-Cv] [-n LABEL] [-F12|16|32] [-b NUM] [-R NUM] [-S SECTSIZE] [-s SECTCLUST] FILE|BLOCKDEV [KBYTES]"
// Accepted but ignored:
//     "[-c] [-I] [-l bad-block-file] "
//     "[-m boot-msg-file] [-i volume-id] "
//     "[-s sectors-per-cluster] [-f number-of-FATs] "
//     "[-h hidden-sectors] [-F fat-size] [-r root-dir-entries]"
//
//usage:#define mkfs_vfat_full_usage "\n\n"
//usage:       "Make FAT filesystem\n"
////usage:   "\n	-c	Check BLOCKDEV for bad blocks"
//usage:     "\n	-C	Create new FILE"
//usage:     "\n	-F 12|16|32"
//usage:     "\n	-b BACKUP_SECT"
//usage:     "\n	-R FAT_START	FAT start sector"
// ^^^ even though in FAT lingo it's "reserved sectors", in ordinary language
// it's the sector where FAT table starts.
//usage:     "\n	-S SECTOR_SIZE"
//usage:     "\n	-s SECTORS_PER_CLUSTER"
////usage:   "\n	-I	Allow to use entire disk device (e.g. /dev/hda)"
//usage:     "\n	-n LBL	Volume label"
//usage:     "\n	-v	Verbose"

int mkfs_vfat_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int mkfs_vfat_main(int argc UNUSED_PARAM, char **argv)
{
	mode_t st_mode;
	const char *arg_volume_label = bb_NO_NAME_11; //default
	const char *arg_sector_size;
	const char *arg_sect_per_clust;
	const char *arg_F;
	char volume_label11[12];
	char *buf;
	uoff_t volume_size_bytes;
	uoff_t volume_size_sect;
	uint32_t total_clust;
	uint32_t volume_id;
	int dev;
	unsigned bytes_per_sect;
	unsigned sect_per_fat;
	unsigned backup_boot_sct = 6; // match default of mkfs.fat 4.2
	unsigned reserved_sct = 32;   // match default of mkfs.fat 4.2
	unsigned opts;
	uint16_t sect_per_track;
	uint8_t media_byte;
	uint8_t sect_per_clust; // must be 8-bit (code expects that 256 won't fit)
	uint8_t heads_8bit;

	uint32_t max_clust_cnt;
	unsigned fat_entry_nibbles; // 3, 4 or 8
	unsigned rootdir_sct;
	unsigned rootdir_ent = 0;

	enum {
		//-A "make Atari variant": won't be supported - obsolete (early 1990s)
		//TODO: implement these as needed:
		//OPT_a = 1 << 0,  // [IGNORED] disable alignment of data structures
		//OPT_b = 1 << 1,  // location of backup boot sector, 0: none
		//OPT_c = 1 << 2,  // [IGNORED] check filesystem
		OPT_C = 1 << 3,    // create new file (fail if exists)
		//OPT_f = 1 << 4,  // [IGNORED] number of FATs
		OPT_F = 1 << 5,    // choose FAT size: 12/16/32
		//OPT_h = 1 << 6,  // [IGNORED] number of hidden sectors
		//OPT_I = 1 << 7,  // [IGNORED] don't bark at entire disk devices
		//OPT_i = 1 << 8,  // [IGNORED] volume ID
		//OPT_l = 1 << 9,  // [IGNORED] bad block filename
		//OPT_m = 1 << 10,  // [IGNORED] message file
		//OPT_n = 1 << 11, // volume label
		//OPT_r = 1 << 12, // [IGNORED] root directory entries
		OPT_R = 1 << 13,   // number of reserved sectors
		OPT_s = 1 << 14,   // sectors per cluster
		OPT_S = 1 << 15,   // sector size
		OPT_v = 1 << 16,   // verbose
	};

	opts = getopt32(argv, "^"
		"ab:+cCf:F:h:Ii:l:m:n:r:R:+s:S:v"
		"\0" "-1", //:b+:f+:F+:h+:r+:R+:s+:S+:vv:c--l:l--c
		/*b*/&backup_boot_sct,
		/*f*/NULL,
		/*F*/&arg_F, /*h*/NULL, /*i*/NULL, /*l*/NULL, /*m*/NULL,
		/*n*/&arg_volume_label, /*r*/NULL,
		/*R*/&reserved_sct,
		/*s*/&arg_sect_per_clust, /*S*/&arg_sector_size
	);
	argv += optind;

	// default volume ID: somewhat random (not crypto-strong)
	// (mix in pid to avoid same IDs when run in rapid succession)
	volume_id = time(NULL) ^ getpid();
	// truncate to exactly 11 chars, pad with spaces
	sprintf(volume_label11, "%-11.11s", arg_volume_label);

	dev = xopen(argv[0], (opts & OPT_C) ? (O_CREAT|O_EXCL|O_RDWR) : O_RDWR);
	st_mode = xfstat_mode(dev, argv[0]);

	//
	// Get image size and sector size
	//
	bytes_per_sect = SECTOR_SIZE;
	if (!S_ISBLK(st_mode)) {
		if (!S_ISREG(st_mode)) {
			if (!argv[1])
				bb_simple_error_msg_and_die("image size must be specified");
		}
		// not a block device, skip bad sectors check
//		opts &= ~OPT_c;
	} else {
		int min_bytes_per_sect;
#if 0
		unsigned device_num;
		// for true block devices we do check sanity
		device_num = st.st_rdev & 0xff3f;
		// do we allow to format the whole disk device?
		if (!(opts & OPT_I) && (
			device_num == 0x0300 || // hda, hdb
			(device_num & 0xff0f) == 0x0800 || // sd
			device_num == 0x0d00 || // xd
			device_num == 0x1600 )  // hdc, hdd
		)
			bb_error_msg_and_die("will not try to make filesystem on full-disk device (use -I if wanted)");
		// can't work on mounted filesystems
		if (find_mount_point(argv[0], 0))
			bb_error_msg_and_die("can't format mounted filesystem");
#endif
		// get true sector size
		// (parameter must be int*, not long* or size_t*)
		xioctl(dev, BLKSSZGET, &min_bytes_per_sect);
		if (min_bytes_per_sect > SECTOR_SIZE) {
			bytes_per_sect = min_bytes_per_sect;
			bb_error_msg("for this device sector size is %u", min_bytes_per_sect);
		}
	}

	if (opts & OPT_S) {
		// -S: upstream accepts hex too, accepts any power-of-2 from 512 to 0x8000, but sectors >4k
		// emit "Warning: sector size N > 4096 is non-standard, filesystem may not be usable"
		// and it's correct: Linux kernel does not support >4k
		bytes_per_sect = bb_strtou(arg_sector_size, NULL, 0);
		if (bytes_per_sect < 0x200 || bytes_per_sect > 0x8000
		 || (bytes_per_sect & (bytes_per_sect-1))
		) {
			bb_error_msg_and_die("bad -S '%s'", arg_sector_size);
		}
	}

	volume_size_bytes = get_volume_size_in_bytes(dev, argv[1], 1024,
			/*minsize:*/ 6 * bytes_per_sect | /*extend:*/ 1
		);
	volume_size_sect = volume_size_bytes / bytes_per_sect;
	dbg("volume_size_sect:%llu", ULL(volume_size_sect));

	//
	// Find out or guess media parameters
	//
	media_byte = 0xf8; // "hard disk"
	heads_8bit = 255;
	sect_per_track = 63;
	sect_per_clust = 1;
	{
		struct hd_geometry geometry;
		// size (in sectors), sect (per track), head
		struct floppy_struct floppy;

		// N.B. whether to use HDIO_GETGEO or HDIO_REQ?
		if (ioctl(dev, HDIO_GETGEO, &geometry) == 0
		 && geometry.sectors != 0
		 && geometry.heads != 0
		) {
			// hard drive
			sect_per_track = geometry.sectors;
			heads_8bit = geometry.heads;
 set_cluster_size:
			// For FAT32, try to do the same as M$'s format command
			// (see http://www.win.tue.nl/~aeb/linux/fs/fat/fatgen103.pdf p. 20):
			// size <= 260M: 0.5k clusters
			// size <=   8G: 4k clusters
			// size <=  16G: 8k clusters
			// size <=  32G: 16k clusters
			// else: 32k clusters // TODO: we don't implement this step, we max out at 16k
			if (volume_size_bytes >= 260*1024*1024) {
				sect_per_clust = 8;
				// fight gcc:
				// "error: integer overflow in expression"
				// "error: right shift count >= width of type"
				if (sizeof(off_t) > 4) {
					unsigned t = (volume_size_bytes >> 31 >> 1);
					if (t >= 8/4)
						sect_per_clust = 16;
					if (t >= 16/4)
						sect_per_clust = 32;
// with 2^32 (0x100000000) sectors and 32 sectors/cluster, there are 0x08000000 clusters
// (which fits into maximum allowed FAT32 cluster number, 0x0FFFFFF7)
// and each FAT table is 512 MBytes.
// 32 sectors/cluster stops being enough only at about twice as many sectors,
// which is currently not allowed anyway (total sector count must fit into a 32 bit header field).
				}
//FIXME: do we need to adjust for bigger sectors? sect_per_clust /= (bytes_per_sect/0x200)
				dbg("sect_per_clust:%d", sect_per_clust);
			}
		} else {
			// floppy, loop, or regular file
			int not_floppy = ioctl(dev, FDGETPRM, &floppy);
			if (not_floppy == 0) {
				// floppy disk
				sect_per_track = floppy.sect;
				heads_8bit = floppy.head;
				volume_size_sect = floppy.size;
				volume_size_bytes = floppy.size * SECTOR_SIZE;
			}
			// setup media descriptor byte and floppy-specific params
			switch (volume_size_sect) {
			case 2*360:	// 5.25", 2, 9, 40 - 360K
				media_byte = 0xfd;
				rootdir_ent = 112;
				sect_per_clust = 2; // dosfstools-4.2
				break;
			case 2*720:	// 3.5", 2, 9, 80 - 720K
				media_byte = 0xf9;
				rootdir_ent = 112;
				sect_per_clust = 2; // dosfstools-4.2
				break;
			case 2*1200:	// 5.25", 2, 15, 80 - 1200K
				media_byte = 0xf9;
				rootdir_ent = 224;
				//sect_per_clust = 1; // dosfstools-4.2
				break;
			default:	// anything else
				if (not_floppy)
					goto set_cluster_size;
				// non-standard sized floppy: use 2880k format's params
			case 2*2880:	// 3.5", 2, 36, 80 - 2880K
				sect_per_clust = 2; // dosfstools-4.2
				//^^^ necessary to make 2880k image use FAT12, not FAT16
			case 2*1440:	// 3.5", 2, 18, 80 - 1440K
				//sect_per_clust = 1; // dosfstools-4.2
				media_byte = 0xf0;
				rootdir_ent = 224;
				break;
			}
			// not floppy, but size matches floppy exactly.
			// perhaps it is a floppy image.
			// we already set media_byte as if it is a floppy,
			// now set sect_per_track and heads.
			heads_8bit = 2;
			sect_per_track = (unsigned)volume_size_sect / 160;
			if (sect_per_track < 9)
				sect_per_track = 9;
			if (!(opts & OPT_R))
				reserved_sct = 1;
		}
	}

	if (reserved_sct < 1 || reserved_sct > 0xffff /* won't fit in header field */)
		bb_error_msg_and_die("-R %u is invalid", reserved_sct);

	if (volume_size_sect > 0xFFFFFFFFu) {
		// FAT32 can't store this (modulo sparsely supported DR-DOS extension).
		// Match upstream message and adjust.
		// Upstream does not try to automatically increase sector size,
		// requires explicit -S SECTSIZE.
		// Example: mkfs.vfat -F32 -C FILE2TB $((1024*1024*1024*2))
		// should say that one sector is unused:
		bb_error_msg("target too large, unused sectors: %llu",
			ULL(volume_size_sect - 0xFFFFFFFFu));
		volume_size_sect = 0xFFFFFFFFu;
	}

// From now on we know it fits 32 bits
#define volume_size_sect ((uint32_t)volume_size_sect)

	if (opts & OPT_s) {
		// -s: upstream accepts hex too, accepts any power-of-2 from 1 to 0x80
		unsigned n = bb_strtou(arg_sect_per_clust, NULL, 0);
		if (n == 0 || n > 0x80 || (n & (n-1)))
			bb_error_msg_and_die("bad -s '%s'", arg_sect_per_clust);
		sect_per_clust = n; // uint8_t
	}

	if (opts & OPT_F) {
		int f = index_in_strings("12\0""16\0""32\0", arg_F);
		if (f < 0)
			bb_show_usage();
		if (f > 1)
			goto make_fat32;
		if (f != 1) {
 make_fat12:
			dbg("trying FAT12");
			fat_entry_nibbles = 3;
			max_clust_cnt = MAX_CLUST_CNT12;
		} else {
 make_fat16:
			dbg("trying FAT16");
			fat_entry_nibbles = 4;
			max_clust_cnt = MAX_CLUST_CNT16;
		}
		// mkfs.fat 4.2 simply sets rootdir_ent = 512. For small images,
		// we use rootdir size which is close to what "standard floppy" has.
		// The formula generates rootdirs as follows:
		//  360k floppy: 112 entries (standard, see code above)
		//  361k image:   96 entries (this formula)
		//  720k floppy: 112 entries (standard)
		//  721k image:  192 entries (this formula)
		// 1200k floppy: 224 entries (standard)
		// 1201k image:  304 entries (this formula)
		if (!rootdir_ent)
			rootdir_ent = volume_size_sect / 8 ? : 1;
		if (rootdir_ent > 512)
			rootdir_ent = 512;
		// Round up rootdir_ent so that rootdir fills its last sector
		while ((rootdir_ent * sizeof(struct msdos_dir_entry)) & (bytes_per_sect-1))
			rootdir_ent++;
		rootdir_sct = (rootdir_ent * sizeof(struct msdos_dir_entry)) / bytes_per_sect;
	} else {
 make_fat32:
		dbg("trying FAT32");
		fat_entry_nibbles = 8;
		max_clust_cnt = MAX_CLUST_CNT32;
		rootdir_sct = 0;
	}

	// Calculate number of clusters, sectors/cluster, sectors/FAT
	// (an initial guess for sect_per_clust should already be set)
	while (1) {
		dbg("sect_per_clust:%d", sect_per_clust);
		sect_per_fat = 1;
		while (1) {
			int spf_adj;
			total_clust = (volume_size_sect - reserved_sct - rootdir_sct - NUM_FATS * sect_per_fat);
			dbg("FAT%d sect_per_clust:%u sect_per_fat:%u reserved_sct:%u rootdir_sct:%u total_clust:%u 0x%08x",
				fat_entry_nibbles * 4, sect_per_clust, sect_per_fat,
				reserved_sct, rootdir_sct,
				(unsigned)total_clust, (unsigned)total_clust);
			if (total_clust > volume_size_sect) { // subtraction overflowed
 too_small:
				bb_error_msg_and_die("image is too small: %lld bytes", // common message
					ULL(volume_size_sect) * bytes_per_sect);
			}
			total_clust /= sect_per_clust;
			// "mkfs.vfat -v -F32 -b0 -R2 -s4 -C IMAGE_ZERO_CLUSTERS 3" has total_clust = 0
			if (total_clust == 0) // Linux would not mount such FS
				goto too_small;
			// total_clust may be > max_clust_cnt here, but it may be
			// because sect_per_fat is underestimated,
			// and with increased sect_per_fat it still may become
			// <= max_clust_cnt. Therefore, we do not check
			// against max_clust_cnt, but against a bigger const:
			if (total_clust >= max_clust_cnt * 2)
				goto next;
			// +2 since FAT has space for non-existent clusters 0 and 1.
			// Let's see how many sectors that needs.
// 32-bit overflow is impossible since total_clust <= 0x1ffffff0 and fat_entry_nibbles <= 8, 0x1fffffff*8 does not overflow
			spf_adj = ((total_clust+2)*fat_entry_nibbles + (bytes_per_sect*2)-1) // number of half-bytes needed, rounded up to full sector
					/ (bytes_per_sect*2) // divide by the number of half-bytes in one sector
				- sect_per_fat;
			dbg("adjust to sect_per_fat:%d", spf_adj);
			if (spf_adj <= 0) {
				// do not need to adjust sect_per_fat.
				// so, was total_clust too big after all?
				if (total_clust <= max_clust_cnt)
					goto found_total_clust; // no
				// yes, total_clust is _a bit_ too big
				goto next;
			}
			// adjust sect_per_fat, go back and recalc total_clust
			// (note: just "sect_per_fat += spf_adj" isn't ok)
			sect_per_fat += ((unsigned)spf_adj / 2) | 1;
		} // while (1)
 next:
		if (fat_entry_nibbles != 8 && !(opts & OPT_F)) {
			// retry as FAT16 or FAT32
			if (fat_entry_nibbles == 3)
				goto make_fat16;
			opts |= OPT_F; // do not try FAT16 anymore, we now know it does not fit
			goto make_fat32;
		}
		if (opts & OPT_s) // if -s SECTORS was given explicitly, do not auto-adjust
// upstream says "Not enough or too many clusters for filesystem - try less or more sectors per cluster"
			bb_error_msg_and_die("too small -s %s", arg_sect_per_clust);
		sect_per_clust *= 2;
		if (sect_per_clust == 0) // uint8_t overflowed: 128*2 = 0
			bb_simple_error_msg_and_die("can't make FAT32 with >128 sectors/cluster");
		// loop back and restart with initial 1-sector FAT size estimate
	} // while (1)
 found_total_clust:

	if (fat_entry_nibbles == 8) { // FAT32?
		if (total_clust <= MAX_CLUST_CNT16) { // maybe fits FAT12/16?
			if (!(opts & OPT_F)) // not explicit -F32?
				goto make_fat12; // go try to fit into FAT12
// NB: FATs shrink when you go 32->12/16, increasing new total_clust - this could
// make attempt to go to FAT16 invalid (too many clusters). In this case
// FAT size calc loop goes back to FAT32 but sets OPT_F, preventing another
// attempt of trying to fit into FAT16.
		}

		// Ok, we have to be FAT32. A few additional sanity checks...
		if (reserved_sct < 2)
			bb_error_msg_and_die("-R %u is invalid", reserved_sct);
		if (backup_boot_sct >= reserved_sct - 1)
			bb_error_msg_and_die("-b %u is too large", backup_boot_sct);
		// We allow -b1 (will write the value to header) but treat it as 0.
		// mkfs.fat 4.2 allows "-b N -R N+1" (meaning: backup of only bootsect,
		// but no backup of fsinfo). We do not allow this (not very useful, adds code).
	}

	//
	// Print info
	//
	if (opts & OPT_v) {
		fprintf(stderr,
			"%s has %u heads and %u sectors/track, bytes/sector:%u\n"
			"media descriptor:%02x\n"
			"total sectors:%u, clusters:%u, sectors/cluster:%u\n"
			"FATs:2, sectors/FAT:%u\n"
			"volumeID:%08x, label:'%s'\n",
// Example output from mkfs.vfat -v -F32 -S4096 -C IMAGE $((1024*1024*1024*2))
//mkfs.fat 4.2 (2021-01-31)
//IMAGE has 255 heads and 63 sectors per track,
//hidden sectors 0x0000;
//logical sector size is 4096,
//using 0xf8 media descriptor, with 536870880 sectors;
//drive number 0x80;
//filesystem has 2 32-bit FATs and 64 sectors per cluster.
//FAT size is 8192 sectors, and provides 8388350 clusters.
//There are 64 reserved sectors.
//Root directory contains 512 slots and uses 32 sectors. <--FAT12/16 adds this line
//Volume ID is bd24d78e, no volume label.
			argv[0],
			heads_8bit, sect_per_track, bytes_per_sect,
			(unsigned)media_byte,
			(unsigned)volume_size_sect, (unsigned)total_clust, (unsigned)sect_per_clust,
			sect_per_fat,
			(unsigned)volume_id, volume_label11
		);
		//fprintf(stderr,
		//	"root directory:%u slots\n",
		//	rootdir_ent
		//);
	}

	//
	// Write filesystem image sequentially (no seeking)
	//
	{
		unsigned bufsize = reserved_sct;
		// (a | b) is poor man's max(a, b)
		bufsize |= sect_per_clust;
		bufsize |= rootdir_sct;
		buf = xzalloc(bufsize * bytes_per_sect);
	}

#define STORE_BYTE_VALUE(var, val) ((uint8_t*)(&var))[0] = (val)
	{
		// Common for all FATnn
		struct msdos_boot_sector32 *boot_blk = (void*)buf;

		strcpy(boot_blk->boot_jump_and_sys_id, "\xeb\x58\x90" "mkdosfs");
		STORE_LE(boot_blk->bytes_per_sect, bytes_per_sect);
		boot_blk->sect_per_clust = sect_per_clust;
		// cast is needed on big endian to suppress a warning
		STORE_LE(boot_blk->reserved_sect, (uint16_t)reserved_sct);
		boot_blk->fats = 2;
		boot_blk->media_byte = media_byte;
		STORE_LE(boot_blk->sect_per_track, sect_per_track);
		STORE_BYTE_VALUE(boot_blk->heads, heads_8bit); // even though field is 16-bit, values are always < 256
		//STORE_LE(boot_blk->hidden, 0);
		STORE_LE(boot_blk->volume_size_sct32, volume_size_sect); // field exists in FAT12/16 too
		// we place boot code at the same offset for all types of FATnn
		memcpy(boot_blk->boot_code, boot_code, sizeof(boot_code));
		boot_blk->boot_sign = SWAP_LE16(BOOT_SIGN);
	}
	if (fat_entry_nibbles != 8) {
		// FAT12/16
		struct msdos_boot_sector *boot_blk = (void*)buf;

		STORE_LE(boot_blk->dir_entries, rootdir_ent);
		if (volume_size_sect <= 0xffff)
			STORE_LE(boot_blk->volume_size_sct16, (uint16_t)volume_size_sect);
		STORE_LE(boot_blk->fat_size_sct16, sect_per_fat);
		//STORE_LE(boot_blk->drive_number, 0); ???
		//STORE_LE(boot_blk->boot_flags, 0);
		boot_blk->ext_boot_sign = 0x29;
		boot_blk->volume_id32 = volume_id; // STORE_LE not used here since the id is random anyway, swap does not add anything
		memcpy(boot_blk->fs_type, "FAT16   ", sizeof(boot_blk->fs_type));
		if (fat_entry_nibbles == 3)
			boot_blk->fs_type[4] = '2';
		memcpy(boot_blk->volume_label, volume_label11, 11);
	} else {
		// FAT32: boot and fsinfo sectors, and their copies
		struct msdos_boot_sector32 *boot_blk = (void*)buf;
		struct fat32_fsinfo *info = (void*)(buf + bytes_per_sect);

		//STORE_LE(boot_blk->dir_entries, 0);
		//STORE_LE(boot_blk->volume_size_sct16, 0); // for FAT32, stays 0 (Microsoft says must be 0 for FAT32, Linux does not care)
		//STORE_LE(boot_blk->fat_size_sct16, 0); // for FAT32, stays 0. Linux treats this as FAT32 indicator
		STORE_LE(boot_blk->fat32_fat_size_sct32, sect_per_fat);
		//STORE_LE(boot_blk->fat32_flags, 0);
		//STORE_LE(boot_blk->fat32_version[2], 0,0);
		STORE_BYTE_VALUE(boot_blk->fat32_root_cluster, 2);
		STORE_BYTE_VALUE(boot_blk->fat32_info_sector, 1); // we use fixed info_sector_number = 1
		STORE_LE(boot_blk->fat32_backup_boot, backup_boot_sct);
		//boot_blk->fat32_boot_file filled with zeros
		//STORE_LE(boot_blk->fat32_drive_number, 0); ???
		//STORE_LE(boot_blk->fat32_boot_flags, 0);
		boot_blk->fat32_ext_boot_sign = 0x29;
		boot_blk->fat32_volume_id32 = volume_id; // STORE_LE not used here since the id is random anyway, swap does not add anything
		memcpy(boot_blk->fat32_fs_type, "FAT32   ", sizeof(boot_blk->fat32_fs_type));
		memcpy(boot_blk->fat32_volume_label, volume_label11, 11);

		info->signature1 = SWAP_LE32(FAT_FSINFO_SIG1);
		info->signature2 = SWAP_LE32(FAT_FSINFO_SIG2);
		// we've allocated cluster 2 for the root dir
		STORE_LE(info->free_clusters, (total_clust - 1));
		STORE_BYTE_VALUE(info->next_cluster, 2);
		info->boot_sign = SWAP_LE16(BOOT_SIGN);

		if (backup_boot_sct > 1) // if backup should exist, create it
			memcpy(buf + bytes_per_sect * backup_boot_sct, buf, bytes_per_sect * 2);
	}

	// fill entire area up to start of FATs
	xwrite(dev, buf, bytes_per_sect * reserved_sct);

	{ // file allocation tables
		unsigned i, j;
		unsigned value;

		memset(buf, 0, bytes_per_sect * 2);

		// initial FAT entries
#define MARK_CLUSTER(cluster, value) ((uint32_t *)buf)[cluster] = SWAP_LE32(value)
		value = 0x00ffff00 | media_byte;
		if (fat_entry_nibbles != 3)
			value |= 0xff000000;
		MARK_CLUSTER(0, value);
		if (fat_entry_nibbles == 8) {
			MARK_CLUSTER(1, EOF_FAT32);
			// mark cluster 2 as EOF (used for root dir)
			MARK_CLUSTER(2, EOF_FAT32);
		}
		for (i = 0; i < NUM_FATS; i++) {
			xwrite(dev, buf, bytes_per_sect);
//TODO: this is slow on very large images (FAT tables can be up to 1 GByte).
//Can try fallocate(FALLOC_FL_PUNCH_HOLE), then try fallocate(FALLOC_FL_ZERO_RANGE).
//Can have larger write buffer.
			for (j = 1; j < sect_per_fat; j++) {
				if (opts & OPT_C)
					// If -C, file has been created and extended from zero size,
					// no need to clear FAT tables.
					xlseek(dev, bytes_per_sect, SEEK_CUR);
				else
					xwrite(dev, buf + bytes_per_sect, bytes_per_sect);
			}
		}
	}

	// root directory
	if (fat_entry_nibbles == 8)
		rootdir_sct = sect_per_clust;
	dbg("rootdir_sct:%d", rootdir_sct);
	// empty directory is just zero bytes
	memset(buf, 0, rootdir_sct * bytes_per_sect);
	// mkfs.fat 4.1 won't create dir entry even with explicit -n 'NO NAME',
	// but will create one with e.g. -n '', -n '  zZz'
	// We _will_ create a 'NO NAME' volume label in root directory
	// when explicitly requested using -n option.
	if (arg_volume_label != bb_NO_NAME_11) {
		// create dir entry for volume label
		struct msdos_dir_entry *de;
#if 0
		struct tm tm_time;
		uint16_t t, d;
#endif
		de = (void*)buf;
		memcpy(de->name, volume_label11, 11);
		STORE_LE(de->attr, ATTR_VOLUME);
#if 0
		localtime_r(&create_time, &tm_time);
		t = (tm_time.tm_sec >> 1) + (tm_time.tm_min << 5) + (tm_time.tm_hour << 11);
		d = tm_time.tm_mday + ((tm_time.tm_mon+1) << 5) + ((tm_time.tm_year-80) << 9);
		STORE_LE(de->time, t);
		STORE_LE(de->date, d);
		//STORE_LE(de->ctime_cs, 0);
		de->ctime = de->time;
		de->cdate = de->date;
		de->adate = de->date;
#endif
		goto write_rootdir;
	}
	// If -C, file has been created and extended from zero size,
	// no need to clear rootdir. Else:
	if (!(opts & OPT_C)) {
 write_rootdir:
		xwrite(dev, buf, rootdir_sct * bytes_per_sect);
	}

#if 0
	if (opts & OPT_c) {
		uoff_t volume_size_blocks;
		unsigned start_data_sector;
		unsigned start_data_block;
		unsigned badblocks = 0;
		int try, got;
		off_t currently_testing;
		char *blkbuf = xmalloc(BLOCK_SIZE * TEST_BUFFER_BLOCKS);

		volume_size_blocks = (volume_size_bytes >> BLOCK_SIZE_BITS);
		// N.B. the two following vars are in hard sectors, i.e. SECTOR_SIZE byte sectors!
		start_data_sector = (reserved_sect + NUM_FATS * sect_per_fat) * (bytes_per_sect / SECTOR_SIZE);
		start_data_block = (start_data_sector + SECTORS_PER_BLOCK - 1) / SECTORS_PER_BLOCK;

		bb_error_msg("searching for bad blocks");
		currently_testing = 0;
		try = TEST_BUFFER_BLOCKS;
		while (currently_testing < volume_size_blocks) {
			if (currently_testing + try > volume_size_blocks)
				try = volume_size_blocks - currently_testing;
			// perform a test on a block. return the number of blocks
			// that could be read successfully.
			// seek to the correct location
			xlseek(dev, currently_testing * BLOCK_SIZE, SEEK_SET);
			// try reading
			got = read(dev, blkbuf, try * BLOCK_SIZE);
			if (got < 0)
				got = 0;
			if (got & (BLOCK_SIZE - 1))
				bb_error_msg("unexpected values in do_check: probably bugs");
			got /= BLOCK_SIZE;
			currently_testing += got;
			if (got == try) {
				try = TEST_BUFFER_BLOCKS;
				continue;
			}
			try = 1;
			if (currently_testing < start_data_block)
				bb_error_msg_and_die("bad blocks before data-area: cannot make fs");

			// mark all of the sectors in the block as bad
			for (i = 0; i < SECTORS_PER_BLOCK; i++) {
				int cluster = (currently_testing * SECTORS_PER_BLOCK + i - start_data_sector) / (int) (sect_per_clust) / (bytes_per_sect / SECTOR_SIZE);
				if (cluster < 0)
					bb_error_msg_and_die("invalid cluster number in mark_sector: probably bug!");
				MARK_CLUSTER(cluster, BAD_FAT32);
			}
			badblocks++;
			currently_testing++;
		}
		free(blkbuf);
		if (badblocks)
			bb_error_msg("%d bad block(s)", badblocks);
	}
#endif

	// cleanup
	if (ENABLE_FEATURE_CLEAN_UP) {
		free(buf);
		close(dev);
	}

	return 0;
}
