#if ENABLE_FEATURE_AIX_LABEL
/*
 * Copyright (C) Andreas Neuper, Sep 1998.
 *
 * Licensed under GPLv2, see file LICENSE in this source tree.
 */

typedef struct {
	uint32_t magic;        /* expect AIX_LABEL_MAGIC */
	uint32_t fillbytes1[124];
	uint32_t physical_volume_id;
	uint32_t fillbytes2[124];
} aix_partition;

#define AIX_LABEL_MAGIC         0xc9c2d4c1
#define AIX_LABEL_MAGIC_SWAPPED 0xc1d4c2c9
#define AIX_INFO_MAGIC          0x00072959
#define AIX_INFO_MAGIC_SWAPPED  0x59290700

/*
 * Changes:
 * 1999-03-20 Arnaldo Carvalho de Melo <acme@conectiva.com.br>
 *     Internationalization
 *
 * 2003-03-20 Phillip Kesling <pkesling@sgi.com>
 *      Some fixes
 */

// Write-only vars, unfinished code?
//static smallint aix_other_endian; /* bool */
//static smallint aix_volumes = 1; /* max 15 */

/*
 * only dealing with free blocks here
 */

static void
aix_info(void)
{
	puts("\n"
"There is a valid AIX label on this disk.\n"
"Unfortunately Linux cannot handle these disks at the moment.\n"
"Nevertheless some advice:\n"
"1. fdisk will destroy its contents on write.\n"
"2. Be sure that this disk is NOT a still vital part of a volume group.\n"
"   (Otherwise you may erase the other disks as well, if unmirrored.)\n"
"3. Before deleting this physical volume be sure to remove the disk\n"
"   logically from your AIX machine. (Otherwise you become an AIXpert).\n"
	);
}

static int
check_aix_label(void)
{
	aix_partition *aixlabel = (void*)MBRbuffer;

	if (aixlabel->magic != AIX_LABEL_MAGIC
	 && aixlabel->magic != AIX_LABEL_MAGIC_SWAPPED
	) {
		current_label_type = LABEL_DOS;
//		aix_other_endian = 0;
		return 0;
	}
//	aix_other_endian = (aixlabel->magic == AIX_LABEL_MAGIC_SWAPPED);
	update_units();
	current_label_type = LABEL_AIX;
	g_partitions = 1016;
//	aix_volumes = 15;
	aix_info();
	/*aix_nolabel();*/              /* %% */
	/*aix_label = 1;*/              /* %% */
	return 1;
}
#endif  /* AIX_LABEL */
