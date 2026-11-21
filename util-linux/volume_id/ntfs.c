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
//config:config FEATURE_VOLUMEID_NTFS
//config:	bool "ntfs filesystem"
//config:	default y
//config:	depends on VOLUMEID

//kbuild:lib-$(CONFIG_FEATURE_VOLUMEID_NTFS) += ntfs.o

#include "volume_id_internal.h"

struct ntfs_super_block {
	uint8_t		jump[3];   // The beginning has a few fields "inherited" from FAT:
	uint8_t		oem_id[8];                      // 003 "NTFS    "
	uint16_t	bytes_per_sector;               // 00b bytes per logical sector
	uint8_t		sectors_per_cluster;            // 00d sectors/cluster. If > 0x80, value is 1 << (-byte_value)
	// Windows 10 v.1709 and Windows Server 2019 support clusters up to 2MB, these can have sectors_per_cluster > 0x80: 2MB cluster with 512-byte sectors is 1<<12 sectors, byte needs to be -12 = 0xf4
	uint16_t	reserved_sectors;               // 00e unused, 0
	uint8_t		fats;                           // 010 unused, 0
	uint16_t	root_entries;                   // 011 unused, 0
	uint16_t	sectors;                        // 013 unused, 0
	uint8_t		media_type;                     // 015 media code
	uint16_t	sectors_per_fat;                // 016 unused, 0
	uint16_t	sectors_per_track;              // 018 sectors per track
	uint16_t	heads;                          // 01a number of heads
	uint32_t	hidden_sectors;                 // 01c hidden sectors (sector offset of volume within physical disk)
	uint32_t	large_sectors;                  // 020 unused, 0 <--- FAT-like fields end here
	uint32_t	unused;                         // 024
	uint64_t	number_of_sectors;              // 028
	uint64_t	mft_cluster_location;           // 030
	uint64_t	mft_mirror_cluster_location;    // 038
	int8_t		cluster_per_mft_record;         // 040 if < 0x80, value in clusters, else value is in bytes and = 1 << (-byte_value)
	uint8_t		reserved1[3];                   //
	int8_t		cluster_per_index_record;       // 044 (same rules wrt 0x80)
	uint8_t		reserved2[3];                   //
	uint8_t		volume_serial[8];               // 048
	uint16_t	checksum;                       // 050 (often zeroed)
} PACKED;

struct master_file_table_record {
	uint8_t		magic[4];               // 000 "FILE"
	uint16_t	usa_ofs;                // 004 offset to Update Sequence Array (often 0x0030)
	uint16_t	usa_count;              // 006 size in words of Update Sequence Array (often 0x0003)
	uint64_t	lsn;                    // 008 logfile sequence number
	uint16_t	sequence_number;        // 010
	uint16_t	link_count;             // 012 how many directory entries refer to here
	uint16_t	attrs_offset;           // 014 first attribute starts at this ofs (typically 0x0038?)
	uint16_t	flags;                  // 016
	uint32_t	bytes_in_use;           // 018 rounded up to 8 bytes?
	uint32_t	bytes_allocated;        // 01c typically set to MFT record size (0x400 = 1K)
	// uint64_t base_MFT_or_0;              // 020
	// uint16_t next_attr_id;		// 028
	// uint16_t pad;
	// uint32_t MFT_record_no;		// 02c zero-based
	// typically: uint16_t usa[3];          // 030
	//
	// When NTFS writes an MFT record, it:
	// * usa[0] is Update Sequence Number (USN): increments it.
	// * Saves the last two bytes of each sector in the record into usa[1..n].
	// * Replaces those bytes at the end of every sector with USN.
	// * Writes the record.
	// When reading, NTFS checks that each sector ends with the expected USN (usa[0]).
	// If they all match, it restores the original two bytes from the usa[1..n];
	// if one doesn’t, the record is treated as damaged / incompletely written.
} PACKED;

struct file_attribute {
	uint32_t	type;                   // 000
	uint32_t	len;                    // 004
	uint8_t		non_resident;           // 008
	uint8_t		name_len;               // 009
	uint16_t	name_offset;            // 00a
	uint16_t	flags;                  // 00c
	uint16_t	instance;               // 00e
	uint32_t	value_len;              // 010
	uint16_t	value_offset;           // 014
} PACKED;

struct volume_info {
	uint64_t	reserved;
	uint8_t		major_ver;
	uint8_t		minor_ver;
} PACKED;

#define MFT_RECORD_VOLUME			3
#define MFT_RECORD_ATTR_VOLUME_NAME		0x60
#define MFT_RECORD_ATTR_VOLUME_INFO		0x70
#define MFT_RECORD_ATTR_OBJECT_ID		0x40
#define MFT_RECORD_ATTR_END			0xffffffffu

int FAST_FUNC volume_id_probe_ntfs(struct volume_id *id /*,uint64_t off*/)
{
#define off ((uint64_t)0)
	unsigned sector_size;
	unsigned cluster_size;
	uint64_t mft_cluster;
	uint64_t mft_off;
	unsigned mft_record_size;
	unsigned attr_off;
	unsigned n;
	struct master_file_table_record *mftr;
	struct ntfs_super_block *ns;
	const uint8_t *buf;

	dbg("probing at offset 0x%llx", (unsigned long long) off);

	ns = volume_id_get_buffer(id, off, 0x200);
	if (ns == NULL)
		return -1;

	if (memcmp(ns->oem_id, "NTFS", 4) != 0)
		return -1;

	// Calculate and validate some parameters
	sector_size = le16_to_cpu(ns->bytes_per_sector);
	if (sector_size < 512 || (sector_size & (sector_size-1)))
		return -1; // too small, or not power-of-2
	n = ns->sectors_per_cluster;
	if (n > 0x80) { // (yes, >, not >=)
		n = -(int8_t)n;
		if (n >= 16) // sanity check
			return -1;
		n = 1 << n;
	}
	cluster_size = n * sector_size; // 0x8000 * 0x8000 maximum
	if (cluster_size == 0)
		return -1; // sectors_per_cluster == 0 is bogus
	if (cluster_size > 2*1024*1024)
		return -1; // > 0x1000000
	if (cluster_size & (cluster_size-1))
		return -1; // not power-of-2

	n = bb_popcnt_32(cluster_size - 1); // cluster_size_shift
	mft_cluster = le64_to_cpu(ns->mft_cluster_location);
	mft_off = mft_cluster << n;
	if ((int64_t)mft_off <= 0) // if 0: mft_cluster == 0, or overflow in multiplication
		return -1;         // if <0: might be valid, but so huge that it overflows off_t
	if ((mft_off >> n) != mft_cluster)
		return -1; // overflow in multiplication

	if (ns->cluster_per_mft_record < 0) {
		// normally 0xF6, denoting 1 << 10 = 1024 bytes
		n = -ns->cluster_per_mft_record;
		if (n > 16) // >64K???
			return -1;
		mft_record_size = 1 << n; // in bytes, not in clusters!
	} else
		mft_record_size = ns->cluster_per_mft_record * cluster_size; // 127 * 2MB maximum
	// M$ only supports 1KB and 4KB, we allow wider range, but future support
	// for extra large MFT records is unlikely - space usage with it would be very wasteful,
	// and many offset fields in the record are only 16-bit.
	if (mft_record_size < 512 || mft_record_size > 64*1024)
		return -1;

	// Ok, we conclude it's a valid NTFS volume

	volume_id_set_uuid(id, ns->volume_serial, UUID_NTFS);

	dbg("sectorsize  0x%x", sector_size);
	dbg("clustersize 0x%x", cluster_size);
	dbg("mftcluster  %llu", (unsigned long long) mft_cluster);
	dbg("mftoffset  0x%llx", (unsigned long long) mft_off);
	dbg("cluster per mft_record  %i", ns->cluster_per_mft_record);
	dbg("mft record size  %i", mft_record_size);

	// Read MFT record #3 (volume id, "$Volume")
	mft_off += off + (MFT_RECORD_VOLUME * mft_record_size);
	if ((off_t)(mft_off + mft_record_size) < 0) // does it overflow off_t?
		goto ret;
	buf = volume_id_get_buffer(id, mft_off, mft_record_size);
	if (buf == NULL)
		goto ret;

	mftr = (struct master_file_table_record*) buf;

	dbg("mftr->magic '%c%c%c%c'", mftr->magic[0], mftr->magic[1], mftr->magic[2], mftr->magic[3]);
	if (memcmp(mftr->magic, "FILE", 4) != 0)
		goto ret;

//FIXME: process mftr->usa_ofs / mftr->usa_count!

	attr_off = le16_to_cpu(mftr->attrs_offset);
	dbg("file $Volume's attributes are at offset %i", attr_off);

	while (1) {
		struct file_attribute *attr;
		unsigned attr_len;

		if (attr_off & 3)
			break;
		if (attr_off + sizeof(struct file_attribute) > mft_record_size)
			break;

		attr = (struct file_attribute*) &buf[attr_off];
		if (attr->type == cpu_to_le32(MFT_RECORD_ATTR_END))
			break;

		dbg("found attribute type 0x%x, len %i, at offset %i",
			le32_to_cpu(attr->type), le32_to_cpu(attr->len), attr_off);

//		if (attr->type == cpu_to_le32(MFT_RECORD_ATTR_VOLUME_INFO)) {
//			struct volume_info *info;
//			dbg("found info, len %i", val_len);
//			info = (struct volume_info*) (((uint8_t *) attr) + val_off);
//			snprintf(id->type_version, sizeof(id->type_version)-1,
//				 "%u.%u", info->major_ver, info->minor_ver);
//		}

		if (attr->type == cpu_to_le32(MFT_RECORD_ATTR_VOLUME_NAME)) {
			unsigned val_off;
			unsigned val_len;
			const uint8_t *val;

			val_off = le16_to_cpu(attr->value_offset);
			val_len = le32_to_cpu(attr->value_len);
			dbg("found label, len %i", val_len);
			if (val_len > VOLUME_ID_LABEL_SIZE)
				val_len = VOLUME_ID_LABEL_SIZE;
			if (attr_off + val_off + val_len > mft_record_size)
				break;
			val = ((uint8_t*)attr) + val_off;
//			volume_id_set_label_raw(id, val, val_len);
			volume_id_set_label_unicode16(id, val, LE, val_len);
			break;
		}

		attr_len = le32_to_cpu(attr->len);
		if (attr_len == 0 || attr_len >= 0x10000)
			break;
		attr_off += attr_len; // can't overflow 32 bits due to above check
	}

 ret:
//	volume_id_set_usage(id, VOLUME_ID_FILESYSTEM);
	IF_FEATURE_BLKID_TYPE(id->type = "ntfs";)

	return 0;
}
