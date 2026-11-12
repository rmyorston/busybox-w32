/*
 * volume_id - reads filesystem label and uuid
 *
 * Copyright (C) 2004 Kay Sievers <kay.sievers@vrfy.org>
 *
 *	This library is free software; you can redistribute it and/or
 *	modify it under the terms of the GNU Lesser General Public
 *	License as published by the Free Software Foundation; either
 *	version 2.1 of the License, or (at your option) any later version.
 *
 *	This library is distributed in the hope that it will be useful,
 *	but WITHOUT ANY WARRANTY; without even the implied warranty of
 *	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 *	Lesser General Public License for more details.
 *
 *	You should have received a copy of the GNU Lesser General Public
 *	License along with this library; if not, write to the Free Software
 *	Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */
//config:config FEATURE_VOLUMEID_FAT
//config:	bool "fat filesystem"
//config:	default y
//config:	depends on VOLUMEID

//kbuild:lib-$(CONFIG_FEATURE_VOLUMEID_FAT) += fat.o

#include "volume_id_internal.h"

/* linux/msdos_fs.h says: */
#define FAT12_MAX			0xff4
#define FAT16_MAX			0xfff4
#define FAT32_MAX			0x0ffffff6
// These are the maximum allowed counts of clusters, *not* the maximum cluster#!
// Example: the cluster numbers on largest possible FAT12 fs are 2..FAT12_MAX+1
// (clusters 0 and 1 do not exist).
// General rules for cluster numbers in FAT table:
// 0x0: is "free clister"
// 0x1: reserved
// from 0x2 to 0xF..F5 are valid cluster numbers (compare with the above constants).
// 0xF..F6 reserved (but probably can be a valid cluster# on a large FAT32 volume?)
// 0xF..F7 is "has bad sector(s)" (but probably can be a valid cluster# on a large FAT32 volume?)
// from 0xF..F8 to 0xF..FF are "no next cluster in this chain".
// FAT32 ignores upper 4 bits of 32-bit cluster number.

#define FAT_ATTR_VOLUME_ID		0x08
#define FAT_ATTR_DIR			0x10
#define FAT_ATTR_LONG_NAME		0x0f
#define FAT_ATTR_MASK			0x3f
#define FAT_ENTRY_FREE			0xe5

struct vfat_super_block {
	uint8_t		boot_jump[3];
	uint8_t		sysid[8];            /* 003 name - can be used to special case partition manager volumes */
	uint16_t	sector_size_bytes;   /* 00b bytes per logical sector */
	uint8_t		sectors_per_cluster; /* 00d sectors/cluster */
	uint16_t	reserved_sct;        /* 00e reserved sectors (sector offset of 1st FAT relative to volume start) */
	uint8_t		fats;                /* 010 number of FATs */
	uint16_t	dir_entries;         /* 011 root directory entries, must be 0 for FAT32 */
	uint16_t	volume_size_sct16;   /* 013 volume size in sectors, must be 0 for FAT32 */
	uint8_t		media;               /* 015 media code */
	uint16_t	fat_sct16;           /* 016 sectors/FAT, must be 0 for FAT32 */
	uint16_t	secs_track;          /* 018 sectors per track */
	uint16_t	heads;               /* 01a number of heads */
	uint32_t	hidden;              /* 01c hidden sectors (sector offset of volume within physical disk) */
	uint32_t	volume_size_sct32;   /* 020 volume size in sectors (if volume_size_sct16 == 0) */
	union {
		struct fat_super_block {
			uint8_t		drive_number;           /* 024 */
			uint8_t		boot_flags;             /* 025 */
			uint8_t		ext_boot_sign;          /* 026 0x28 - DOS 3.4 EBPB; 0x29 - DOS 4.0 EBPB */
			uint8_t		serno[4];               /* 027 volume serial number */
			uint8_t		label[11];              /* 02b volume label (exists if ext_boot_sign=0x29) */
			uint8_t		magic[8];               /* 036 typically "FATnn" (exists if ext_boot_sign=0x29) */
			uint8_t		dummy2[0x1fe - 0x3e];   /* 03e */
			uint8_t		pmagic[2];
		} PACKED fat;
		struct fat32_super_block {
			uint32_t	fat_sct32;              /* 024 sectors/FAT */
			uint16_t	flags;                  /* 028 bit 8: fat mirroring, low 4: active fat */
			uint8_t		version[2];             /* 02a major, minor filesystem version (I see 0,0) */
			uint32_t	root_cluster;           /* 02c first cluster in root directory */
			uint16_t	fsinfo_sct;             /* 030 filesystem info sector (usually 1) */
			uint16_t	backup_boot_sct;        /* 032 backup boot sector (usually 6, 0: none) */
			uint8_t		boot_file[12];          /* 034 Boot file name (boot code may search root dir for this name) */
			uint8_t		drive_number;           /* 040 BIOS drive number */
			uint8_t		reserved3;              /* 041 unused */
			uint8_t		ext_boot_sign;          /* 042 0x29 if fields below exist (DOS 3.3+) */
			uint8_t		serno[4];               /* 043 volume serial number */
			uint8_t		label[11];              /* 047 volume label */
			uint8_t		magic[8];               /* 052 typically "FATnn" */
// Wikipedia: "If both total logical sectors entries at offset 0x020 and 0x013
// are 0 on volumes using a FAT32 EBPB with signature 0x29, volumes with more
// than 4,294,967,295 (2^32-1) sectors (f.e. some DR-DOS volumes with 32-bit
// cluster entries) can use this entry as 64-bit total logical sectors entry
// instead. In this case, the OEM label at sector offset 0x003 may be retrieved
// as new-style file system type instead."
			uint8_t		dummy2[0x1fe - 0x5a];   /* 05a */
			uint8_t		pmagic[2];
		} PACKED fat32;
	} PACKED type;
} PACKED;

struct vfat_dir_entry {
	uint8_t		name[11];
	uint8_t		attr;
	uint16_t	time_creat;
	uint16_t	date_creat;
	uint16_t	time_acc;
	uint16_t	date_acc;
	uint16_t	cluster_high;
	uint16_t	time_write;
	uint16_t	date_write;
	uint16_t	cluster_low;
	uint32_t	size;
} PACKED;

static uint8_t *get_attr_volume_id(struct vfat_dir_entry *dir, int count)
{
	for (;--count >= 0; dir++) {
		/* end marker */
		if (dir->name[0] == 0x00) {
			dbg("end of dir");
			break;
		}

		/* empty entry */
		if (dir->name[0] == FAT_ENTRY_FREE)
			continue;

		/* long name */
		if ((dir->attr & FAT_ATTR_MASK) == FAT_ATTR_LONG_NAME)
			continue;

		if ((dir->attr & (FAT_ATTR_VOLUME_ID | FAT_ATTR_DIR)) == FAT_ATTR_VOLUME_ID) {
			/* labels do not have file data */
			if (dir->cluster_high != 0 || dir->cluster_low != 0)
				continue;

			dbg("found ATTR_VOLUME_ID id in root dir");
			return dir->name;
		}

		dbg("skip dir entry");
	}

	return NULL;
}

int FAST_FUNC volume_id_probe_vfat(struct volume_id *id /*,uint64_t fat_partition_off*/)
{
#define fat_partition_off ((uint64_t)0)
	struct vfat_super_block *vs;
	struct vfat_dir_entry *dir;
	uint16_t sector_size_bytes;
	uint32_t rootdir_entries; /* 16 bits really, but wider type works better */
	uint32_t rootdir_size_sct;
	uint32_t sect_count;
	uint32_t reserved_sct; /* 16 bits really, but wider type works better */
	uint32_t fat_size_sct;
	uint32_t cluster_count;
	uint64_t max_cluster_count_in_fat;
	uint64_t root_start_off;
	uint32_t after_fat_sct;
	uint32_t next_cluster;
	uint8_t *buf;
	uint32_t buf_size;
	uint8_t *label = NULL;
	int maxloop;

	dbg("probing at offset 0x%llx", (unsigned long long) fat_partition_off);

	vs = volume_id_get_buffer(id, fat_partition_off, 0x200);
	if (vs == NULL)
		return -1;

	/* believe only that it is FAT, don't trust the version -
	 * the cluster count will tell us
	 */
	if (memcmp(vs->sysid, "NTFS", 4) == 0)
		return -1;
	if (memcmp(vs->type.fat32.magic, "MSWIN", 5) == 0)
		goto valid;
	if (memcmp(vs->type.fat32.magic, "FAT32   ", 8) == 0)
		goto valid;
	if (memcmp(vs->type.fat.magic, "FAT16   ", 8) == 0)
		goto valid;
	if (memcmp(vs->type.fat.magic, "MSDOS", 5) == 0)
		goto valid;
	if (memcmp(vs->type.fat.magic, "FAT12   ", 8) == 0)
		goto valid;

	/*
	 * There are old floppies out there without a magic, so we check
	 * for well known values and guess if it's a fat volume
	 */

	/* boot jump address check */
	if ((vs->boot_jump[0] != 0xeb || vs->boot_jump[2] != 0x90)
	 && vs->boot_jump[0] != 0xe9
	) {
		return -1;
	}
//TODO: the above check is bogus, FAT partitions are not required to contain boot code
	/* media check */
	if (vs->media < 0xf8 && vs->media != 0xf0)
		return -1;

 valid:
	/* fat count */
	if (vs->fats != 2 && vs->fats != 1)
		return -1;
	/* sector size check */
	sector_size_bytes = get_unaligned_le16(&vs->sector_size_bytes);
	dbg("sector_size_bytes 0x%x", sector_size_bytes);
	if (sector_size_bytes != 0x200 && sector_size_bytes != 0x400
	 && sector_size_bytes != 0x800 && sector_size_bytes != 0x1000
	) {
		return -1;
	}
	/* sectors_per_cluster check */
	dbg("sectors_per_cluster 0x%x", vs->sectors_per_cluster);
	if (vs->sectors_per_cluster == 0
	 || (vs->sectors_per_cluster & (vs->sectors_per_cluster-1))
	) {
		return -1;
	}

	reserved_sct = le16_to_cpu(vs->reserved_sct); /* not unaligned */
	dbg("reserved_sct 0x%x", reserved_sct);
	if (reserved_sct == 0) /* FAT start sector can't be 0 */
		return -1;

	fat_size_sct = le16_to_cpu(vs->fat_sct16); /* not unaligned */
	if (fat_size_sct == 0)
		fat_size_sct = le32_to_cpu(vs->type.fat32.fat_sct32); /* not unaligned */
	// Zero here means it's not a FAT volume (FAT table can't have zero sectors).
	// Maximum size of FAT32 table is fewer than 0x100000000 32-bit elements
	// (if we imagine a version of FAT32 which does *not* clear top 4 bits
	// of cluster#, otherwise it is 16 times fewer still). 512-byte sector
	// contains 0x200/4=0x80 32-bit elements. 0x100000000/0x80=0x2000000 sectors.
	// A real FAT32 filesystem should not have more than that sectors per FAT.
	if (fat_size_sct == 0 || fat_size_sct > 0x02000000)
		return -1;
	dbg("fat_size_sct 0x%x", fat_size_sct);

	rootdir_entries = get_unaligned_le16(&vs->dir_entries);
	rootdir_size_sct = (rootdir_entries * sizeof(struct vfat_dir_entry) + (sector_size_bytes-1)) / sector_size_bytes;
	dbg("dir_size_sct 0x%x", rootdir_size_sct); /* zero on FAT32 */

	sect_count = get_unaligned_le16(&vs->volume_size_sct16);
	if (sect_count == 0)
		sect_count = le32_to_cpu(vs->volume_size_sct32); /* not unaligned */
	dbg("sect_count 0x%x", sect_count);

	after_fat_sct = reserved_sct + fat_size_sct * vs->fats;

	cluster_count = sect_count - (after_fat_sct + rootdir_size_sct);
	if (cluster_count > sect_count) /* subtraction underflowed? */
		return -1; /* data area can't be missing */
	cluster_count /= vs->sectors_per_cluster;
	if (cluster_count == 0)
		return -1; /* data area can't be missing */
	dbg("cluster_count 0x%x", cluster_count);

	// Microsoft insists that FAT type (12/16/32) is determined solely by cluster_count.
	// It's funny, because one of the steps to calculate it uses dir_entries (offset 0x11)
	// which _just by itself_ already determines whether FS is FAT12/16 or FAT32.
////	if (cluster_count <= FAT12_MAX) {
////		strcpy(id->type_version, "FAT12");
////	} else if (cluster_count <= FAT16_MAX) {
////		strcpy(id->type_version, "FAT16");
////	} else {
////		strcpy(id->type_version, "FAT32");
////		goto fat32;
////	}
//	if (cluster_count > FAT16_MAX)
////FIXME: what if cluster_count > FAT32_MAX too? return -1?
//		goto fat32;
	// HOWEVER! Linux kernel does not do this. It simply looks at vs->fat_sct16,
	// and if it is zero, then it considers the fs to be FAT32. This means that
	// Linux supports arbitrarily tiny FAT32! Let's detect them the same way,
	// to avoid being discrepant wrt what kernel does.
	if (vs->fat_sct16 == 0) /* not unaligned */
		goto fat32; /* FAT12/16 uses 16-bit field for FAT sector count, FAT32 uses fat_sct32 */

	if (rootdir_entries == 0)
		return -1; /* can't be zero for FAT12/16 */

	/* String "NO NAME" has a special meaning (empty label) only for label stored in boot sector */
	if (memcmp(vs->type.fat.label, bb_NO_NAME_11, 11) != 0) {
//		volume_id_set_label_raw(id, vs->type.fat.label, 11);
		volume_id_set_label_string(id, vs->type.fat.label, 11);
	}
	volume_id_set_uuid(id, vs->type.fat.serno, UUID_DOS);

	/* the label may be an attribute in the root directory */
	root_start_off = after_fat_sct * sector_size_bytes;
	dbg("root dir start 0x%llx", (unsigned long long) root_start_off);
	dbg("expected entries 0x%x", rootdir_entries);

	buf_size = rootdir_entries * sizeof(struct vfat_dir_entry);
	buf = volume_id_get_buffer(id, fat_partition_off + root_start_off, buf_size);
	if (buf == NULL)
		goto ret;

	label = get_attr_volume_id((struct vfat_dir_entry*) buf, rootdir_entries);
	goto ret;

 fat32:
	// fatgen103.doc, Microsoft's FAT32 File System Specification version 1.03:
//	if (get_unaligned16(&vs->volume_size_sct16) != 0) // "On FAT32 volumes this field must be 0"
//		return -1;
	// ^^^Linux kernel does not care whether volume size came from 16-bit or 32-bit field
//	if (rootdir_entries != 0) // "For FAT32 volumes, this field must be set to 0"
//		return -1;
	// ^^^Linux kernel allows non-zero here! Meaning, there can be a rootdir area
	// after FAT tables even on FAT32 - it will be ignored by Linux.

	// FATs can be too small to address the entire data area -
	// in this case decrease cluster_count
	max_cluster_count_in_fat = (uint64_t)fat_size_sct * (sector_size_bytes/4) - 2;
	if (cluster_count > max_cluster_count_in_fat)
		cluster_count = max_cluster_count_in_fat;

	/* String "NO NAME" has a special meaning (empty label) only for label stored in boot sector */
	if (memcmp(vs->type.fat32.label, bb_NO_NAME_11, 11) != 0) {
//		volume_id_set_label_raw(id, vs->type.fat32.label, 11);
		volume_id_set_label_string(id, vs->type.fat32.label, 11);
	}
	volume_id_set_uuid(id, vs->type.fat32.serno, UUID_DOS);

	/* FAT32 root dir is a cluster chain like any other directory */
	buf_size = vs->sectors_per_cluster * sector_size_bytes;
	next_cluster = le32_to_cpu(vs->type.fat32.root_cluster);
	maxloop = 100; /* in case rootdir cluster chain is in fact a loop */
	while (--maxloop) {
		uint64_t next_off_sct;
		uint64_t next_off;
		uint64_t fat_entry_off;
		int count;

		dbg("next_cluster 0x%x", (unsigned)next_cluster);
		if (next_cluster < 2 || next_cluster > cluster_count + 1)
			break;

		next_off_sct = (uint64_t)(next_cluster - 2) * vs->sectors_per_cluster;
		next_off = (after_fat_sct + next_off_sct) * sector_size_bytes;
		dbg("cluster offset 0x%llx", (unsigned long long) next_off);

		/* get cluster */
		buf = volume_id_get_buffer(id, fat_partition_off + next_off, buf_size);
		if (buf == NULL)
			goto ret;

		dir = (struct vfat_dir_entry*) buf;
		count = buf_size / sizeof(struct vfat_dir_entry);
		dbg("expected entries 0x%x", count);

		label = get_attr_volume_id(dir, count);
		if (label)
			break;

		/* get FAT entry */
		fat_entry_off = (reserved_sct * sector_size_bytes) + (next_cluster * sizeof(uint32_t));
		dbg("fat_entry_off 0x%llx", (unsigned long long)fat_entry_off);
		buf = volume_id_get_buffer(id, fat_partition_off + fat_entry_off, sizeof(uint32_t));
		if (buf == NULL)
			goto ret;

		/* set next cluster */
		next_cluster = le32_to_cpu(*(uint32_t*)buf) & 0x0fffffff;
	}
	if (maxloop == 0)
		dbg("reached maximum follow count of root cluster chain, give up");

 ret:
	if (label /* do not reject "NO NAME" here! This string a valid volume label if specified in the root dir */) {
//		volume_id_set_label_raw(id, label, 11);
		volume_id_set_label_string(id, label, 11);
	}

//	volume_id_set_usage(id, VOLUME_ID_FILESYSTEM);
	IF_FEATURE_BLKID_TYPE(id->type = "vfat";)

	return 0;
}
