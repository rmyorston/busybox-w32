/* vi: set sw=4 ts=4: */
/*
 * fdisk.c -- Partition table manipulator for Linux.
 *
 * Copyright (C) 1992  A. V. Le Blanc (LeBlanc@mcc.ac.uk)
 * Copyright (C) 2001,2002 Vladimir Oleynik <dzo@simtreas.ru> (initial bb port)
 *
 * Licensed under GPLv2 or later, see file LICENSE in this source tree.
 */
//config:config FDISK
//config:	bool "fdisk (31 kb)"
//config:	default y
//config:	help
//config:	The fdisk utility is used to divide hard disks into one or more
//config:	logical disks, which are generally called partitions. This utility
//config:	can be used to list and edit the set of partitions or BSD style
//config:	'disk slices' that are defined on a hard drive.
//config:
//config:config FDISK_SUPPORT_LARGE_DISKS
//config:	bool "Support over 4GB disks"
//config:	default y
//config:	depends on FDISK
//config:	depends on !LFS   # with LFS no special code is needed
//config:
//config:config FEATURE_FDISK_BLKSIZE
//config:	bool "Support -s option to list sizes"
//config:	default y
//config:	depends on FDISK
//config:
//config:config FEATURE_FDISK_WRITABLE
//config:	bool "Write support"
//config:	default y
//config:	depends on FDISK
//config:	help
//config:	Enabling this option allows you to create or change a partition table
//config:	and write those changes out to disk. If you leave this option
//config:	disabled, you will only be able to view the partition table.
//config:
//config:config FEATURE_AIX_LABEL
//config:	bool "Support AIX disklabels"
//config:	default n
//config:	depends on FDISK && FEATURE_FDISK_WRITABLE
//config:	help
//config:	Enabling this option allows you to create or change AIX disklabels.
//config:	Most people can safely leave this option disabled.
//config:
//config:config FEATURE_OSF_LABEL
//config:	bool "Support BSD disklabels"
//config:	default n
//config:	depends on FDISK && FEATURE_FDISK_WRITABLE
//config:	help
//config:	Enabling this option allows you to create or change BSD disklabels
//config:	and define and edit BSD disk slices.
//config:	Most people can safely leave this option disabled.
//config:
//config:config FEATURE_GPT_LABEL
//config:	bool "Support GPT disklabels"
//config:	default n
//config:	depends on FDISK && FEATURE_FDISK_WRITABLE
//config:	help
//config:	Enabling this option allows you to view GUID Partition Table
//config:	disklabels.
//config:
//config:config FEATURE_FDISK_ADVANCED
//config:	bool "Support expert mode"
//config:	default y
//config:	depends on FDISK && FEATURE_FDISK_WRITABLE
//config:	help
//config:	Enabling this option allows you to do terribly unsafe things like
//config:	define arbitrary drive geometry, move the beginning of data in a
//config:	partition, and similarly evil things. Unless you have a very good
//config:	reason you would be wise to leave this disabled.

//applet:IF_FDISK(APPLET(fdisk, BB_DIR_SBIN, BB_SUID_DROP))

//kbuild:lib-$(CONFIG_FDISK) += fdisk.o

//usage:#define fdisk_trivial_usage
//usage:       "[-ul" IF_FEATURE_FDISK_BLKSIZE("s") "] "
//usage:       "[-C CYLINDERS] [-H HEADS] [-S SECTORS] [-b SSZ] [-t PARTTYPE] DISK"
//usage:#define fdisk_full_usage "\n\n"
//usage:	IF_FEATURE_FDISK_WRITABLE("Change")IF_NOT_FEATURE_FDISK_WRITABLE("Show")" partition table\n"
//usage:     "\n	-u		Start and End are in sectors (instead of cylinders)"
//usage:     "\n			Also disable rounding of sizes to cylinders"
//usage:     "\n	-l		Show partition table for each DISK and exit"
//usage:	IF_FEATURE_FDISK_BLKSIZE(
//usage:     "\n	-s		Show size in kb for each DISK and exit"
//NB: util-linux 2.41.1 says: "-s,--getsz: display device size in 512-byte sectors"
//but in fact, util-linux 2.41.1 shows the size in KILOBYTES!
//usage:	)
//usage:     "\n	-b 2048		(for certain MO disks) use 2048-byte sectors"
//usage:     "\n	-t PARTTYPE	Force 'dos' partition if 'gpt' also present"
//usage:     "\n	-C CYLINDERS	Set number of cylinders/heads/sectors"
//usage:     "\n	-H HEADS	Typically 255"
//usage:     "\n	-S SECTORS	Typically 63"

#ifndef _LARGEFILE64_SOURCE
/* For lseek64 */
# define _LARGEFILE64_SOURCE
#endif
#include <assert.h>             /* assert */
#include <sys/mount.h>
#include "libbb.h"
#include "unicode.h"
#if !defined(BLKSSZGET)
# define BLKSSZGET _IO(0x12, 104)
#endif
#if !defined(BLKGETSIZE64)
# define BLKGETSIZE64 _IOR(0x12,114,size_t)
#endif

#if 0
# define dbg(...) bb_error_msg(__VA_ARGS__)
#else
# define dbg(...) ((void)0)
#endif

/* Get device geometry in this struct: */
#define HDIO_GETGEO 0x0301
struct hd_geometry {
	unsigned char heads;
	unsigned char sectors;
	unsigned short cylinders;
	unsigned long start;
};

typedef unsigned long long ullong;

#if BB_LITTLE_ENDIAN
# define inline_if_little_endian ALWAYS_INLINE
#else
# define inline_if_little_endian /* nothing */
#endif

/* You can test 4K sector size in QEMU a-la:
 * qemu-img create -f qcow2 disk.qcow2 1g
 * qemu-system-x86_64 -enable-kvm \
 *   -device virtio-scsi-pci,id=scsi0 \
 *   -drive file=disk.qcow2,if=none,id=drv0,format=qcow2 \
 *   -device scsi-hd,drive=drv0,logical_block_size=4096,physical_block_size=4096
 */

#define DEFAULT_SECTOR_SIZE      512
#define DEFAULT_SECTOR_SIZE_STR "512"
#define MAX_SECTOR_SIZE         2048
#define SECTOR_SIZE              512 /* still used in OSF code */
#define MAXIMUM_PARTS             60

#define ACTIVE_FLAG             0x80

#define EXTENDED                0x05
#define WIN98_EXTENDED          0x0f
#define LINUX_PARTITION         0x81
#define LINUX_SWAP              0x82
#define LINUX_NATIVE            0x83
#define LINUX_EXTENDED          0x85
#define LINUX_LVM               0x8e
#define LINUX_RAID              0xfd

/* Used for sector numbers. Partition formats we know
 * (except GPT!) do not support more than 2^32 sectors.
 */
typedef uint32_t sector_t;
#if UINT_MAX == 0xffffffff
# define SECT_FMT ""
# define SECT_TYPE unsigned
#elif ULONG_MAX == 0xffffffff
# define SECT_FMT "l"
# define SECT_TYPE unsigned long
#else
# error Cant detect sizeof(uint32_t)
#endif

enum {
	OPT_b = 1 << 0,
	OPT_C = 1 << 1,
	OPT_H = 1 << 2,
	OPT_l = 1 << 3,
	OPT_S = 1 << 4,
	OPT_u = 1 << 5,
	OPT_t = 1 << 6,
	OPT_s = (1 << 7) * ENABLE_FEATURE_FDISK_BLKSIZE,
};
#define USER_SET_SECTOR_SIZE (option_mask32 & OPT_b)
#define NOWARN_OPT_ls        (!ENABLE_FEATURE_FDISK_WRITABLE || (option_mask32 & (OPT_l|OPT_s)))
#define DISPLAY_IN_CYL_UNITS (!(option_mask32 & OPT_u))
#define TOGGLE_DISPLAY_IN_CYL_UNITS (option_mask32 ^= OPT_u)

#define SUPPORT_DISKLABELS (0 \
	| ENABLE_FEATURE_AIX_LABEL \
	| ENABLE_FEATURE_OSF_LABEL \
	| ENABLE_FEATURE_GPT_LABEL \
)

struct dos_partition {
	uint8_t boot_ind;         /* 0x80 - active */
	uint8_t head;             /* starting head */
	uint8_t sector;           /* starting sector */
	uint8_t cyl;              /* starting cylinder */
	uint8_t sys_ind;          /* what partition type */
	uint8_t end_head;         /* end head */
	uint8_t end_sector;       /* end sector */
	uint8_t end_cyl;          /* end cylinder */
	uint8_t start4[4];        /* starting sector counting from 0 */
	uint8_t size4[4];         /* nr of sectors in partition */
} PACKED;

/*
 * per partition table entry data
 *
 * The four primary partitions have the same sectorbuffer (MBRbuffer)
 * and have NULL ext_pointer.
 * Each logical partition table entry has two pointers, one for the
 * partition and one link to the next one.
 */
struct pte {
	struct dos_partition *part_table;   /* points into sectorbuffer */
	struct dos_partition *ext_pointer;  /* points into sectorbuffer */
	sector_t offset_from_dev_start;     /* disk sector number */
	uint8_t *sectorbuffer;              /* disk sector contents */
#if ENABLE_FEATURE_FDISK_WRITABLE
	char changed; /* boolean */
#endif
};

#define unable_to_open "can't open '%s'"
#define unable_to_read "can't read '%s'"
#define unable_to_seek "can't seek '%s'"

enum label_type {
	LABEL_DOS, LABEL_AIX, LABEL_OSF, LABEL_GPT
};

#define LABEL_IS_DOS	(LABEL_DOS == current_label_type)

#if ENABLE_FEATURE_AIX_LABEL
#define LABEL_IS_AIX	(LABEL_AIX == current_label_type)
#define STATIC_AIX static
#else
#define LABEL_IS_AIX	0
#define STATIC_AIX extern
#endif

#if ENABLE_FEATURE_OSF_LABEL
#define LABEL_IS_OSF	(LABEL_OSF == current_label_type)
#define STATIC_OSF static
#else
#define LABEL_IS_OSF	0
#define STATIC_OSF extern
#endif

#if ENABLE_FEATURE_GPT_LABEL
#define LABEL_IS_GPT	(LABEL_GPT == current_label_type)
#define STATIC_GPT static
#else
#define LABEL_IS_GPT	0
#define STATIC_GPT extern
#endif

enum action { OPEN_MAIN, TRY_ONLY };

static void update_units(void);
#if ENABLE_FEATURE_FDISK_WRITABLE
static void change_units(void);
static void reread_partition_table(int leave);
static void delete_partition(int i);
static unsigned input_partition_number(int warn, unsigned max);
static void list_types(const char *const *sys);
static sector_t read_int(sector_t low, sector_t dflt, sector_t high, sector_t base, const char *mesg);
#endif
static const char *partition_type(uint8_t type);
static void get_geometry(void);
static void read_pte(struct pte *pe, sector_t offset);
#if ENABLE_FEATURE_FDISK_WRITABLE
static int get_boot(enum action what);
#else
static int get_boot(void);
#endif
static sector_t get_start_sect(const struct dos_partition *p);
static sector_t get_nr_sects(const struct dos_partition *p);

/* DOS partition types */

static const char *const i386_sys_types[] ALIGN_PTR = {
	"\x00" "Empty",
	"\x01" "FAT12",
	"\x04" "FAT16 <32M",
	"\x05" "Extended",         /* DOS 3.3+ extended partition */
	"\x06" "FAT16",            /* DOS 16-bit >=32M */
	"\x07" "HPFS/NTFS",        /* OS/2 IFS, eg, HPFS or NTFS or QNX */
	"\x0a" "OS/2 Boot Manager",/* OS/2 Boot Manager */
	"\x0b" "Win95 FAT32",
	"\x0c" "Win95 FAT32 (LBA)",/* LBA really is 'Extended Int 13h' */
	"\x0e" "Win95 FAT16 (LBA)",
	"\x0f" "Win95 Ext'd (LBA)",
	"\x11" "Hidden FAT12",
	"\x12" "Compaq diagnostics",
	"\x14" "Hidden FAT16 <32M",
	"\x16" "Hidden FAT16",
	"\x17" "Hidden HPFS/NTFS",
	"\x1b" "Hidden Win95 FAT32",
	"\x1c" "Hidden W95 FAT32 (LBA)",
	"\x1e" "Hidden W95 FAT16 (LBA)",
	"\x3c" "Part.Magic recovery",
	"\x41" "PPC PReP Boot",
	"\x42" "SFS",
	"\x63" "GNU HURD or SysV", /* GNU HURD or Mach or Sys V/386 (such as ISC UNIX) */
	"\x80" "Old Minix",        /* Minix 1.4a and earlier */
	"\x81" "Minix / old Linux",/* Minix 1.4b and later */
	"\x82" "Linux swap",       /* also Solaris */
	"\x83" "Linux",
	"\x84" "OS/2 hidden C: drive",
	"\x85" "Linux extended",
	"\x86" "NTFS volume set",
	"\x87" "NTFS volume set",
	"\x8e" "Linux LVM",
	"\x9f" "BSD/OS",           /* BSDI */
	"\xa0" "Thinkpad hibernation",
	"\xa5" "FreeBSD",          /* various BSD flavours */
	"\xa6" "OpenBSD",
	"\xa8" "Darwin UFS",
	"\xa9" "NetBSD",
	"\xab" "Darwin boot",
	"\xaf" "HFS / HFS+",
	"\xb7" "BSDI fs",
	"\xb8" "BSDI swap",
	"\xbe" "Solaris boot",
	"\xeb" "BeOS fs",
	"\xee" "EFI GPT",                    /* Intel EFI GUID Partition Table */
	"\xef" "EFI (FAT-12/16/32)",         /* Intel EFI System Partition */
	"\xf0" "Linux/PA-RISC boot",         /* Linux/PA-RISC boot loader */
	"\xf2" "DOS secondary",              /* DOS 3.3+ secondary */
	"\xf8" "EBBR protective",            /* Arm EBBR firmware protective partition */
	"\xfd" "Linux raid autodetect",      /* New (2.2.x) raid partition with
						autodetect using persistent
						superblock */
#if 0 /* ENABLE_WEIRD_PARTITION_TYPES */
	"\x02" "XENIX root",
	"\x03" "XENIX usr",
	"\x08" "AIX",              /* AIX boot (AIX -- PS/2 port) or SplitDrive */
	"\x09" "AIX bootable",     /* AIX data or Coherent */
	"\x10" "OPUS",
	"\x18" "AST SmartSleep",
	"\x24" "NEC DOS",
	"\x39" "Plan 9",
	"\x40" "Venix 80286",
	"\x4d" "QNX4.x",
	"\x4e" "QNX4.x 2nd part",
	"\x4f" "QNX4.x 3rd part",
	"\x50" "OnTrack DM",
	"\x51" "OnTrack DM6 Aux1", /* (or Novell) */
	"\x52" "CP/M",             /* CP/M or Microport SysV/AT */
	"\x53" "OnTrack DM6 Aux3",
	"\x54" "OnTrackDM6",
	"\x55" "EZ-Drive",
	"\x56" "Golden Bow",
	"\x5c" "Priam Edisk",
	"\x61" "SpeedStor",
	"\x64" "Novell Netware 286",
	"\x65" "Novell Netware 386",
	"\x70" "DiskSecure Multi-Boot",
	"\x75" "PC/IX",
	"\x93" "Amoeba",
	"\x94" "Amoeba BBT",       /* (bad block table) */
	"\xa7" "NeXTSTEP",
	"\xbb" "Boot Wizard hidden",
	"\xc1" "DRDOS/sec (FAT-12)",
	"\xc4" "DRDOS/sec (FAT-16 < 32M)",
	"\xc6" "DRDOS/sec (FAT-16)",
	"\xc7" "Syrinx",
	"\xda" "Non-FS data",
	"\xdb" "CP/M / CTOS / ...",/* CP/M or Concurrent CP/M or Concurrent DOS or CTOS */
	"\xde" "Dell Utility",     /* Dell PowerEdge Server utilities */
	"\xdf" "BootIt",           /* BootIt EMBRM */
	"\xe1" "DOS access",       /* DOS access or SpeedStor 12-bit FAT extended partition */
	"\xe3" "DOS R/O",          /* DOS R/O or SpeedStor */
	"\xe4" "SpeedStor",        /* SpeedStor 16-bit FAT extended partition <1024 cyl. */
	"\xf1" "SpeedStor",
	"\xf4" "SpeedStor",        /* SpeedStor large partition */
	"\xfe" "LANstep",          /* SpeedStor >1024 cyl. or LANstep */
	"\xff" "BBT",              /* Xenix Bad Block Table */
#endif
	NULL
};

enum {
	dev_fd = 3                  /* the disk */
};

#if ENABLE_FEATURE_GPT_LABEL
struct gpt_header;
#endif

/* Globals */
struct globals {
	char *line_ptr;

	const char *disk_device;
	// NB: g_partitions is never less than 4.
	// If some primary partitions are not populated, they still "exist".
	int g_partitions; // = 4;       // maximum partition + 1
	unsigned units_per_sector; // = 1;
	unsigned sector_size; // = DEFAULT_SECTOR_SIZE;
	// How much to advance after MBR or extended partition link
	// before starting next partition? Minimum is 1,
	// "DOS compatible" way is to skip to the end of track
	// (set it to g_sectors).
	unsigned offset_after_MBR_and_ext; // = 1;
	unsigned g_heads, g_sectors, g_cylinders;
	smallint /* enum label_type */ current_label_type;
#if ENABLE_FEATURE_OSF_LABEL
	smallint possibly_osf_label;
#endif
	smallint dos_compatible_flag;
#if ENABLE_FEATURE_OSF_LABEL
# if !defined(__alpha__)
	struct dos_partition *xbsd_part;
	unsigned xbsd_part_index;
# endif
#endif
	unsigned user_cylinders, user_heads, user_sectors;
	unsigned pt_heads, pt_sectors;
	unsigned kern_heads, kern_sectors;
	int ext_index;                  // the prime extended partition (0-3)
	sector_t extended_offset;       // offset of extended partition start
	sector_t total_number_of_sectors;

	const char *opt_t;
#if ENABLE_FEATURE_GPT_LABEL
	struct gpt_header *gpt_hdr;
	uint8_t *gpt_part_array;
	unsigned gpt_n_parts;
	unsigned gpt_part_entry_len;
#endif

	jmp_buf listingbuf;
	char line_buffer[80];
	/* Raw disk label. For DOS-type partition tables the MBR,
	 * with descriptions of the primary partitions. */
	uint8_t MBRbuffer[MAX_SECTOR_SIZE];
	/* Partition tables */
	struct pte ptes[MAXIMUM_PARTS];
};
#define G (*ptr_to_globals)
#define line_ptr             (G.line_ptr            )
#define disk_device          (G.disk_device         )
#define g_partitions         (G.g_partitions        )
#define units_per_sector     (G.units_per_sector    )
#define sector_size          (G.sector_size         )
#define offset_after_MBR_and_ext (G.offset_after_MBR_and_ext)
#define g_heads              (G.g_heads             )
#define g_sectors            (G.g_sectors           )
#define g_cylinders          (G.g_cylinders         )
#define current_label_type   (G.current_label_type  )
#define possibly_osf_label   (G.possibly_osf_label  )
#define dos_compatible_flag     (G.dos_compatible_flag    )
#define ext_index               (G.ext_index              )
#define user_cylinders          (G.user_cylinders         )
#define user_heads              (G.user_heads             )
#define user_sectors            (G.user_sectors           )
#define pt_heads                (G.pt_heads               )
#define pt_sectors              (G.pt_sectors             )
#define kern_heads              (G.kern_heads             )
#define kern_sectors            (G.kern_sectors           )
#define extended_offset         (G.extended_offset        )
#define total_number_of_sectors (G.total_number_of_sectors)
#define listingbuf      (G.listingbuf     )
#define line_buffer     (G.line_buffer    )
#define MBRbuffer       (G.MBRbuffer      )
#define ptes            (G.ptes           )
#define INIT_G() do { \
	SET_PTR_TO_GLOBALS(xzalloc(sizeof(G))); \
	sector_size = DEFAULT_SECTOR_SIZE; \
	offset_after_MBR_and_ext = 1; \
	g_partitions = 4; \
	units_per_sector = 1; \
	/* off by default in util-linux 2.41.1: */ \
	/* dos_compatible_flag = 1; */ \
} while (0)

/* TODO: move to libbb? */
static uint64_t bb_getsize_in_512sect(int fd)
{
	const char *how;
	uint64_t sz64;
	/* BLKGETSIZE64 takes pointer to uint64_t, not ullong: */
	uint64_t sz_get64;
	/* BLKGETSIZE takes pointer to ulong: */
	unsigned long sectors_long;

	if (ioctl(fd, BLKGETSIZE, &sectors_long) == 0
	) {
		/* If in "fdisk -b 4096 /dev/512byte_sect_dev" scenario,
		 * IOW: sector size is OVERRIDDEN by user,
		 * the user tries to create 4K sector layout on a 512 sector
		 * device (presumably because the device can't operate
		 * in 4K mode on this machine, but can on another one?):
		 * __do NOT assume BLKGETSIZE returns 4K sectors__ -
		 * it always returns 512-sized "sectors"!
		 */
		sz64 = (uint64_t)sectors_long * 512;
		how = "BLKGETSIZE";
	} else {
		/* BLKGETSIZE failed, assume this is a disk image */
#if ENABLE_FDISK_SUPPORT_LARGE_DISKS
		off64_t sz = lseek64(fd, 0, SEEK_END);
		if (sz == (off64_t)-1)
			bb_perror_msg_and_die("lseek(%llu, %d)", 0ULL, SEEK_END);
#else
		off_t sz = xlseek(fd, 0, SEEK_END);
#endif
		xlseek(fd, 0, SEEK_SET);
		sz64 = sz;
		how = "seek";
	}
	/* We did the above in case BLKGETSIZE64 is not available.
	 * If it is, double-check:
	 */
	if (ioctl(fd, BLKGETSIZE64, &sz_get64) == 0
	 && sz_get64 != sz64
	) {
		/* Something is very fishy. Abort. */
		bb_error_msg_and_die(
			"bad sizes: BLKGETSIZE64:%llu %s:%llu",
			(ullong)sz_get64,
			how, (ullong)sz64
		);
	}
#if 0 /* this breaks "fdisk -s FILE" (util-linux 2.41.1 silently rounds down) */
	if ((sz64 & 0x1ff) != 0)
		bb_error_msg("size %llu not a multiple of 512 bytes", (ullong)sz64);
#endif

	/* Got bytes, convert to 512 byte blocks */
	return sz64 >> 9;
}

static sector_t bb_getsize_in_sectors(int fd)
{
	uint64_t sz64 = bb_getsize_in_512sect(fd) / (sector_size >> 9);
	if (sz64 != (sector_t)sz64) {
		/* Most partition tables we support can't record
		 * more than 32 bit sector counts or offsets.
		 * (GPT can)
		 */
		bb_simple_error_msg("device has more than 2^32 sectors, can't use all of them");
		sz64 = (sector_t)-1LL;
	}
	return (sector_t)sz64;
}

#define IS_EXTENDED(i) \
	((i) == EXTENDED || (i) == WIN98_EXTENDED || (i) == LINUX_EXTENDED)

#define cround(n)       (DISPLAY_IN_CYL_UNITS ? ((n)/units_per_sector)+1 : (n))

#define scround(x)      (((x)+units_per_sector-1)/units_per_sector)

#define pt_offset(b, n) \
	((struct dos_partition *)((b) + 0x1be + (n) * sizeof(struct dos_partition)))

#define sector(s)       ((s) & 0x3f)

#define cylinder(s, c)  ((c) | (((s) & 0xc0) << 2))

static void
close_dev_fd(void)
{
	/* Not really closing, but making sure it is open, and to harmless place */
	xmove_fd(xopen(bb_dev_null, O_RDONLY), dev_fd);
}

/* Return partition name */
static const char *
partname(const char *dev, int pno, int lth)
{
	const char *p;
	int w, wp;
	int bufsiz;
	char *bufp;

	bufp = auto_string(xzalloc(80));
	bufsiz = 80;

	w = strlen(dev);
	p = "";
	if (isdigit(dev[w - 1]))
		p = "p";
	wp = (p[0] != '\0'); //=strlen(p)

	if (lth) {
		snprintf(bufp, bufsiz, "%*.*s%s%-2u",
			lth - wp - 2, w, dev, p, pno);
	} else {
		snprintf(bufp, bufsiz, "%.*s%s%-2u", w, dev, p, pno);
	}
	return bufp;
}

#if ENABLE_FEATURE_OSF_LABEL
static ALWAYS_INLINE struct dos_partition *
get_part_table(int i)
{
	return ptes[i].part_table;
}
#endif

static ALWAYS_INLINE const char *
str_units(void)
{
	return DISPLAY_IN_CYL_UNITS ? "cylinder" : "sector";
}

static int
valid_55AA_signature(const uint8_t *mbuffer)
{
	return (mbuffer[510] == 0x55 && mbuffer[511] == 0xaa);
}

static void fdisk_fatal(const char *why)
{
	if (!ENABLE_FEATURE_FDISK_WRITABLE /* this assumes -l */
	 || (option_mask32 & OPT_l)
	) {
		close_dev_fd();
		longjmp(listingbuf, 1);
	}
	bb_error_msg_and_die(why, disk_device);
}

static void
seek_sector(sector_t secno)
{
#if ENABLE_FDISK_SUPPORT_LARGE_DISKS
	off64_t off = (off64_t)secno * sector_size;
	if (lseek64(dev_fd, off, SEEK_SET) == (off64_t) -1)
		fdisk_fatal(unable_to_seek);
#else
	uint64_t off = (uint64_t)secno * sector_size;
	if (off > MAXINT(off_t)
	 || lseek(dev_fd, (off_t)off, SEEK_SET) == (off_t) -1
	) {
		fdisk_fatal(unable_to_seek);
	}
#endif
}

#if ENABLE_FEATURE_FDISK_WRITABLE
static void
set_all_unchanged(void)
{
	int i;

	for (i = 0; i < MAXIMUM_PARTS; i++)
		ptes[i].changed = 0;
}

static ALWAYS_INLINE void
set_changed(int i)
{
	ptes[i].changed = 1;
}

static ALWAYS_INLINE void
write_55AA_signature(uint8_t *b)
{
	b[510] = 0x55;
	b[511] = 0xaa;
}

/* Read line; return 0 or first printable non-space char */
static int
read_line(const char *prompt)
{
	int sz;

	sz = read_line_input(NULL, prompt, line_buffer, sizeof(line_buffer));
	if (sz <= 0)
		exit_SUCCESS(); /* Ctrl-D or Ctrl-C */

	if (line_buffer[sz-1] == '\n')
		line_buffer[--sz] = '\0';

	line_ptr = line_buffer;
	while (*line_ptr != '\0' && (unsigned char)*line_ptr <= ' ')
		line_ptr++;
	return *line_ptr;
}

static char
read_nonempty(const char *mesg)
{
	while (!read_line(mesg))
		continue;
	return *line_ptr;
}

static char
read_maybe_empty(const char *mesg)
{
	if (!read_line(mesg)) {
		line_ptr = line_buffer;
		line_ptr[0] = '\n';
		line_ptr[1] = '\0';
	}
	return line_ptr[0];
}

static int
read_hex(const char *const *sys)
{
	unsigned long v;
	while (1) {
		read_nonempty("Hex code (type L to list codes): ");
		if ((line_ptr[0] | 0x20) == 'l') {
			list_types(sys);
			continue;
		}
		v = bb_strtoul(line_ptr, NULL, 16);
		if (v <= 0xff)
			return v;
	}
}

static void
write_sector(sector_t secno, const void *buf)
{
	seek_sector(secno);
	xwrite(dev_fd, buf, sector_size);
}
#endif /* FEATURE_FDISK_WRITABLE */


#include "fdisk_aix.c"

STATIC_OSF void if_osf_label_loop_in_menu_until_r(void);
STATIC_OSF void xbsd_print_disklabel(int);
#include "fdisk_osf.c"

STATIC_GPT void gpt_print_disklabel(void);
#include "fdisk_gpt.c"

static inline_if_little_endian uint32_t
read4_little_endian(const uint8_t *cp)
{
	uint32_t v;
	move_from_unaligned32(v, cp);
	return SWAP_LE32(v);
}

static sector_t
get_start_sect(const struct dos_partition *p)
{
	return read4_little_endian(p->start4);
}

static sector_t
get_nr_sects(const struct dos_partition *p)
{
	return read4_little_endian(p->size4);
}

#if ENABLE_FEATURE_FDISK_WRITABLE
/* start_sect and nr_sects are stored little endian on all machines */
/* moreover, they are not aligned correctly */
static inline_if_little_endian void
store4_little_endian(uint8_t *cp, uint32_t val)
{
	uint32_t v = SWAP_LE32(val);
	move_to_unaligned32(cp, v);
}

static void
set_start_sect(struct dos_partition *p, unsigned start_sect)
{
	store4_little_endian(p->start4, start_sect);
}

static void
set_nr_sects(struct dos_partition *p, unsigned nr_sects)
{
	store4_little_endian(p->size4, nr_sects);
}
#endif

/* Allocate a buffer and read a partition table sector */
static void
read_pte(struct pte *pe, sector_t offset)
{
	pe->offset_from_dev_start = offset;
	pe->sectorbuffer = xzalloc(sector_size);
	seek_sector(offset);
	/* xread would make us abort - bad for fdisk -l */
	if (full_read(dev_fd, pe->sectorbuffer, sector_size) != sector_size)
		fdisk_fatal(unable_to_read);
	IF_FEATURE_FDISK_WRITABLE(pe->changed = 0;)
	pe->part_table = pe->ext_pointer = NULL;
}

static sector_t
get_partition_start_from_dev_start(const struct pte *pe)
{
	return pe->offset_from_dev_start + get_start_sect(pe->part_table);
}

#define get_sys_types() i386_sys_types

static const char *
partition_type(uint8_t type)
{
	int i;
	const char *const *types = get_sys_types();

	for (i = 0; types[i]; i++)
		if ((uint8_t)types[i][0] == type)
			return types[i] + 1;

	return "Unknown";
}

static int
is_cleared_partition(const struct dos_partition *p)
{
	/* We consider partition "cleared" only if it has only zeros */
	const uint8_t *cp = (const uint8_t *)p;
	int cnt = sizeof(*p);
	char bits = 0;
	while (--cnt >= 0)
		bits |= *cp++;
	return (bits == 0);
}

static void
clear_partition(struct dos_partition *p)
{
	if (p)
		memset(p, 0, sizeof(*p));
}

#if ENABLE_FEATURE_FDISK_WRITABLE
static int
get_sysid(int i)
{
	return ptes[i].part_table->sys_ind;
}

static void
list_types(const char *const *sys)
{
	enum { COLS = 3 };

	unsigned last[COLS];
	unsigned done, next, size;
	int i;

	for (size = 0; sys[size]; size++)
		continue;

	done = 0;
	for (i = COLS-1; i >= 0; i--) {
		done += (size + i - done) / (i + 1);
		last[COLS-1 - i] = done;
	}

	i = done = next = 0;
	do {
		printf("%c%2x %-22.22s", i ? ' ' : '\n',
			(unsigned char)sys[next][0],
			sys[next] + 1);
		next = last[i++] + done;
		if (i >= COLS || next >= last[i]) {
			i = 0;
			next = ++done;
		}
	} while (done < last[0]);
	bb_putchar('\n');
}
#endif // WRITABLE

static int
warn_geometry(void)
{
	if (g_heads && g_sectors && g_cylinders)
		return 0;

	printf("Unknown value(s) for:");
	if (!g_heads)
		printf(" heads");
	if (!g_sectors)
		printf(" sectors");
	if (!g_cylinders)
		printf(" cylinders");
#if ENABLE_FEATURE_FDISK_WRITABLE
	puts(" (settable in the extra functions menu)");
#else
	bb_putchar('\n');
#endif
	return 1;
}

static void
update_units(void)
{
	int cyl_units = g_heads * g_sectors;

	if (DISPLAY_IN_CYL_UNITS && cyl_units)
		units_per_sector = cyl_units;
	else
		units_per_sector = 1;   /* in sectors */
}

#if ENABLE_FEATURE_FDISK_WRITABLE
static void
warn_cylinders(void)
{
	if (LABEL_IS_DOS && g_cylinders > 1024 && !NOWARN_OPT_ls)
		printf(
"The number of cylinders for this disk is set to %u.\n"
"There is nothing wrong with that, but this is larger than 1024,\n"
"and could in certain setups cause problems with:\n"
"1) software that runs at boot time (e.g., old versions of LILO)\n"
"2) booting and partitioning software from other OSs\n"
"   (e.g., DOS FDISK, OS/2 FDISK)\n",
			g_cylinders);
}
#endif

// linux/block/partitions/msdos.c:
// ...
// The logical partitions form a linked list, with each entry being
// a partition table with two entries.  The first entry
// is the real data partition (with a start relative to the partition
// table start).  The second is a pointer to the next logical partition
// (with a start **relative to the entire extended partition**).
// We do not create a Linux partition for the partition tables, but
// only for the actual data partitions.
// ...
// Usually, the first entry is the real data partition,
// the 2nd entry is the next extended partition, or empty,
// and the 3rd and 4th entries are unused.
// However, DRDOS sometimes has the extended partition as
// the first entry (when the data partition is empty),
// and OS/2 **seems to use all four entries**.
//
// Due to the above, there can be up to four data partitions
// in a link of this "extended partition chain".
// ^^^ we do not handle this case
//
// However, kernel code uses only the first "next extended partition" entry
// it finds in the current link, and ignores any other "extended partitions":
// "tree of extended partitions" is not a thing.
//
// Kernel code requires that the links have nr_sects != 0
// (otherwise it is not considered to be "extended partition entry").
// The starting link, visible as one of first 4 partitions,
// is a real block device (!) but its size is capped to 2 sectors
// regardless of how large ns_sects field is.
// Kernel does not complain if this tiny partition intrudes into first sector
// of the next partition (!!!) which happens to start in the very next sector.
static void
read_extended_chain(int ext)
{
	int i;
	struct pte *pex;
	struct dos_partition *p;

	ext_index = ext;
	pex = &ptes[ext];
	pex->ext_pointer = pex->part_table;

	p = pex->part_table;
	if (get_start_sect(p) == 0) {
		puts("Zero offset in primary extended partition");
		return;
	}
	//TODO: check nr_sect != 0 too?

	while (IS_EXTENDED(p->sys_ind)) {
		struct dos_partition *pt0;
		struct pte *pe = &ptes[g_partitions];

		if (g_partitions >= MAXIMUM_PARTS) {
			/* This is not a Linux restriction, but
			   this program uses arrays of size MAXIMUM_PARTS.
			   Do not try to 'improve' this test. */
			struct pte *pre = &ptes[g_partitions - 1];
#if ENABLE_FEATURE_FDISK_WRITABLE
			printf("Warning: deleting partitions after %u\n",
				g_partitions);
			pre->changed = 1;
#endif
			clear_partition(pre->ext_pointer);
			return;
		}

		read_pte(pe, extended_offset + get_start_sect(p));
		// ^^^ does not return on failure to seek or read

		if (extended_offset == 0) // remember base ofs of whole extended partition
			extended_offset = get_start_sect(p);

		pt0 = p = pt_offset(pe->sectorbuffer, 0);
		for (i = 0; i < 4; i++, p++) {
			if (get_nr_sects(p) == 0)
				continue;
			if (IS_EXTENDED(p->sys_ind)) {
				if (pe->ext_pointer)
					printf("Warning: extra %s "
						"pointer in ext.partition chain"
						" %u\n", "link", g_partitions + 1);
				else
					pe->ext_pointer = p;
			} else if (!is_cleared_partition(p)) {
				if (pe->part_table)
					printf("Warning: extra %s "
						"pointer in ext.partition chain"
						" %u\n", "data", g_partitions + 1);
				else
					pe->part_table = p;
			}
		}

		// Make sure that if not found, data/link pointers
		// do terminate - point them to invalid entries
		// (hopefully? Imagine a link with 4 "next ext" ptrs:
		// none of them have manifestly invalid form!!!)
//FIXME: allow them to dangle?
		if (!pe->part_table) {
			if (pe->ext_pointer != pt0)
				pe->part_table = pt0;
			else
				pe->part_table = pt0 + 1;
		}
		if (!pe->ext_pointer) {
			if (pe->part_table != pt0)
				pe->ext_pointer = pt0;
			else
				pe->ext_pointer = pt0 + 1;
		}

		p = pe->ext_pointer;
		g_partitions++;
	}

#if ENABLE_FEATURE_FDISK_WRITABLE
	// Remove empty data partitions in extended chain
 remove:
	for (i = 4; i < g_partitions; i++) {
		struct pte *pe = &ptes[i];
		if (get_nr_sects(pe->part_table) == 0) {
			if (g_partitions == 5 && is_cleared_partition(pe->part_table)) {
				// empty extended partition
				// (need to keep it for its sector buffer?)
			} else {
				printf("Omitting empty partition (%u)\n", i + 1);
				delete_partition(i);
				goto remove;    // numbering changed
			}
		}
	}
#endif
}

#if ENABLE_FEATURE_FDISK_WRITABLE
static void
create_doslabel(void)
{
	// util-linux 2.41.1 says:
	// "Created a new DOS (MBR) disklabel with disk identifier 0xdeafbead."
	puts("Created a new DOS (MBR) disklabel.");

	current_label_type = LABEL_DOS;
	IF_FEATURE_OSF_LABEL(possibly_osf_label = 0;)

	g_partitions = 4;

	memset(&MBRbuffer[510 - 4*16], 0, 4*16);
	write_55AA_signature(MBRbuffer);
	extended_offset = 0;
	set_all_unchanged();
	set_changed(0);
}
#endif

static void
get_sectorsize(void)
{
	if (!USER_SET_SECTOR_SIZE) {
		int arg;
		if (ioctl(dev_fd, BLKSSZGET, &arg) == 0 && arg >= 512)
			sector_size = arg;
		if (sector_size != DEFAULT_SECTOR_SIZE)
			printf("Note: sector size is %u "
				"(not " DEFAULT_SECTOR_SIZE_STR ")\n",
				sector_size);
	}
}

static void
get_kernel_geometry(void)
{
	struct hd_geometry geometry;

	if (!ioctl(dev_fd, HDIO_GETGEO, &geometry)) {
		kern_heads = geometry.heads;
		kern_sectors = geometry.sectors;
		/* never use geometry.cylinders - it is truncated */
	}
}

static void
get_partition_table_geometry(void)
{
	const uint8_t *bufp = MBRbuffer;
	struct dos_partition *p;
	int i, h, s, hh, ss;
	int first = 1;
	int bad = 0;

	if (!(valid_55AA_signature(bufp)))
		return;

	hh = ss = 0;
	for (i = 0; i < 4; i++) {
		p = pt_offset(bufp, i);
		if (!is_cleared_partition(p)) {
			h = p->end_head + 1;
			s = (p->end_sector & 0x3f);
			if (first) {
				hh = h;
				ss = s;
				first = 0;
			} else if (hh != h || ss != s)
				bad = 1;
		}
	}

	if (!first && !bad) {
		pt_heads = hh;
		pt_sectors = ss;
	}
}

static void
get_geometry(void)
{
	get_sectorsize();

	g_heads = g_cylinders = g_sectors = 0;
	kern_heads = kern_sectors = 0;
	pt_heads = pt_sectors = 0;

	get_kernel_geometry();
	get_partition_table_geometry();

	g_heads = user_heads ? user_heads :
		pt_heads ? pt_heads :
		kern_heads ? kern_heads : 255;
	g_sectors = user_sectors ? user_sectors :
		pt_sectors ? pt_sectors :
		kern_sectors ? kern_sectors : 63;
	total_number_of_sectors = bb_getsize_in_sectors(dev_fd);

	offset_after_MBR_and_ext = 1;
	if (dos_compatible_flag)
		offset_after_MBR_and_ext = g_sectors;

	g_cylinders = total_number_of_sectors / (g_heads * g_sectors);
//TODO? if (total_number_of_sectors % (g_heads * g_sectors) != 0) g_cylinders++;
	if (!g_cylinders)
		g_cylinders = user_cylinders;
}

static void
dos_reset_primary_partition_info(void)
{
	int i;
	g_partitions = 4;
	for (i = 0; i < 4; i++) {
		struct pte *pe = &ptes[i];
		pe->part_table = pt_offset(MBRbuffer, i);
		pe->ext_pointer = NULL;
		pe->offset_from_dev_start = 0;
		pe->sectorbuffer = MBRbuffer;
		IF_FEATURE_FDISK_WRITABLE(pe->changed = 0;)
	}
}

// Opens disk_device and optionally reads MBR.
//   If what == OPEN_MAIN:
//     Open device, read MBR.  Abort program on short read.  Create empty
//     disklabel if the on-disk structure is invalid (WRITABLE mode).
//   If what == TRY_ONLY:
//     Open device, read MBR.  Return an error if anything is out of place.
//     Do not create an empty disklabel.  This is used for the "list"
//     operations: "fdisk -l /dev/sda" and "fdisk -l" (all devices).
// Returns:
//   -1: no 0xaa55 flag present (possibly entire disk BSD)
//   0: found or created label
//   1: I/O error, or can't read 512 bytes.
#if ENABLE_FEATURE_FDISK_WRITABLE
static int get_boot(enum action what)
#else
static int get_boot(void)
#define get_boot(what) get_boot()
#endif
{
	int i, fd;

	dos_reset_primary_partition_info();

#if ENABLE_FEATURE_FDISK_WRITABLE
	fd = open(disk_device, (option_mask32 & OPT_l) ? O_RDONLY : O_RDWR);
	if (fd < 0) {
		fd = open(disk_device, O_RDONLY);
		if (fd < 0) {
			if (what == TRY_ONLY)
				return 1;
			fdisk_fatal(unable_to_open);
		}
		printf("'%s' is opened for read only\n", disk_device);
	}
	xmove_fd(fd, dev_fd);
	if (512 != full_read(dev_fd, MBRbuffer, 512)) {
		if (what == TRY_ONLY) {
			close_dev_fd();
			return 1;
		}
		fdisk_fatal(unable_to_read);
	}
#else
	fd = open(disk_device, O_RDONLY);
	if (fd < 0)
		return 1;
	xmove_fd(fd, dev_fd);
	if (512 != full_read(dev_fd, MBRbuffer, 512)) {
		close_dev_fd();
		return 1;
	}
#endif

	get_geometry();
	update_units();

#if ENABLE_FEATURE_AIX_LABEL
	if (check_aix_label())
		return 0;
#endif
#if ENABLE_FEATURE_GPT_LABEL
	if (check_gpt_label())
		return 0;
#endif
#if ENABLE_FEATURE_OSF_LABEL
	if (check_osf_label()) {
		possibly_osf_label = 1;
		if (!valid_55AA_signature(MBRbuffer)) {
			current_label_type = LABEL_OSF;
			return 0;
		}
		puts("This disk has both DOS and BSD magic.\n"
		     "Give the 'b' command to go to BSD mode.");
	}
#endif

#if !ENABLE_FEATURE_FDISK_WRITABLE
	if (!valid_55AA_signature(MBRbuffer))
		return -1;
#else
	if (!valid_55AA_signature(MBRbuffer)) {
		if (what == TRY_ONLY)
			return -1;
		// OPEN_MAIN:
		// util-linux 2.41.1 says: "Device does not contain a recognized partition table."
		puts("Device has no valid DOS"
# if SUPPORT_DISKLABELS
			IF_FEATURE_OSF_LABEL(", OSF")
			IF_FEATURE_AIX_LABEL(", AIX")
			IF_FEATURE_GPT_LABEL(", GPT")
# endif
			" partition table."
		);
		create_doslabel();
	}
	warn_cylinders();
#endif /* FEATURE_FDISK_WRITABLE */
	warn_geometry();

	// We saw 55AA signature in MBR, this is considered enough to conclude
	// partition table exists (for fdisk -l. Without -l, the requirement
	// is that there are at least 512 bytes in this disk, that's all).
	for (i = 0; i < 4; i++) {
		if (IS_EXTENDED(ptes[i].part_table->sys_ind)) {
			if (g_partitions != 4)
				printf("Ignoring extra extended "
					"partition %u\n", i + 1);
			else
				read_extended_chain(i);
		}
	}

	for (i = 3; i < g_partitions; i++) {
		struct pte *pe = &ptes[i];
		if (!valid_55AA_signature(pe->sectorbuffer)) {
			printf("Warning: invalid flag 0x%02x,0x%02x of partition "
				"table %u will be corrected by w(rite)\n",
				pe->sectorbuffer[510],
				pe->sectorbuffer[511],
				i + 1);
			IF_FEATURE_FDISK_WRITABLE(pe->changed = 1;)
		}
	}

	return 0;
}

#if ENABLE_FEATURE_FDISK_WRITABLE
/*
 * Print the message MESG, then read an integer between LOW and HIGH (inclusive).
 * If the user hits Enter, DFLT is returned.
 * Answers like +10 are interpreted as offsets from BASE.
 *
 * There is no default if DFLT is not between LOW and HIGH.
 */
static sector_t
read_int(sector_t low, sector_t dflt, sector_t high, sector_t base, const char *mesg)
{
	sector_t value;
	int default_ok;
	const char *fmt;

	if (low == high) // no need to ask
		return low;

	default_ok = 1;
	fmt = "%s (%u-%u, default %u): ";
	if (dflt < low || dflt > high) {
		fmt = "%s (%u-%u): ";
		default_ok = 0;
	}
	while (1) {
		int use_default = default_ok;

		/* ask question and read answer */
		do {
			printf(fmt, mesg, low, high, dflt);
			read_maybe_empty("");
		} while (*line_ptr != '\n' && !isdigit(*line_ptr)
		 && *line_ptr != '-' && *line_ptr != '+');

		if (*line_ptr == '+' || *line_ptr == '-') {
			int minus = (*line_ptr == '-');
			unsigned scale_shift;

			if (sizeof(value) <= sizeof(long))
				value = strtoul(line_ptr + 1, NULL, 10);
			else
				value = strtoull(line_ptr + 1, NULL, 10);

			/* (1) if 2nd char is digit, use_default = 0.
			 * (2) move line_ptr to first non-digit.
			 */
			while (isdigit(*++line_ptr))
				use_default = 0;

			scale_shift = 0;
			switch (*line_ptr | 0x20) {
			case 'k':
				scale_shift = 10; /* 1024 */
				break;
/*
 * fdisk from util-linux 2.31 seems to round '+NNNk' and '+NNNK' to megabytes,
 * (512-byte) sector count of the partition does not equal NNN*2:
 *
 * Last sector, +sectors or +size{K,M,G,T,P} (1953792-1000215215, default 1000215215): +9727k
 *   Device     Boot   Start     End Sectors  Size Id Type
 *   /dev/sdaN       1953792 1972223   18432    9M 83 Linux   <-- size exactly 9*1024*1024 bytes
 *
 * Last sector, +sectors or +size{K,M,G,T,P} (1953792-1000215215, default 1000215215): +9728k
 *   /dev/sdaN       1953792 1974271   20480   10M 83 Linux   <-- size exactly 10*1024*1024 bytes
 *
 * If 'k' means 1000 bytes (not 1024), then 9728k = 9728*1000 = 9500*1024,
 * exactly halfway from 9000 to 10000, which explains why it jumps to next mbyte
 * at this value.
 *
 * 'm' does not seem to behave this way: it means 1024*1024 bytes.
 *
 * Not sure we want to copy this. If user says he wants 1234kbyte partition,
 * we do _exactly that_: 1234kbytes = 2468 sectors.
 */
			case 'm':
				scale_shift = 20; /* 1024*1024 */
				break;
			case 'g':
				scale_shift = 30; /* 1024*1024*1024 */
				break;
			case 't':
				scale_shift = 40; /* 1024*1024*1024*1024 */
				break;
			default:
				break;
			}
			if (scale_shift) {
				ullong bytes;
				unsigned long unit;

				bytes = (ullong) value << scale_shift;
				unit = sector_size * units_per_sector;
				bytes += unit/2; /* round */
				bytes /= unit;
				value = (bytes != 0 ? bytes - 1 : 0);
			}
			if (minus)
				value = -value;
			value += base;
		} else {
			if (sizeof(value) <= sizeof(long))
				value = strtoul(line_ptr, NULL, 10);
			else
				value = strtoull(line_ptr, NULL, 10);
			while (isdigit(*line_ptr)) {
				line_ptr++;
				use_default = 0;
			}
		}
		if (use_default) {
			value = dflt;
			printf("Using default value %u\n", value);
		}
		if (value >= low && value <= high)
			break;
		puts("Value is out of range");
	}
	return value;
}

static unsigned
input_partition_number(int warn, unsigned max)
{
	struct pte *pe;
	unsigned i;

	// low, default(none), high, base(?), msg
	i = read_int(1, 0, max, 0, "Partition number") - 1;
	pe = &ptes[i];

	if (warn) {
		if (pe->part_table->sys_ind == 0) {
			printf("Warning: partition %u has type 0\n", i + 1);
		}
	}
	return i;
}

static int
get_existing_partition(int warn, unsigned max)
{
	int pno = -1;
	unsigned i;

	for (i = 0; i < max; i++) {
		struct pte *pe = &ptes[i];
		struct dos_partition *p = pe->part_table;

		if (p && !is_cleared_partition(p)) {
			if (pno >= 0)
				goto not_unique;
			pno = i;
		}
	}
	if (pno >= 0) {
		printf("Selected partition %u\n", pno + 1);
		return pno;
	}
	puts("No partition is defined yet!");
	return pno; // -1

 not_unique:
	return input_partition_number(warn, max);
}

static int
choose_free_primary_partition(void)
{
	int pno = -1;
	unsigned i;

	for (i = 0; i < 4; i++) {
		struct pte *pe = &ptes[i];
		struct dos_partition *p = pe->part_table;

		if (p && is_cleared_partition(p)) {
			if (pno >= 0)
				goto not_unique;
			pno = i;
		}
	}
// Caller ensures cleared partition exists
//	if (pno >= 0) {
		printf("Selected partition %u\n", pno + 1);
		return pno;
//	}
//	puts("All primary partitions have been defined already!");
//	return pno; // -1

 not_unique:
	return input_partition_number(/*warn*/ 0, 4);
}


static void
change_units(void)
{
	TOGGLE_DISPLAY_IN_CYL_UNITS;
	update_units();
	printf("Changing display/entry units to %ss\n",
		str_units());
}

static void
toggle_active(int i)
{
	struct pte *pe = &ptes[i];
	struct dos_partition *p = pe->part_table;

	if (IS_EXTENDED(p->sys_ind) && !p->boot_ind)
		printf("WARNING: Partition %u is an extended partition\n", i + 1);
	p->boot_ind = (p->boot_ind ? 0 : ACTIVE_FLAG);
	pe->changed = 1;
}

static void
toggle_dos_compatibility_flag(void)
{
	dos_compatible_flag = 1 - dos_compatible_flag;
	offset_after_MBR_and_ext = dos_compatible_flag ? g_sectors : 1;
	printf("DOS Compatibility flag is %sset (sector gap:%d)\n", "", offset_after_MBR_and_ext);
}

static void
delete_partition(int i)
{
	struct pte *pe = &ptes[i];
	struct dos_partition *p = pe->part_table;
	struct dos_partition *q = pe->ext_pointer;

/* Note that for the fifth partition (i == 4) we don't actually
 * decrement partitions.
 */

	if (warn_geometry())
		return;         /* C/H/S not set */
	pe->changed = 1;

	if (i < 4) {
		if (IS_EXTENDED(p->sys_ind) && i == ext_index) {
			g_partitions = 4;
			ptes[ext_index].ext_pointer = NULL;
			extended_offset = 0;
		}
		clear_partition(p);
		return;
	}

	if (is_cleared_partition(q) && i > 4) {
		/* the last one in the chain - just delete */
		--g_partitions;
		--i;
		clear_partition(ptes[i].ext_pointer);
		ptes[i].changed = 1;
	} else {
		/* not the last one - further ones will be moved down */
		if (i > 4) {
			/* delete this link in the chain */
			p = ptes[i-1].ext_pointer;
			*p = *q;
			set_start_sect(p, get_start_sect(q));
			set_nr_sects(p, get_nr_sects(q));
			ptes[i-1].changed = 1;
		} else if (g_partitions > 5) {    /* 5 will be moved to 4 */
			/* the first logical in a longer chain */
			pe = &ptes[5];

			if (pe->part_table) /* prevent SEGFAULT */
				set_start_sect(pe->part_table,
						get_partition_start_from_dev_start(pe) -
						extended_offset);
			pe->offset_from_dev_start = extended_offset;
			pe->changed = 1;
		}

		if (g_partitions > 5) {
			g_partitions--;
			while (i < g_partitions) {
				ptes[i] = ptes[i+1];
				i++;
			}
		} else {
			/* the only logical: clear only */
			clear_partition(ptes[i].part_table);
		}
	}
}

static void
change_sysid(void)
{
	int i, sys, origsys;
	struct dos_partition *p;

	i = get_existing_partition(0, g_partitions);
	if (i == -1)
		return;
	p = ptes[i].part_table;
	origsys = sys = get_sysid(i);

	/* if changing types T to 0 is allowed, then
	   the reverse change must be allowed, too */
	if (sys == 0 && get_nr_sects(p) == 0) {
		printf("Partition %u does not exist yet\n", i + 1);
		return;
	}
	while (1) {
		sys = read_hex(get_sys_types());

		if (sys == 0) {
			puts("Type 0 means free space to many systems\n"
				"(but not to Linux). Having partitions of\n"
				"type 0 is unwise.");
			/* break; */
		}

		if (IS_EXTENDED(sys) != IS_EXTENDED(p->sys_ind)) {
			puts("You cannot change a partition into"
				" an extended one or vice versa");
			break;
		}

		if (sys < 256) {
			if (sys == origsys)
				break;
			p->sys_ind = sys;

			printf("Changed system type of partition %u "
				"to %x (%s)\n", i + 1, sys,
				partition_type(sys));
			ptes[i].changed = 1;
			break;
		}
	}
}
#endif /* FEATURE_FDISK_WRITABLE */


/* check_consistency() and linear2chs() added Sat Mar 6 12:28:16 1993,
 * faith@cs.unc.edu, based on code fragments from pfdisk by Gordon W. Ross,
 * Jan.  1990 (version 1.2.1 by Gordon W. Ross Aug. 1990; Modified by S.
 * Lubkin Oct.  1991). */

static void
linear2chs(unsigned ls, unsigned *c, unsigned *h, unsigned *s)
{
	int spc = g_heads * g_sectors;

	*c = ls / spc;
	ls = ls % spc;
	*h = ls / g_sectors;
	*s = ls % g_sectors + 1;  /* sectors count from 1 */
}

static void
check_consistency(const struct dos_partition *p, int partition)
{
	unsigned pbc, pbh, pbs;          /* physical beginning c, h, s */
	unsigned pec, peh, pes;          /* physical ending c, h, s */
	unsigned lbc, lbh, lbs;          /* logical beginning c, h, s */
	unsigned lec, leh, les;          /* logical ending c, h, s */

	if (!g_heads || !g_sectors || (partition >= 4))
		return;         /* do not check extended partitions */

/* physical beginning c, h, s */
	pbc = cylinder(p->sector, p->cyl);
	pbh = p->head;
	pbs = sector(p->sector);

/* physical ending c, h, s */
	pec = cylinder(p->end_sector, p->end_cyl);
	peh = p->end_head;
	pes = sector(p->end_sector);

/* compute logical beginning (c, h, s) */
	linear2chs(get_start_sect(p), &lbc, &lbh, &lbs);

/* compute logical ending (c, h, s) */
	linear2chs(get_start_sect(p) + get_nr_sects(p) - 1, &lec, &leh, &les);

/* Same physical / logical beginning? */
	if (g_cylinders <= 1024 && (pbc != lbc || pbh != lbh || pbs != lbs)) {
		printf("Partition %u has different physical/logical "
			"start (non-Linux?):\n", partition + 1);
		printf("     phys=(%u,%u,%u) ", pbc, pbh, pbs);
		printf("logical=(%u,%u,%u)\n", lbc, lbh, lbs);
	}

/* Same physical / logical ending? */
	if (g_cylinders <= 1024 && (pec != lec || peh != leh || pes != les)) {
		printf("Partition %u has different physical/logical "
			"end:\n", partition + 1);
		printf("     phys=(%u,%u,%u) ", pec, peh, pes);
		printf("logical=(%u,%u,%u)\n", lec, leh, les);
	}
}

/*
 * Check whether partition entries are ordered by their starting positions.
 * Return 0 if OK. Return i if partition i should have been earlier.
 * Two separate checks: primary and logical partitions.
 */
static int
wrong_p_order(int *prev)
{
	const struct pte *pe;
	const struct dos_partition *p;
	sector_t last_p_start_pos = 0, p_start_pos;
	unsigned i, last_i = 0;

	for (i = 0; i < g_partitions; i++) {
		if (i == 4) {
			last_i = 4;
			last_p_start_pos = 0;
		}
		pe = &ptes[i];
		p = pe->part_table;
		if (!is_cleared_partition(p)) {
			p_start_pos = get_partition_start_from_dev_start(pe);
			if (last_p_start_pos > p_start_pos) {
				if (prev)
					*prev = last_i;
				return i;
			}
			last_p_start_pos = p_start_pos;
			last_i = i;
		}
	}
	return 0;
}

#if ENABLE_FEATURE_FDISK_ADVANCED
/*
 * Fix the chain of logicals.
 * extended_offset is unchanged, the set of sectors used is unchanged
 * The chain is sorted so that sectors increase, and so that
 * starting sectors increase.
 *
 * After this it may still be that cfdisk doesnt like the table.
 * (This is because cfdisk considers expanded parts, from link to
 * end of partition, and these may still overlap.)
 * Now
 *   sfdisk /dev/hda > ohda; sfdisk /dev/hda < ohda
 * may help.
 */
static void
fix_chain_of_logicals(void)
{
	int j, oj, ojj, sj, sjj;
	struct dos_partition *pj,*pjj,tmp;

	/* Stage 1: sort sectors but leave sector of part 4 */
	/* (Its sector is the global extended_offset.) */
 stage1:
	for (j = 5; j < g_partitions - 1; j++) {
		oj = ptes[j].offset_from_dev_start;
		ojj = ptes[j+1].offset_from_dev_start;
		if (oj > ojj) {
			ptes[j].offset_from_dev_start = ojj;
			ptes[j+1].offset_from_dev_start = oj;
			pj = ptes[j].part_table;
			set_start_sect(pj, get_start_sect(pj)+oj-ojj);
			pjj = ptes[j+1].part_table;
			set_start_sect(pjj, get_start_sect(pjj)+ojj-oj);
			set_start_sect(ptes[j-1].ext_pointer,
					   ojj-extended_offset);
			set_start_sect(ptes[j].ext_pointer,
					   oj-extended_offset);
			goto stage1;
		}
	}

	/* Stage 2: sort starting sectors */
 stage2:
	for (j = 4; j < g_partitions - 1; j++) {
		pj = ptes[j].part_table;
		pjj = ptes[j+1].part_table;
		sj = get_start_sect(pj);
		sjj = get_start_sect(pjj);
		oj = ptes[j].offset_from_dev_start;
		ojj = ptes[j+1].offset_from_dev_start;
		if (oj+sj > ojj+sjj) {
			tmp = *pj;
			*pj = *pjj;
			*pjj = tmp;
			set_start_sect(pj, ojj+sjj-oj);
			set_start_sect(pjj, oj+sj-ojj);
			goto stage2;
		}
	}

	/* Probably something was changed */
	for (j = 4; j < g_partitions; j++)
		ptes[j].changed = 1;
}

static void
fix_partition_table_order(void)
{
	struct pte *pei, *pek;
	int i,k;

	if (!wrong_p_order(NULL)) {
		puts("Ordering is already correct\n");
		return;
	}

	while ((i = wrong_p_order(&k)) != 0 && i < 4) {
		/* partition i should have come earlier, move it */
		/* We have to move data in the MBR */
		struct dos_partition *pi, *pk, *pe, pbuf;
		pei = &ptes[i];
		pek = &ptes[k];

		pe = pei->ext_pointer;
		pei->ext_pointer = pek->ext_pointer;
		pek->ext_pointer = pe;

		pi = pei->part_table;
		pk = pek->part_table;

		memmove(&pbuf, pi, sizeof(struct dos_partition));
		memmove(pi, pk, sizeof(struct dos_partition));
		memmove(pk, &pbuf, sizeof(struct dos_partition));

		pei->changed = pek->changed = 1;
	}

	if (i)
		fix_chain_of_logicals();

	puts("Done");
}
#endif // ADVANCED

static const char *
chs_string11(unsigned cyl, unsigned head, unsigned sect)
{
	char *buf = auto_string(xzalloc(sizeof(int)*3 * 3));
	sprintf(buf, "%u,%u,%u", cylinder(sect,cyl), head, sector(sect));
	return buf;
}

static void dos_print_disklabel(void)
{
	int i, w;

	// Heuristic: we list partition 3 of /dev/foo as /dev/foo3,
	// but if the device name ends in a digit, say /dev/foo1,
	// then the partition is called /dev/foo1p3.
	w = strlen(disk_device);
	if (w && isdigit(disk_device[w-1]))
		w++;
	if (w < 7)
		w = 7;

	printf("%-*s Boot StartCHS    EndCHS        StartLBA     EndLBA    Sectors  Size Id Type\n",
		   w-1, "Device");

	for (i = 0; i < g_partitions; i++) {
		const struct dos_partition *p;
		const struct pte *pe = &ptes[i];
		char boot4[4];
		char numstr6[6];
		sector_t start_sect;
		sector_t end_sect;
		sector_t nr_sects;

		p = pe->part_table;
		if (!p || is_cleared_partition(p))
			continue;

		sprintf(boot4, "%02x", p->boot_ind);
		if ((p->boot_ind & 0x7f) == 0) {
			/* 0x80 shown as '*', 0x00 is ' ' */
			boot4[0] = p->boot_ind ? '*' : ' ';
			boot4[1] = ' ';
		}

		start_sect = get_partition_start_from_dev_start(pe);
		end_sect = start_sect;
		nr_sects = get_nr_sects(p);
		if (nr_sects != 0)
			end_sect += nr_sects - 1;

		smart_ulltoa5((ullong)nr_sects * sector_size,
			numstr6, " KMGTPEZY")[0] = '\0';

#define SFMT SECT_FMT
		//      Boot StartCHS    EndCHS        StartLBA     EndLBA    Sectors  Size Id Type
		printf("%s%s %-11s"/**/" %-11s"/**/" %10"SFMT"u %10"SFMT"u %10"SFMT"u" " %s %2x %s\n",
			partname(disk_device, i+1, w+2),
			boot4,
			chs_string11(p->cyl, p->head, p->sector),
			chs_string11(p->end_cyl, p->end_head, p->end_sector),
			start_sect,
			end_sect,
			nr_sects,
			numstr6,
			p->sys_ind,
			partition_type(p->sys_ind)
		);
#undef SFMT
		check_consistency(p, i);
	}

	/* Is partition table in disk order? It need not be, but... */
	/* partition table entries are not checked for correct order
	 * if this is an AIX labeled disk... */
	if (LABEL_IS_DOS && wrong_p_order(NULL)) {
		/* FIXME */
		puts("\nPartition table entries are not in disk order");
	}
}

#if ENABLE_FEATURE_FDISK_ADVANCED
static void
x_dos_print_disklabel(int extend)
{
	const struct pte *pe;
	const struct dos_partition *p;
	int i;

	printf("\nDisk %s: %u heads, %u sectors, %u cylinders\n\n",
		disk_device, g_heads, g_sectors, g_cylinders);
	puts("Nr AF  Hd Sec  Cyl  Hd Sec  Cyl      Start       Size ID");
	for (i = 0; i < g_partitions; i++) {
		pe = &ptes[i];
		p = (extend ? pe->ext_pointer : pe->part_table);
		if (p != NULL) {
			printf("%2u %02x%4u%4u%5u%4u%4u%5u%11"SECT_FMT"u%11"SECT_FMT"u %02x\n",
				i + 1, p->boot_ind,
				p->head,
				sector(p->sector),
				cylinder(p->sector, p->cyl),
				p->end_head,
				sector(p->end_sector),
				cylinder(p->end_sector, p->end_cyl),
				get_start_sect(p),
				get_nr_sects(p),
				p->sys_ind
			);
			if (!is_cleared_partition(p))
				check_consistency(p, i);
		}
	}
}
#endif

static void
print_disk_name_and_sizes(void)
{
	char numstr6[6];
	ullong total_bytes;

	total_bytes = (ullong)total_number_of_sectors * sector_size;
	smart_ulltoa5(total_bytes, numstr6, " KMGTPEZY")[0] = '\0';

	printf("Disk %s: %s, %llu bytes, %"SECT_FMT"u sectors\n",
		disk_device,
		skip_whitespace(numstr6),
		total_bytes,
		(SECT_TYPE)total_number_of_sectors
	);
	// util-linux 2.41.1 also says after the above:
	//Disk model: (string from /sys/dev/block/MAJ:MIN/device/model)
	//Units: sectors of 1 * 512 = 512 bytes
	//Sector size (logical/physical): 512 bytes / 512 bytes
	//I/O size (minimum/optimal): 512 bytes / 512 bytes
	//Disklabel type: gpt
	//Disk identifier: FEEDC0DE-BEEF-F00D-FEDC-F0CACC1A1234
	// or
	//Disklabel type: dos
	//Disk identifier: 0xdeafbead
	//
	// On a all-zeros image file, these lines reduce to:
	//Units: sectors of 1 * 512 = 512 bytes
	//Sector size (logical/physical): 512 bytes / 512 bytes
	//I/O size (minimum/optimal): 512 bytes / 512 bytes
	// (yes, with -l, not even "doesn't contain a valid partition table" shown!)
}

static void
print_disk_geometry(void)
{
	printf(
		"%u cylinders, %u heads, %u sectors/track\n"
		"Units: %ss of %u * %u = %u bytes\n"
		"\n",
		g_cylinders, g_heads, g_sectors,
		str_units(),
		units_per_sector, sector_size, units_per_sector * sector_size
	);
}

static void
print_disklabel(int xtra)
{
	// Print common header for all formats
	print_disk_name_and_sizes();

	if (LABEL_IS_GPT) {
		gpt_print_disklabel();
		return;
	}
	// GPT does not talk about cylinders/heads/sectors, other formats do
	print_disk_geometry();

	if (LABEL_IS_OSF) {
		xbsd_print_disklabel(xtra);
		return;
	}

	// So far AIX also goes here - FIXME?
	dos_print_disklabel();
}

static int
is_ide_cdrom_or_tape(const char *device)
{
	FILE *procf;
	char buf[100];
	struct stat statbuf;
	int is_ide = 0;

	/* No device was given explicitly, and we are trying some
	   likely things.  But opening /dev/hdc may produce errors like
	   "hdc: tray open or drive not ready"
	   if it happens to be a CD-ROM drive. It even happens that
	   the process hangs on the attempt to read a music CD.
	   So try to be careful. This only works since 2.1.73. */

	if (!is_prefixed_with(device, "/dev/hd"))
		return 0;

	snprintf(buf, sizeof(buf), "/proc/ide/%s/media", device+5);
	procf = fopen_for_read(buf);
	if (procf != NULL && fgets(buf, sizeof(buf), procf))
		is_ide = (is_prefixed_with(buf, "cdrom") ||
			  is_prefixed_with(buf, "tape"));
	else
		/* Now when this proc file does not exist, skip the
		   device when it is read-only. */
		if (stat(device, &statbuf) == 0)
			is_ide = ((statbuf.st_mode & 0222) == 0);

	if (procf)
		fclose(procf);
	return is_ide;
}

static void
open_list_and_close(const char *device, int user_specified)
{
	int gb;

	disk_device = device;
	if (setjmp(listingbuf))
		return;
	if (!user_specified)
		if (is_ide_cdrom_or_tape(device))
			return;

	// Open disk_device, save file descriptor to dev_fd
	errno = 0;
	gb = get_boot(TRY_ONLY);
	if (gb > 0) {  // I/O error
		// Ignore other errors, since we try IDE
		// and SCSI hard disks which may not be
		// installed on the system.
		if (user_specified || errno == EACCES)
			bb_perror_msg("can't open '%s'", device);
		return;
	}

	if (gb < 0) { // no DOS/AIX signature
		print_disk_name_and_sizes();
		print_disk_geometry();
#if ENABLE_FEATURE_OSF_LABEL
		if (bsd_trydev(device) < 0)
#endif
			printf("Disk %s doesn't contain a valid "
				"partition table\n", device);
	} else {
		print_disklabel(0);
	}
	close_dev_fd();
}

/* Is it a whole disk? The digit check is still useful
   for Xen devices for example. */
static int is_whole_disk(const char *disk)
{
	unsigned len;
	int fd = open(disk, O_RDONLY);

	if (fd != -1) {
		struct hd_geometry geometry;
		int err = ioctl(fd, HDIO_GETGEO, &geometry);
		close(fd);
		if (!err)
			return (geometry.start == 0);
	}

	/* Treat "nameN" as a partition name, not whole disk */
	/* note: mmcblk0 should work from the geometry check above */
	len = strlen(disk);
	if (len != 0 && isdigit(disk[len - 1]))
		return 0;

	return 1;
}

/* for fdisk -l: try all things in /proc/partitions
   that look like a partition name (do not end in a digit) */
static void
list_devs_in_proc_partititons(void)
{
	FILE *procpt;
	char line[100], ptname[100], devname[120];
	int ma, mi, sz;

	procpt = fopen_or_warn("/proc/partitions", "r");

	while (fgets(line, sizeof(line), procpt)) {
		if (sscanf(line, " %u %u %u %[^\n ]",
				&ma, &mi, &sz, ptname) != 4)
			continue;

		sprintf(devname, "/dev/%s", ptname);
		if (is_whole_disk(devname))
			open_list_and_close(devname, 0);
	}
	if (ENABLE_FEATURE_CLEAN_UP)
		fclose(procpt);
}

#if ENABLE_FEATURE_FDISK_WRITABLE
static void
fill_bounds(sector_t *first, sector_t *last)
{
	unsigned i;
	const struct pte *pe = &ptes[0];
	const struct dos_partition *p;

	for (i = 0; i < g_partitions; pe++,i++) {
		p = pe->part_table;
		if (is_cleared_partition(p) || IS_EXTENDED(p->sys_ind)) {
			first[i] = 0xffffffff;
			last[i] = 0;
		} else {
			first[i] = get_partition_start_from_dev_start(pe);
			last[i] = first[i] + get_nr_sects(p) - 1;
		}
	}
}

static void
check(int n, unsigned h, unsigned s, unsigned c, sector_t start)
{
	sector_t total, real_s, real_c;

	real_s = sector(s) - 1;
	real_c = cylinder(s, c);
	total = (real_c * g_sectors + real_s) * g_heads + h;
	if (!total)
		printf("Partition %u contains sector 0\n", n);
	if (h >= g_heads)
		printf("Partition %u: head %u greater than maximum %u\n",
			n, h + 1, g_heads);
	if (real_s >= g_sectors)
		printf("Partition %u: sector %u greater than "
			"maximum %u\n", n, s, g_sectors);
	if (real_c >= g_cylinders)
		printf("Partition %u: cylinder %"SECT_FMT"u greater than "
			"maximum %u\n", n, real_c + 1, g_cylinders);
	if (g_cylinders <= 1024 && start != total)
		printf("Partition %u: previous sectors %"SECT_FMT"u disagrees with "
			"total %"SECT_FMT"u\n", n, start, total);
}

static void
verify(void)
{
	int i, j;
	sector_t total = 1;
	sector_t chs_size;
	sector_t first[g_partitions], last[g_partitions];
	struct dos_partition *p;

	if (warn_geometry())
		return;

	fill_bounds(first, last);
	for (i = 0; i < g_partitions; i++) {
		struct pte *pe = &ptes[i];

		p = pe->part_table;
//TODO: warn about EXTENDED partitions with nr_sect==0? Kernel will ignore these (requires at least 1)
		if (!is_cleared_partition(p) && !IS_EXTENDED(p->sys_ind)) {
			check_consistency(p, i);
			if (get_partition_start_from_dev_start(pe) < first[i])
				printf("Warning: bad start-of-data in "
					"partition %u\n", i + 1);
			check(i + 1, p->end_head, p->end_sector, p->end_cyl,
				last[i]);
			total += last[i] + 1 - first[i];
			for (j = 0; j < i; j++) {
				if ((first[i] >= first[j] && first[i] <= last[j])
				 || ((last[i] <= last[j] && last[i] >= first[j]))) {
					printf("Warning: partition %u overlaps "
						"partition %u\n", j + 1, i + 1);
					total += first[i] >= first[j] ?
						first[i] : first[j];
					total -= last[i] <= last[j] ?
						last[i] : last[j];
				}
			}
		}
	}

	if (extended_offset) {
		struct pte *pex = &ptes[ext_index];
		sector_t e_last = get_start_sect(pex->part_table) +
			get_nr_sects(pex->part_table) - 1;

		for (i = 4; i < g_partitions; i++) {
			total++;
			p = ptes[i].part_table;
			if (is_cleared_partition(p)) {
				if (i != 4 || i + 1 < g_partitions)
					printf("Warning: partition %u "
						"is empty\n", i + 1);
			} else if (first[i] < extended_offset || last[i] > e_last) {
				printf("Logical partition %u not entirely in "
					"partition %u\n", i + 1, ext_index + 1);
			}
		}
	}

	chs_size = (sector_t)g_heads * g_sectors * g_cylinders;
	if (total > chs_size)
		printf("Total allocated sectors %u"
			" greater than CHS size %"SECT_FMT"u\n",
			total, chs_size
		);
	else {
		total = chs_size - total;
		if (total != 0)
			printf("%"SECT_FMT"u unallocated sectors\n", total);
	}
}

static void
set_hsc_start_end(struct dos_partition *p, sector_t start, sector_t stop)
{
#define SET_HEAD_SECT_CYL(h, s, c, sector) do { \
		s = sector % g_sectors + 1;  \
		sector /= g_sectors;         \
		h = sector % g_heads;        \
		sector /= g_heads;           \
		c = sector & 0xff;           \
		s |= (sector >> 2) & 0xc0;   \
	} while (0)

	if (dos_compatible_flag && (start / (g_sectors * g_heads) > 1023))
		start = g_heads * g_sectors * 1024 - 1;
	SET_HEAD_SECT_CYL(p->head, p->sector, p->cyl, start);
	if (dos_compatible_flag && (stop / (g_sectors * g_heads) > 1023))
		stop = g_heads * g_sectors * 1024 - 1;
	SET_HEAD_SECT_CYL(p->end_head, p->end_sector, p->end_cyl, stop);
#undef SET_HEAD_SECT_CYL
}

// add_partition() helper
static void
set_partition(int i, int doext, sector_t start, sector_t stop, int sysid)
{
	struct dos_partition *p;
	sector_t offset;

	if (doext) {
		p = ptes[i].ext_pointer;
		offset = extended_offset;
	} else {
		p = ptes[i].part_table;
		offset = ptes[i].offset_from_dev_start;
	}
	p->boot_ind = 0;
	p->sys_ind = sysid;
	set_start_sect(p, start - offset);
	set_nr_sects(p, stop - start + 1);
	set_hsc_start_end(p, start, stop);
	ptes[i].changed = 1;
}
static void
add_partition(int n, int sys)
{
	char mesg[64];
	int i, num_read;
	struct dos_partition *p;
	struct dos_partition *main_ext;
	sector_t limit, temp;
	sector_t start, stop;
	sector_t first[g_partitions], last[g_partitions];

	p = ptes[n].part_table;
	if (p && !is_cleared_partition(p)) {
		printf("Partition %u is already defined, delete it before re-adding\n", n + 1);
		return;
	}
	fill_bounds(first, last);
	main_ext = ptes[ext_index].part_table;
	if (n < 4) {
		start = offset_after_MBR_and_ext;
		if (DISPLAY_IN_CYL_UNITS || !total_number_of_sectors) {
			limit = (sector_t) g_heads * g_sectors * g_cylinders - 1;
			dbg("%d: limit:%d", __LINE__, limit);
		} else {
			limit = total_number_of_sectors - 1;
			dbg("%d: limit:%d", __LINE__, limit);
		}
		if (extended_offset) {
			first[ext_index] = extended_offset;
			last[ext_index] = extended_offset + get_nr_sects(main_ext) - 1;
		}
		dbg("%d: start:%d limit:%d", __LINE__, start, limit);
	} else {
		start = extended_offset + offset_after_MBR_and_ext;
		limit = extended_offset + get_nr_sects(main_ext) - 1;
		dbg("%d: start:%d limit:%d", __LINE__, start, limit);
	}
	if (DISPLAY_IN_CYL_UNITS)
		for (i = 0; i < g_partitions; i++)
			first[i] = (cround(first[i]) - 1) * units_per_sector;

	sprintf(mesg, "First %s", str_units());
	num_read = 0;
	do {
		temp = start;
		for (i = 0; i < g_partitions; i++) {
			sector_t lastplusoff;

			if (start == ptes[i].offset_from_dev_start) {
				start += offset_after_MBR_and_ext;
				dbg("%d: start:%d", __LINE__, start);
			}
			lastplusoff = last[i] + ((n < 4) ? 0 : offset_after_MBR_and_ext);
			if (start >= first[i] && start <= lastplusoff) {
				start = lastplusoff + 1;
				dbg("%d: start:%d", __LINE__, start);
			}
		}
		if (start > limit)
			break;
		if (start >= temp + units_per_sector && num_read) {
			printf("Sector %"SECT_FMT"u is already allocated\n", temp);
			temp = start;
			num_read = 0;
		}
		if (!num_read && start == temp) {
			sector_t saved_start;

			saved_start = start;
			start = read_int(cround(saved_start), cround(saved_start), cround(limit), 0, mesg);
			if (DISPLAY_IN_CYL_UNITS) {
				start = (start - 1) * units_per_sector;
				dbg("%d: start:%d", __LINE__, start);
				if (start < saved_start) {
					start = saved_start;
					dbg("%d: start:%d", __LINE__, start);
				}
			}
			num_read = 1;
		}
	} while (start != temp || !num_read);

	if (n > 4) {                    /* NOT for fifth partition */
		struct pte *pe = &ptes[n];

		pe->offset_from_dev_start = start - offset_after_MBR_and_ext;
		if (pe->offset_from_dev_start == extended_offset) { /* must be corrected */
			pe->offset_from_dev_start++;
			if (offset_after_MBR_and_ext == 1) {
				start++;
				dbg("%d: ++start:%d", __LINE__, start);
			}
		}
	}

	for (i = 0; i < g_partitions; i++) {
		struct pte *pe = &ptes[i];

		if (start < pe->offset_from_dev_start && limit >= pe->offset_from_dev_start) {
			limit = pe->offset_from_dev_start - 1;
			dbg("%d: limit:%d", __LINE__, limit);
		}
		if (start < first[i] && limit >= first[i]) {
			limit = first[i] - 1;
			dbg("%d: limit:%d", __LINE__, limit);
		}
	}
	if (start > limit) {
		puts("No free sectors available");
		if (n > 4)
			g_partitions--;
		return;
	}
	if (cround(start) == cround(limit)) {
		stop = limit;
	} else {
		sprintf(mesg,
			 "Last %s or +size{,K,M,G,T}",
			 str_units()
		);
		stop = read_int(cround(start), cround(limit), cround(limit), cround(start), mesg);
		if (DISPLAY_IN_CYL_UNITS) {
			stop = stop * units_per_sector - 1;
			if (stop > limit)
				stop = limit;
		}
	}

	set_partition(n, /* ext:*/ 0, start, stop, sys);
	if (n > 4)
		set_partition(n - 1, /* ext:*/ 1, ptes[n].offset_from_dev_start, stop, EXTENDED);

	if (IS_EXTENDED(sys)) {
		struct pte *pe4 = &ptes[4];
		struct pte *pen = &ptes[n];

		ext_index = n;
		pen->ext_pointer = p;
		pe4->offset_from_dev_start = extended_offset = start;
		pe4->sectorbuffer = xzalloc(sector_size);
		pe4->part_table = pt_offset(pe4->sectorbuffer, 0);
		pe4->ext_pointer = pe4->part_table + 1;
		pe4->changed = 1;
		g_partitions = 5;
	}
}

static void
add_logical(void)
{
	if (g_partitions > 5 || !is_cleared_partition(ptes[4].part_table)) {
		struct pte *pe = &ptes[g_partitions];

		pe->sectorbuffer = xzalloc(sector_size);
		pe->part_table = pt_offset(pe->sectorbuffer, 0);
		pe->ext_pointer = pe->part_table + 1;
		pe->offset_from_dev_start = 0;
		pe->changed = 1;
		g_partitions++;
	}
	add_partition(g_partitions - 1, LINUX_NATIVE);
}

static void
new_partition(void)
{
	char c, line[80];
	int free_primary;

	if (warn_geometry())
		return;

	if (LABEL_IS_AIX) {
		puts("Sorry - this fdisk cannot handle AIX disk labels.\n"
"If you want to add DOS-type partitions, create a new empty DOS partition\n"
"table first (use 'o'). This will destroy the present disk contents.");
		return;
	}

	for (free_primary = 0; free_primary < 4; free_primary++)
		if (is_cleared_partition(ptes[free_primary].part_table))
			break;

	if (free_primary == 4) {
		// no free primary partition
		if (g_partitions >= MAXIMUM_PARTS) {
			puts("The maximum number of partitions has been created");
			return;
		}
		if (extended_offset)
			add_logical();
		else
			puts("You must delete some partition and add "
				 "an extended partition first");
		return;
	}

	snprintf(line, sizeof(line),
		"Partition type\n"
		"   p   primary (1-4)\n"
		"   %s\n",
		(extended_offset ?
		"l   logical (5+)" : "e   extended (1-4)"));
	c = read_nonempty(line);
	c |= 0x20; /* lowercase */
	if (c == 'p'
	 || (c == 'e' && !extended_offset)
	) {
		free_primary = choose_free_primary_partition();
		//if (free_primary >= 0) // cannot fail, we know it exists
		add_partition(free_primary, c == 'p' ? LINUX_NATIVE : EXTENDED);
		return;
	}
	if (c == 'l' && extended_offset) {
		add_logical();
		return;
	}
	// Bad answer: go back to main menu
}

static void
reread_partition_table(int leave)
{
	int i;

	puts("Calling ioctl() to re-read partition table");
	sync();
	/* Users with slow external USB disks on a 320MHz ARM system (year 2011)
	 * report that sleep is needed, otherwise BLKRRPART may fail with -EIO:
	 */
	sleep1();
	i = ioctl_or_perror(dev_fd, BLKRRPART, NULL,
			"WARNING: rereading partition table "
			"failed, kernel still uses old table");
	if (leave) {
		if (ENABLE_FEATURE_CLEAN_UP)
			close_dev_fd();
		exit(i != 0);
	}
}

static void NORETURN
write_table_and_exit(void)
{
	int i;

	if (LABEL_IS_DOS) {
		for (i = 0; i < 3; i++)
			if (ptes[i].changed)
				ptes[3].changed = 1;
		// If needed, write MBR and extended partition sectors
		for (i = 3; i < g_partitions; i++) {
			struct pte *pe = &ptes[i];
			if (pe->changed) {
				write_55AA_signature(pe->sectorbuffer);
				write_sector(pe->offset_from_dev_start, pe->sectorbuffer);
			}
		}
	}

	puts("The partition table has been altered.");
	reread_partition_table(1); // 1: exit
	bb_unreachable(abort());
}

# if ENABLE_FEATURE_FDISK_ADVANCED
#define MAX_PER_LINE    16
static void
print_buffer(uint8_t *pbuffer)
{
	int i, l;

	for (i = 0, l = 0; i < sector_size; i++, l++) {
		if (l == 0)
			printf("0x%03X:", i);
		printf(" %02X", pbuffer[i]);
		if (l == MAX_PER_LINE - 1) {
			bb_putchar('\n');
			l = -1;
		}
	}
	if (l > 0)
		bb_putchar('\n');
	bb_putchar('\n');
}

static void
print_raw(void)
{
	int i;

	printf("Device: %s\n", disk_device);
	for (i = 3; i < g_partitions; i++)
		print_buffer(ptes[i].sectorbuffer);
}

static void
move_begin(unsigned i)
{
	struct pte *pe = &ptes[i];
	struct dos_partition *p = pe->part_table;
	sector_t new, first, nr_sects;

	if (warn_geometry())
		return;
	nr_sects = get_nr_sects(p);
	if (nr_sects == 0 || IS_EXTENDED(p->sys_ind)) {
		printf("Partition %u has no data area\n", i + 1);
		return;
	}
	first = get_partition_start_from_dev_start(pe); // pe->offset_from_dev_start + get_start_sect(p)
	new = read_int(0 /*was:first*/, first, first + nr_sects - 1, first, "New beginning of data");
	if (new != first) {
		sector_t new_relative = new - pe->offset_from_dev_start;
		nr_sects += (get_start_sect(p) - new_relative);
		set_start_sect(p, new_relative);
		set_nr_sects(p, nr_sects);
		read_nonempty("Recalculate C/H/S values? (Y/N): ");
		if ((line_ptr[0] | 0x20) == 'y')
			set_hsc_start_end(p, new, new + nr_sects - 1);
		pe->changed = 1;
	}
}
# endif /* ADVANCED mode */

static void
menu(void)
{
	puts("Command Action");
	if (LABEL_IS_AIX) {
		puts("o\tcreate a new empty DOS partition table");
		puts("q\tquit without saving changes");
		//deleted: puts("s\tcreate a new empty Sun disklabel");  /* sun */
	} else if (LABEL_IS_GPT) {
		puts("o\tcreate a new empty DOS partition table");
		puts("p\tprint partition table");
		puts("q\tquit without saving changes");
		//deleted: puts("s\tcreate a new empty Sun disklabel");  /* sun */
	} else {
		puts("a\ttoggle a bootable flag");
		puts("b\tedit bsd disklabel");
		printf("c\tturn o%s DOS sector gap flag\n", dos_compatible_flag ? "ff" : "n");
		puts("d\tdelete a partition");
		puts("l\tlist known partition types");
		puts("n\tadd a new partition");
		puts("o\tcreate a new empty DOS partition table");
		puts("p\tprint partition table");
		puts("q\tquit without saving changes");
		//deleted: puts("s\tcreate a new empty Sun disklabel");  /* sun */
		puts("t\tchange a partition's system id");
		puts("u\tchange display/entry units");
		puts("v\tverify partition table");
		puts("w\twrite table to disk and exit");
# if ENABLE_FEATURE_FDISK_ADVANCED
		puts("x\textra functionality (experts only)");
# endif
	}
}

# if ENABLE_FEATURE_FDISK_ADVANCED
static void
xmenu(void)
{
	puts("Command Action");
	puts("c\tchange number of cylinders");
	puts("h\tchange number of heads");
	puts("s\tchange number of sectors/track");
	if (LABEL_IS_AIX) {
		puts("b\tmove beginning of data in a partition");
		puts("e\tprint extended partitions");
	} else {
		puts("f\tfix partition order");               /* !aix */
		puts("b\tmove beginning of data in a partition");
		puts("e\tprint extended partitions");
	}
	puts("d\tprint raw data in partition table");
	puts("p\tprint partition table");
	puts("v\tverify partition table");
	puts("w\twrite table to disk and exit");
	puts("q\tquit without saving changes");
	puts("r\treturn to main menu");
}

static void
xselect(void)
{
	char c;

	while (1) {
		bb_putchar('\n');
		c = 0x20 | read_nonempty("Expert command (m for help): ");
		switch (c) {
		//deleted:
		//case 'a':
		//	if (LABEL_IS_SUN) sun_set_alt_cyl();
		//	break;
		case 'b':
			if (LABEL_IS_DOS)
				move_begin(input_partition_number(0, g_partitions));
			break;
		case 'c':
			user_cylinders = g_cylinders =
				read_int(1, g_cylinders, 1048576, 0,
					"Number of cylinders");
			if (LABEL_IS_DOS)
				warn_cylinders();
			break;
		case 'd':
			print_raw();
			break;
		case 'e': // "list extended partitions"
			if (LABEL_IS_DOS)
				x_dos_print_disklabel(1);
			break;
		case 'f':
			if (LABEL_IS_DOS)
				fix_partition_table_order();
			break;
		//SGI_LABEL code deleted:
		//case 'g':
		//	create_sgilabel();
		//	break;
		case 'h':
			user_heads = g_heads = read_int(1, g_heads, 256, 0, "Number of heads");
			update_units();
			break;
		//deleted:
		//case 'i':
		//	if (LABEL_IS_SUN) sun_set_ilfact();
		//	break;
		//case 'o':
		//	if (LABEL_IS_SUN) sun_set_rspeed();
		//	break;
		case 'p':
			x_dos_print_disklabel(0);
			break;
		case 'q':
			if (ENABLE_FEATURE_CLEAN_UP)
				close_dev_fd();
			bb_putchar('\n');
			exit_SUCCESS();
		case 'r':
			return;
		case 's':
			user_sectors = g_sectors = read_int(1, g_sectors, 63, 0, "Number of sectors");
			if (dos_compatible_flag) {
				offset_after_MBR_and_ext = g_sectors;
				printf("Warning: setting sector offset %u for DOS "
					"compatibility\n", g_sectors);
			}
			update_units();
			break;
		case 'v':
			verify();
			break;
		case 'w':
			write_table_and_exit();
			break;
		//deleted:
		//case 'y':
		//	if (LABEL_IS_SUN) sun_set_pcylcount();
		//	break;
		default:
			xmenu();
		}
	}
}
# endif /* ADVANCED mode */

static void
unknown_command(int c)
{
	printf("%c: unknown command\n", c);
}
#endif /* FEATURE_FDISK_WRITABLE */

int fdisk_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int fdisk_main(int argc UNUSED_PARAM, char **argv)
{
	unsigned opt;
	/*
	 *  fdisk -v
	 *  fdisk -l [-b sectorsize] [-u] device ...
	 *  fdisk -s [partition] ...
	 *  fdisk [-b sectorsize] [-u] device
	 *
	 * Options -C, -H, -S set the geometry.
	 */
	INIT_G();

	close_dev_fd(); /* needed: fd 3 must not stay closed */

	//G.opt_t = NULL;
	opt = getopt32(argv, "^" "b:+C:+H:+lS:+ut:"IF_FEATURE_FDISK_BLKSIZE("s")"\0"
		/* among -s and -l, the last one takes precedence */
		IF_FEATURE_FDISK_BLKSIZE("s-l:l-s"),
		&sector_size, // -b
		&user_cylinders, &user_heads, &user_sectors, //-CHS
		&G.opt_t
	);
	argv += optind;

#if ENABLE_FEATURE_FDISK_BLKSIZE
	/* -s ignores -b SECTSIZE, has to be before OPT_b check */
	if (opt & OPT_s) {
		int j;

		if (!argv[0])
			bb_show_usage();
		for (j = 0; argv[j]; j++) {
			ullong size;
			int fd = xopen(argv[j], O_RDONLY);
			size = bb_getsize_in_512sect(fd) / 2;
//NB: util-linux 2.41.1 says: "-s,--getsz: display device size in 512-byte sectors"
//but in fact, util-linux 2.41.1 shows the size in KILOBYTES!
			close(fd);
			if (!argv[1])
				printf("%llu\n", size);
			else
				printf("%s: %llu\n", argv[j], size);
		}
		return 0;
	}
#endif

	//if (opt & OPT_b) {
		/* Ugly: this sector size is really per device,
		 * so cannot be combined with multiple disks,
		 * and the same goes for the C/H/S options.
		 */
		if (sector_size < 512
		 || sector_size > 0x10000
		 || (sector_size & (sector_size-1)) /* not power of 2 */
		) {
			bb_show_usage();
		}
	//}
	if (user_heads <= 0 || user_heads >= 256)
		user_heads = 0;
	if (user_sectors <= 0 || user_sectors >= 64)
		user_sectors = 0;

	if (!ENABLE_FEATURE_FDISK_WRITABLE || (opt & OPT_l)) {
		if (*argv) {
			do {
				open_list_and_close(*argv, 1);
			} while (*++argv);
		} else {
			/* we don't have device names, */
			/* use /proc/partitions instead */
			list_devs_in_proc_partititons();
		}
		return 0;
	}

#if ENABLE_FEATURE_FDISK_WRITABLE
	if (!argv[0] || argv[1])
		bb_show_usage();

	// Unless -l or -s, util-linux 2.41.1 prints this message
	// even before trying to open the device (try "fdisk /dev/bogus").
	puts( //"Welcome to fdisk (busybox "BB_VER")\n" //should we print this too?
	"Changes will remain in memory only, until you decide to write them.\n"
	"Be careful before using the write command."
	);

	disk_device = argv[0];
	get_boot(OPEN_MAIN);

	if (LABEL_IS_OSF) {
		// OSF label, and no DOS label
		printf("Detected an OSF/1 disklabel on %s, entering "
			"disklabel mode\n", disk_device);
		if_osf_label_loop_in_menu_until_r();
		// ^^^ returned: it's not an OSF, or user selected 'return to main menu'
		current_label_type = LABEL_DOS; // Why do we do this? It seems counter-intuitive
	}

	while (1) {
		int c;
		bb_putchar('\n');
		c = 0x20 | read_nonempty("Command (m for help): ");
		switch (c) {
		case 'a':
			if (LABEL_IS_DOS)
				toggle_active(input_partition_number(1, g_partitions));
			else
				goto unknown_cmd;
			break;
		case 'b':
# if ENABLE_FEATURE_OSF_LABEL
			if_osf_label_loop_in_menu_until_r();
# endif
			break;
		case 'c':
			if (LABEL_IS_DOS)
				toggle_dos_compatibility_flag();
			else
				goto unknown_cmd;
			break;
		case 'd':
			{
				int j = get_existing_partition(1, g_partitions);
				if (j >= 0)
					delete_partition(j);
			}
			break;
		//deleted:
		//case 'i':
		//	if (LABEL_IS_SGI) create_sgiinfo(); else goto unknown_cmd;
		case 'l':
			list_types(get_sys_types());
			break;
		case 'm':
			menu();
			break;
		case 'n':
			new_partition();
			break;
		case 'o':
			create_doslabel();
			dos_reset_primary_partition_info();
			break;
		case 'p':
			print_disklabel(0);
			break;
		case 'q':
			if (ENABLE_FEATURE_CLEAN_UP)
				close_dev_fd();
			bb_putchar('\n');
			return 0;
		//SUN_LABEL code deleted:
		//case 's':
		//	create_sunlabel();
		//	break;
		case 't':
			change_sysid();
			break;
		case 'u':
			change_units();
			break;
		case 'v':
			verify();
			break;
		case 'w':
			write_table_and_exit();
			break;
# if ENABLE_FEATURE_FDISK_ADVANCED
		case 'x':
			xselect();
			break;
# endif
		default:
 unknown_cmd:
			unknown_command(c);
			menu();
		}
	}
	return 0;
#endif /* FEATURE_FDISK_WRITABLE */
}
