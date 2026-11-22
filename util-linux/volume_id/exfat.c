/*
 * volume_id - reads filesystem label and uuid
 *
 * Copyright (C) 2012 S-G Bergh <sgb@systemasis.org>
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
//config:config FEATURE_VOLUMEID_EXFAT
//config:	bool "exFAT filesystem"
//config:	default y
//config:	depends on VOLUMEID
//config:	help
//config:	exFAT (extended FAT) is a proprietary file system designed especially
//config:	for flash drives. It has many features from NTFS, but with less
//config:	overhead. exFAT is used on most SDXC cards for consumer electronics.

//kbuild:lib-$(CONFIG_FEATURE_VOLUMEID_EXFAT) += exfat.o

#include "volume_id_internal.h"

#define EXFAT_SB_OFFSET		0
#define EXFAT_DIR_ENTRY_SZ	32

// Overall volume structure:
// 0      Boot Sector. Boot signature is 0x55,0xAA at byte 510
// 1..8   Extended Boot Sectors. Each must have 0x00,0x00,0x55,0xAA at the end of the sector (NOT at byte 508, if sector is > 512)
// 9      OEM Parameters. 48-byte records: (16-byte GUID, 32 bytes payload)
// 10     Reserved sector
// 11     Checksum sector
// 12..23 Copy of the above 12 sectors
//        [padding]
//        FAT#1
//        [FAT#2]
//        [padding]
//        Clusters

struct exfat_super_block {
/* 0x00 */	uint8_t		boot_jump[3];
/* 0x03 */	uint8_t		fs_name[8];		// "EXFAT   "
/* 0x0B */	uint8_t		must_be_zero[0x40-8-3];
/* 0x40 */	uint64_t	partition_offset;	// What FAT was calling "hidden sectors"
/* 0x48 */	uint64_t	volume_length;		// In sectors
/* 0x50 */	uint32_t	fat_offset;		// Sector address of 1st FAT
/* 0x54 */	uint32_t	fat_size;		// In sectors
/* 0x58 */	uint32_t	cluster_heap_offset;	// Sector address of Data Region
/* 0x5C */	uint32_t	cluster_count;		// (feels redundant, volume_length allows calculation of it)
/* 0x60 */	uint32_t	root_dir;		// Cluster address of Root Directory
/* 0x64 */	uint8_t		vol_serial_nr[4];	// Volume ID
/* 0x68 */	uint16_t	fs_revision;		// VV.MM (as of 2026, 1.0)
/* 0x6A */	uint16_t	vol_flags;		// bits: 0:active fat 1/2, 1:dirty, 2:I/O error (bad sector scan probably required); excluded from checksumming (both bytes)
/* 0x6C */	uint8_t		sector_size_shift;	// Power of 2: 9 => 512, 12 => 4096
/* 0x6D */	uint8_t		cluster_sct_shift;	// Power of 2
/* 0x6E */	uint8_t		nr_of_fats;		// 2 for TexFAT
/* 0x6F */	uint8_t		BIOS_drive_no;		// usually 0x80
/* 0x70 */	uint8_t		percent_used;		// excluded from checksumming
/* 0x71 */	uint8_t		unused;
/* 0x72 boot_code */
/* 0x1fe boot_sign[2] */
} PACKED;

// Maximum total clusters: 0xFFFFFFF5.
// Maximum valid cluster no is 0xFFFFFFF6.
// FAT[0] entry is 0xFFFFFFF8 (IOW: unlike FAT fs, "media byte" is fixed: 0xF8)
// 0xFFFFFFF7 is "bad cluster"
// 0xFFFFFFFF is "last cluster", 0xFFFFFFF8..E *not* used as "last cluster"

// Microsoft recommends that a newly formatted volume should have clusters
// allocated for Allocation Bitmap(s), Upcase Table, rootdir (in this order).

// Some directory entry types:
// 0x00     end of directory
// 0x01..7F unused / deleted
// 0x80     do not use this value
// 0x81     Allocation bitmap
// 0x82     Upcase table
// 0x83     Volume Label
// 0x85     File or directory
// 0xA0     Volume GUID
// 0xA1     TexFAT Padding
// 0xA2     Windows CE Access Control Table
// 0xC0     Stream Extension
// 0xC1     File Name
// 0xC2     Windows CE Access Control List
// 0xE0     Vendor Extension
// 0xE1     Vendor Allocation
struct exfat_dir_entry {
/* 0x00 */	uint8_t		entry_type;
		union {
			struct volume_label {
/* 0x01 */			uint8_t		char_count;		// Length of label
/* 0x02 */			uint16_t	vol_label[11];		// UTF16 string without null termination
/* 0x18 */			uint8_t		reserved[8];
/* 0x20 */		} PACKED label;
			struct volume_guid {
/* 0x01 */			uint8_t		sec_count;
/* 0x02 */			uint16_t	set_checksum;
/* 0x04 */			uint16_t	flags;
/* 0x06 */			uint8_t		vol_guid[16];
/* 0x16 */			uint8_t		reserved[10];
/* 0x20 */		} PACKED guid;
		} PACKED type;
} PACKED;

int FAST_FUNC volume_id_probe_exfat(struct volume_id *id /*,uint64_t off*/)
{
	struct exfat_super_block *sb;
	struct exfat_dir_entry *de;
	uint64_t root_dir_off;
	unsigned cluster_sz;
	unsigned dirpos;
	unsigned need_lbl_guid;

	// Primary super block
	dbg("exFAT: probing at offset 0x%x", EXFAT_SB_OFFSET);
	sb = volume_id_get_buffer(id, EXFAT_SB_OFFSET, sizeof(*sb));
	if (!sb)
		return -1;
	if (memcmp(sb->fs_name, "EXFAT   ", 8) != 0)
		return -1;
	// spec: 512..4096 byte sectors, clusters up to 32M
	if (sb->sector_size_shift < 9
	 || sb->sector_size_shift > 12
	 || sb->cluster_sct_shift > (25 - sb->sector_size_shift)
	)
		return -1;

	// Use DOS uuid as fallback, if no GUID set
	volume_id_set_uuid(id, sb->vol_serial_nr, UUID_DOS);

	if (le32_to_cpu(sb->root_dir) < 2)
		return -1;
	root_dir_off = (((uint64_t)(le32_to_cpu(sb->root_dir) - 2) << sb->cluster_sct_shift) // which sector (relative to cluster_heap) rootdir starts?
			+ le32_to_cpu(sb->cluster_heap_offset))                              // + which sector cluster_heap starts
			<< sb->sector_size_shift;                                            // ^^^ multiply the above by sector size
	dbg("exFAT: sector size 0x%x bytes", 1 << sb->sector_size_shift);
	dbg("exFAT: root dir is at 0x%llx", (long long)root_dir_off);
// TODO: we scan only first cluster of rootdir (128 entries if cluster is 4K)
// fat.c has code to walk FAT table, do the same here
// NB: unlike FAT32, exFAT uses full 32 bits in FAT entries. 0xFFFFFFF7 is bad cluster, EOF is 0xFFFFFFFF
	cluster_sz = (1 << sb->sector_size_shift) << sb->cluster_sct_shift;
	dbg("exFAT: cluster size 0x%x bytes", cluster_sz);
	need_lbl_guid = (1 << 0) | (1 << 1);
	for (dirpos = 0; dirpos < cluster_sz; dirpos += EXFAT_DIR_ENTRY_SZ) {
		de = volume_id_get_buffer(id, root_dir_off + dirpos, EXFAT_DIR_ENTRY_SZ);
		if (de == NULL)
			break;
		if (de->entry_type == 0x00) {
			// End of Directory Marker
			dbg("exFAT: End of root directory reached after %u entries", dirpos / EXFAT_DIR_ENTRY_SZ);
			break;
		}
		if (de->entry_type == 0x83) {
			// Volume Label Directory Entry
			unsigned char_count = de->type.label.char_count;
			// at most 11 chars; more would read past the 32-byte entry
			if (char_count > ARRAY_SIZE(de->type.label.vol_label))
				char_count = ARRAY_SIZE(de->type.label.vol_label);
			volume_id_set_label_unicode16(id, (uint8_t *)de->type.label.vol_label,
						LE, 2 * char_count);
			need_lbl_guid &= ~(1 << 0);
		}
		if (de->entry_type == 0xA0) {
			// Volume GUID Directory Entry
			volume_id_set_uuid(id, de->type.guid.vol_guid, UUID_DCE);
			need_lbl_guid &= ~(1 << 1);
		}
		if (!need_lbl_guid)
			break;
	}

	IF_FEATURE_BLKID_TYPE(id->type = "exfat";)
	return 0;
}
