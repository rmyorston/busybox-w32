/* vi: set sw=4 ts=4: */
/*
 * Copyright 2026 Denys Vlasenko <vda.linux@googlemail.com>
 *
 * Licensed under GPLv2, see file LICENSE in this source tree.
 */
//config:config SWAPLABEL
//config:	bool "swaplabel (1 kb)"
//config:	default y
//config:	help
//config:	swaplabel shows or sets swap's label.

//applet:IF_SWAPLABEL(APPLET(swaplabel, BB_DIR_SBIN, BB_SUID_DROP))

//kbuild:lib-$(CONFIG_SWAPLABEL) += swaplabel.o

//usage:#define swaplabel_trivial_usage
//usage:       "[-L LBL] BLOCKDEV"
//usage:#define swaplabel_full_usage "\n\n"
//usage:       "Show or set swap label on BLOCKDEV\n"
//usage:     "\n	-L LBL	Label"

#include "libbb.h"

/* from Linux 2.6.23 */
/*
 * Magic header for a swap area. ... Note that the first
 * kilobyte is reserved for boot loader or disk label stuff.
 */
struct swap_header_v1 {
/*	char     bootbits[1024];    Space for disklabel etc. */
	uint32_t version;        /* second kbyte, word 0 */
	uint32_t last_page;      /* 1 */
	uint32_t nr_badpages;    /* 2 */
	uint8_t  sws_uuid[16];   /* 3,4,5,6 */
	char     sws_volume[16]; /* 7,8,9,10 */
	uint32_t padding[117];   /* 11..127 */
	uint32_t badpages[1];    /* 128 */
	/* total 129 32-bit words in 2nd kilobyte */
} FIX_ALIASING;

int swaplabel_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int swaplabel_main(int argc UNUSED_PARAM, char **argv)
{
	char buf32[32];
	const char *label = NULL;
	int fd;

	// TODO: -U UUID
	getopt32(argv, "^" "L:" "\0" "=1"/*exactly one arg*/, &label);
	argv += optind;

	fd = xopen(argv[0], label ? O_RDWR : O_RDONLY);

	xlseek(fd, bb_getpagesize() - 10, SEEK_SET);
	xread(fd, buf32, 10);
	if (memcmp(buf32, bb_SWAPSPACE2, 10) != 0)
		bb_error_msg_and_die("%s: not a valid swap partition", argv[0]);
	// TODO: also check hdr.version == 1?

	xlseek(fd, 1024 + offsetof(struct swap_header_v1, sws_uuid) + 16 * (!!label), SEEK_SET);
	if (label) {
		int sz = strnlen(label, 15) + 1; // includes NUL unless full 16 bytes
		xwrite(fd, label, sz);
		// BTW, writing to active swap partition fails with ETXTBUSY!
	} else {
		char uuid_string37[37];

		xread(fd, buf32, 32);
		if (buf32[16])
			printf("LABEL: %.16s\n", buf32 + 16);

		// f.e. dfd9c173-be52-4d27-99a5-c34c6c2ff55f
		format_uuid_DCE_37_chars(uuid_string37, (uint8_t*)buf32);
		printf("UUID:  %s\n", uuid_string37);
	}

	if (ENABLE_FEATURE_CLEAN_UP)
		close(fd);

	fflush_stdout_and_exit_SUCCESS();
}
