#if ENABLE_FEATURE_GPT_LABEL
/*
 * Copyright (C) 2010 Kevin Cernekee <cernekee@gmail.com>
 *
 * Licensed under GPLv2, see file LICENSE in this source tree.
 */

#define GPT_MAGIC 0x5452415020494645ULL
enum {
	LEGACY_GPT_TYPE = 0xee,
	GPT_MAX_PARTS   = 256,
	GPT_MAX_PART_ENTRY_LEN = 4096,
	GUID_LEN        = 16,
};

typedef struct gpt_header {
	uint64_t magic;
	uint32_t revision;
	uint32_t hdr_size;
	uint32_t hdr_crc32;
	uint32_t reserved;
	uint64_t current_lba;
	uint64_t backup_lba;
	uint64_t first_usable_lba;
	uint64_t last_usable_lba;
	uint8_t  disk_guid[GUID_LEN];
	uint64_t first_part_lba;
	uint32_t n_parts;
	uint32_t part_entry_len;
	uint32_t part_array_crc32;
} gpt_header;

typedef struct gpt_partition {
	uint8_t  type_guid[GUID_LEN];
	uint8_t  part_guid[GUID_LEN];
	uint64_t lba_start;
	uint64_t lba_end;
	uint64_t flags;
	uint16_t name36[36];
} gpt_partition;

static gpt_partition *
gpt_part(int i)
{
	if (i >= G.gpt_n_parts) {
		return NULL;
	}
	return (gpt_partition *)&G.gpt_part_array[i * G.gpt_part_entry_len];
}

static uint32_t
gpt_crc32(void *buf, int len)
{
	return ~crc32_block_endian0(0xffffffff, buf, len, global_crc32_table);
}

static void
gpt_print_guid(uint8_t *buf)
{
	printf(
		"%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
		buf[3], buf[2], buf[1], buf[0], /* GPT byteswaps... */
		buf[5], buf[4], /* ...these */
		buf[7], buf[6], /* ...fields */
		buf[8], buf[9],
		buf[10], buf[11], buf[12], buf[13], buf[14], buf[15]);
}

static void
gpt_print_wide36(uint16_t *s)
{
#if ENABLE_UNICODE_SUPPORT
	char buf[37 * 4];
	wchar_t wc[37];
	int i = 0;
	while (i < ARRAY_SIZE(wc)-1) {
		if (s[i] == 0)
			break;
		wc[i] = s[i];
		i++;
	}
	wc[i] = 0;
	if (wcstombs(buf, wc, sizeof(buf)) <= sizeof(buf)-1)
		fputs_stdout(printable_string(buf));
#else
	char buf[37];
	int i = 0;
	while (i < ARRAY_SIZE(buf)-1) {
		if (s[i] == 0)
			break;
		buf[i] = (0x20 <= s[i] && s[i] < 0x7f) ? s[i] : '?';
		i++;
	}
	buf[i] = '\0';
	fputs_stdout(buf);
#endif
}

static void
gpt_list_table(int xtra UNUSED_PARAM)
{
	int i;
	char numstr6[6];

	list_disk_name_and_sizes();
	printf("Logical sector size: %u\n", sector_size);
	printf("Disk identifier (GUID): ");
//util-linux 2.41.1 does not print " (GUID)" in above line,
//we do: keep users less confused what kind of identifier is meant.
	gpt_print_guid(G.gpt_hdr->disk_guid);
	printf("\nPartition table holds up to %u entries\n",
		(int)SWAP_LE32(G.gpt_hdr->n_parts));
	printf("First usable sector is %llu, last usable sector is %llu\n\n",
		(ullong)SWAP_LE64(G.gpt_hdr->first_usable_lba),
		(ullong)SWAP_LE64(G.gpt_hdr->last_usable_lba)
	);

/* "GPT fdisk" has a concept of 16-bit extension of the original MBR 8-bit type codes,
 * which it displays here: its output columns are ... Size Code Name
 * They are their own invention and are not stored on disk.
 * Looks like they use them to support "hybrid" GPT: for example, they have
 *   AddType(0x8307, "69DAD710-2CE4-4E3C-B16C-21A1D49ABED3", "Linux ARM32 root (/)");
 * and then (code>>8) matches what you need to put into MBR's type field for such a partition.
 * To print those codes, we'd need a GUID lookup table. Lets just drop the "Code" column instead:
 */
	puts("Number  Start (sector)    End (sector)  Size Name");
	//    123456 123456789012345 123456789012345 12345 abc
	for (i = 0; i < G.gpt_n_parts; i++) {
		gpt_partition *p = gpt_part(i);
		if (p->lba_start) {
			smart_ulltoa5((1 + SWAP_LE64(p->lba_end) - SWAP_LE64(p->lba_start)) * sector_size,
				numstr6, " KMGTPEZY")[0] = '\0';
			printf("%6u %15llu %15llu %s ",
				i + 1,
				(ullong)SWAP_LE64(p->lba_start),
				(ullong)SWAP_LE64(p->lba_end),
				numstr6
			);
			gpt_print_wide36(p->name36);
			bb_putchar('\n');
		}
	}
}

static int
check_gpt_label(void)
{
	unsigned part_array_len;
	struct partition *first = pt_offset(MBRbuffer, 0);
	struct pte pe;
	uint32_t crc;

	current_label_type = LABEL_DOS;

	/* LBA 0 contains the legacy MBR */

	if (!valid_part_table_flag(MBRbuffer)
	 || first->sys_ind != LEGACY_GPT_TYPE
	) {
		return 0;
	}

	/* LBA 1 contains the GPT header */

	read_pte(&pe, 1);
	G.gpt_hdr = (void *)pe.sectorbuffer;

	if (G.gpt_hdr->magic != SWAP_LE64(GPT_MAGIC)) {
		return 0;
	}

	init_unicode();
	if (!global_crc32_table) {
		global_crc32_new_table_le();
	}

	crc = SWAP_LE32(G.gpt_hdr->hdr_crc32);
	G.gpt_hdr->hdr_crc32 = 0;
	if (gpt_crc32(G.gpt_hdr, SWAP_LE32(G.gpt_hdr->hdr_size)) != crc) {
		/* FIXME: read the backup table */
		puts("\nwarning: GPT header CRC is invalid");
	}

	G.gpt_n_parts = SWAP_LE32(G.gpt_hdr->n_parts);
	G.gpt_part_entry_len = SWAP_LE32(G.gpt_hdr->part_entry_len);
	if (G.gpt_n_parts > GPT_MAX_PARTS
	 || G.gpt_part_entry_len > GPT_MAX_PART_ENTRY_LEN
	 || SWAP_LE32(G.gpt_hdr->hdr_size) > sector_size
	) {
		puts("\nwarning: can't parse GPT disklabel");
		return 0;
	}

	part_array_len = G.gpt_n_parts * G.gpt_part_entry_len;
	G.gpt_part_array = xmalloc(part_array_len);
	seek_sector(SWAP_LE64(G.gpt_hdr->first_part_lba));
	if (full_read(dev_fd, G.gpt_part_array, part_array_len) != part_array_len) {
		fdisk_fatal(unable_to_read);
	}

	if (gpt_crc32(G.gpt_part_array, part_array_len) != G.gpt_hdr->part_array_crc32) {
		/* FIXME: read the backup table */
		puts("\nwarning: GPT array CRC is invalid");
	}

	fputs_stdout("Found valid GPT with protective MBR; ");

	if (!G.opt_t || strcasecmp(G.opt_t, "gpt") == 0) {
		puts("using GPT (-t dos to override)");
		current_label_type = LABEL_GPT;
		return 1;
	}
	puts("NOT using it (-t specified)");
	return 0;
}

#endif /* GPT_LABEL */
