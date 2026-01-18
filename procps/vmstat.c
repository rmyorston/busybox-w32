/* vi: set sw=4 ts=4: */
/*
 * Report virtual memory statistics.
 *
 * Copyright (C) 2025 David Leonard
 *
 * Licensed under GPLv2, see file LICENSE in this source tree.
 */
//config:config VMSTAT
//config:	bool "vmstat (3 kb)"
//config:	default y
//config:	help
//config:	Report virtual memory statistics

//applet:IF_VMSTAT(APPLET(vmstat, BB_DIR_BIN, BB_SUID_DROP))

//kbuild:lib-$(CONFIG_VMSTAT) += vmstat.o

#include "libbb.h"
#include "common_bufsiz.h"

/* Must match option string! */
enum {
	OPT_n = 1 << 0,
};

#define FROM_PROC_STAT          "\1"
#define FROM_PROC_STAT_CPU      "\2"
#define FROM_PROC_VMSTAT        "\3"
#define FROM_PROC_MEMINFO       "\4"
#define M_DELTA                 "\x81"  /* differentiate */
#define M_DPERCENT              "\x83"  /* DELTA + sum and scale to 100 */
#define M_DECREMENT             "\x84"  /* decrement (exclude self proc) */
#define PSEUDO_SWPD             "_1"    /* SwapTotal - SwapFree */
#define PSEUDO_CACHE            "_2"    /* Cached + SReclaimable */

#if 1
#define FIXED_HEADER 1
#define L(a) /*nothing*/
#else
#define FIXED_HEADER 0
#define L(a) a
#endif

/* Column descriptors */
static const char coldescs[] =
/* [grplabel\0] \width   label\0  from              [m_mod]      fromspec\0 */
L("procs\0")   "\2"   L("r\0"    )FROM_PROC_STAT     M_DECREMENT "procs_running\0"
               "\2"   L("b\0"    )FROM_PROC_STAT                 "procs_blocked\0"
L("memory\0")  "\6"   L("swpd\0" )FROM_PROC_MEMINFO              PSEUDO_SWPD "\0"
               "\6"   L("free\0" )FROM_PROC_MEMINFO              "MemFree\0"
               "\6"   L("buff\0" )FROM_PROC_MEMINFO              "Buffers\0"
               "\6"   L("cache\0")FROM_PROC_MEMINFO              PSEUDO_CACHE "\0"
L("swap\0")    "\4"   L("si\0"   )FROM_PROC_VMSTAT   M_DELTA     "pswpin\0"
               "\4"   L("so\0"   )FROM_PROC_VMSTAT   M_DELTA     "pswpout\0"
L("io\0")      "\5"   L("bi\0"   )FROM_PROC_VMSTAT   M_DELTA     "pgpgin\0"
               "\5"   L("bo\0"   )FROM_PROC_VMSTAT   M_DELTA     "pgpgout\0"
L("system\0")  "\4"   L("in\0"   )FROM_PROC_STAT     M_DELTA     "intr\0"
               "\4"   L("cs\0"   )FROM_PROC_STAT     M_DELTA     "ctxt\0"
L("cpu\0")     "\2"   L("us\0"   )FROM_PROC_STAT_CPU M_DPERCENT  "\x0d" /* user */
               "\2"   L("sy\0"   )FROM_PROC_STAT_CPU M_DPERCENT  "\x0b" /* system */
               "\2"   L("id\0"   )FROM_PROC_STAT_CPU M_DPERCENT  "\x04" /* idle */
               "\2"   L("wa\0"   )FROM_PROC_STAT_CPU M_DPERCENT  "\x05" /* iowait */
               "\2"   L("st\0"   )FROM_PROC_STAT_CPU M_DPERCENT  "\x08" /* steal */
// "gu"est columnt seems to be added in procps-ng 4.x.x (it makes the output not 80, but 83 chars):
               "\2"   L("gu\0"   )FROM_PROC_STAT_CPU M_DPERCENT  "\x0c" /* guest */
;
/* Packed row data from coldescs[] is decoded into this structure */
struct col {
	L(const char *grplabel;)
	L(const char *label;)
	const char *fromspec;
	unsigned char from;
	unsigned char width;
	unsigned char mod;
#define MOD_DELTA     0x01
#define MOD_PERCENT   0x02
#define MOD_DECREMENT 0x04
};

/* Number of columns defined in coldescs[] */
#define NCOLS (2+4+2+2+2+6)

/* Globals. Sort by size and access frequency. */
struct globals {
	unsigned data1[NCOLS];
	unsigned data2[NCOLS];
};
#define G (*(struct globals*)bb_common_bufsiz1)
#define INIT_G() do { \
	/* memset(&G, 0, sizeof G); */ \
} while (0)


//usage:#define vmstat_trivial_usage
//usage:       "[-n] [INTERVAL [COUNT]]"
//usage:#define vmstat_full_usage "\n\n"
//usage:       "Show virtual memory statistics\n"
//usage:     "\n	-n	Show header once"
// From 4.0.4 manpage:
// -a, --active         Display active and inactive memory, given a 2.5.41 kernel or better.
// -f, --forks          The  -f switch displays the number of forks since boot. This includes the fork, vfork, and clone system calls, and is equivalent to the total number of tasks created. This display does not repeat.
// -m, --slabs          Displays slabinfo.
// -s, --stats          Displays a table of various event counters and memory statistics. This display does not repeat.
// -d, --disk           Report disk statistics (2.5.70 or above required).
// -D, --disk-sum       Report some summary statistics about disk activity.
// -p, --partition device  Detailed statistics about partition (2.5.70 or above required).
// -S, --unit character Switches outputs between 1000 (k), 1024 (K), 1000000 (m), or 1048576 (M) bytes. Note this does not change the swap (si/so) or block (bi/bo) fields.
// -t, --timestamp      Append timestamp to each line
// -w, --wide           Wide output mode (useful for systems with higher amount of memory, where the default output mode suffers from unwanted column breakage). The output is wider than 80 characters per line.
// -y, --no-first       Omits first report with statistics since system boot.

/*
 * Advance an iterator over the coldescs[] packed descriptors.
 *   col_return - pointer to storage to hold the next unpacked descriptor.
 *   cp - pointer to iterator storage; should be initialised with coldescs.
 * Returns 1 when *cp has been advanced, and *col_return filled
 * (i.e. col_return->label will not be NULL).
 * Returns 0 when coldescs[] has been exhausted, and sets col_return->label
 * and col_return->grplabel to NULL.
 */
static bool next_col(struct col *col_return, const char **cpp)
{
	const char *cp = *cpp;
	L(col_return->grplabel = NULL;)
	if (*cp == '\0') {
		L(col_return->label = NULL;)
		return 0;
	}
#if !FIXED_HEADER
	if (*cp > ' ') {
		/* Only the first column of a group gets the grplabel */
		col_return->grplabel = cp;
		while (*cp++)
			continue;	/* Skip over the grplabel */
	}
#endif
	col_return->width = *cp++;
#if !FIXED_HEADER
	col_return->label = cp;
	while (*cp++)
		continue;	/* Skip over the label */
#endif
	col_return->from = *cp++;
	col_return->mod = 0;
	if (*cp & 0x80)
		col_return->mod = *cp++;
	col_return->fromspec = cp;
	while (*cp++ >= ' ')
		continue;	/* Skip over the fromspec */
	*cpp = cp;
	return 1;
}

/* Compares two fromspec strings for equality.
 * A fromspec can be a C string, or be terminated inclusively with
 * a byte 1..31. */
static bool fromspec_equal(const char *fs1, const char *fs2)
{
	while (*fs1 == *fs2) {
		if (*fs1 < ' ')
			return 1;
		fs1++;
		fs2++;
	}
	return 0;
}

/*
 * Finds a column in coldescs[] that has the the given from and fromspec.
 * Returns the index if found, otherwise -1.
 */
static int find_col(unsigned char from, const char *fromspec)
{
	const char *coli;
	struct col col;
	int i;

	for (i = 0, coli = coldescs; next_col(&col, &coli); i++) {
		if (col.from == from
		 && fromspec_equal(col.fromspec, fromspec)
		) {
			return i;
		}
	}
	return -1;
}

/*
 * Reads current system state into the data array elements corresponding
 * to coldescs[] columns.
 */
static void load_row(unsigned data[NCOLS])
{
	FILE *fp;
	char label[32];
	char line[256];
	int colnum;
	unsigned SwapFree = 0;
	unsigned SwapTotal = 0;
	unsigned Cached = 0;
	unsigned SReclaimable = 0;

	memset(data, 0, NCOLS * sizeof(data[0]));

	/*
	 * Open each FROM_* source and read all their items. These are
	 * generally labeled integer items. As we read each item, hunt
	 * through coldescs[] to see if a column uses it and, if one does,
	 * store the item's value in the corresponding data[] element.
	 */

	/* FROM_PROC_STAT and FROM_PROC_STAT_CPU */
	fp = xfopen_for_read("/proc/stat");
	while (fgets(line, sizeof(line), fp)) {
		enum stat_cpu {
			STAT_CPU_user = 1,
			STAT_CPU_nice = 2,
			STAT_CPU_system = 3,
			STAT_CPU_idle = 4,
			STAT_CPU_iowait = 5,
			STAT_CPU_irq = 6,
			STAT_CPU_softirq = 7,
			STAT_CPU_steal = 8,
			STAT_CPU_guest = 9,	  /* included in user */
			STAT_CPU_guest_nice = 10, /* included in nice */
			/* computed columns */
			STAT_CPU_pseudo_sy = 0xb, /* system + irq + softirq */
			STAT_CPU_pseudo_gu = 0xc, /* guest + guest_nice */
			STAT_CPU_pseudo_us = 0xd, /* user + nice - pseudo_gu */
			_STAT_CPU_max
		};
		unsigned num[_STAT_CPU_max];
		int n = sscanf(line,
			"%31s %u %u %u %u %u %u %u %u %u %u",
			label,
			&num[STAT_CPU_user],
# define _STAT_CPU_first STAT_CPU_user
			&num[STAT_CPU_nice],
			&num[STAT_CPU_system],
			&num[STAT_CPU_idle],
			&num[STAT_CPU_iowait],
			&num[STAT_CPU_irq],
			&num[STAT_CPU_softirq],
			&num[STAT_CPU_steal],
			&num[STAT_CPU_guest],
			&num[STAT_CPU_guest_nice]);
		if (n == 11 && strcmp(label, "cpu") == 0) {
			const char *coli;
			struct col col;
			int i;

			num[STAT_CPU_pseudo_sy] = num[STAT_CPU_system]
						+ num[STAT_CPU_irq]
						+ num[STAT_CPU_softirq]
						;
			num[STAT_CPU_pseudo_gu] = num[STAT_CPU_guest]
						+ num[STAT_CPU_guest_nice]
						;
			num[STAT_CPU_pseudo_us] = num[STAT_CPU_user]
						+ num[STAT_CPU_nice]
						- num[STAT_CPU_pseudo_gu]
						;
			for (i = 0, coli = coldescs; next_col(&col, &coli); i++)
				if (col.from == *FROM_PROC_STAT_CPU)
					data[i] = num[(int)*col.fromspec];
		}
		else if (n >= 2) {
			colnum = find_col(*FROM_PROC_STAT, label);
			if (colnum != -1)
				data[colnum] = num[_STAT_CPU_first];
		}
	}
	fclose(fp);

	/* FROM_PROC_VMSTAT */
	fp = xfopen_for_read("/proc/vmstat");
	while (fgets(line, sizeof(line), fp)) {
		unsigned num;

		if (sscanf(line, "%31s %u", label, &num) == 2) {
			colnum = find_col(*FROM_PROC_VMSTAT, label);
			if (colnum != -1)
				data[colnum] = num;
		}
	}
	fclose(fp);

	/* FROM_PROC_MEMINFO */
	fp = xfopen_for_read("/proc/meminfo");
//FIXME: unsigned is way too narrow (>4TB of RAM machines are not that rare these days)
	while (fgets(line, sizeof(line), fp)) {
		unsigned num;

		if (sscanf(line, "%31[^:]: %u", label, &num) == 2) {
			/* Store some values for computed (pseudo) values */
			if (strcmp(label, "SwapTotal") == 0)
				SwapTotal = num;
			else if (strcmp(label, "SwapFree") == 0)
				SwapFree = num;
			else if (strcmp(label, "Cached") == 0)
				Cached = num;
			else if (strcmp(label, "SReclaimable") == 0)
				SReclaimable = num;

			colnum = find_col(*FROM_PROC_MEMINFO, label);
			if (colnum != -1)
				data[colnum] = num;
		}
	}
	fclose(fp);

	/* "Pseudo" items computed from other items */
#if 0
	colnum = find_col(*FROM_PROC_MEMINFO, PSEUDO_SWPD);
	if (colnum != -1) {
		bb_error_msg("PSEUDO_SWPD %d", colnum);
		data[colnum] = SwapTotal - SwapFree;
	}
	colnum = find_col(*FROM_PROC_MEMINFO, PSEUDO_CACHE);
	if (colnum != -1) {
		bb_error_msg("PSEUDO_CACHE %d", colnum);
		data[colnum] = Cached + SReclaimable;
	}
#else // indexes are constant (modulo changes to the source code)
	data[2] = SwapTotal - SwapFree;
	data[5] = Cached + SReclaimable;
#endif
}

/* Modify, format and print a row of data values */
static void print_row(const unsigned data[NCOLS],
		      const unsigned old_data[NCOLS])
{
	const char *coli;
	struct col col;
	unsigned i;
	bool is_first;
	unsigned percent_sum = 0;

	for (coli = coldescs, i = 0; next_col(&col, &coli); i++)
		if (col.mod & MOD_PERCENT) {
			unsigned value = data[i];
			if (col.mod & MOD_DELTA) {
				value -= old_data[i];
//For some counters, kernel does not guarantee that they never go backwards.
//Either due to bugs, or sometimes it's just too hard to 100.00% guarantee that
//(imagine the horrors of reliably and locklessly collecting per-CPU counters).
//The idea is that _userspace_ can semi-trivially work around rare small backward steps:
				if ((int)value < 0) value = 0;
			}
			percent_sum += value;
		}

	is_first = 1;
	for (coli = coldescs, i = 0; next_col(&col, &coli); i++) {
		unsigned value = data[i];

		if (col.mod & MOD_DELTA) {
			value -= old_data[i];
//For some counters, kernel does not guarantee that they never go backwards
			if ((int)value < 0) value = 0;
		}
		if (col.mod & MOD_PERCENT) {
			value = percent_sum ? 100 * value / percent_sum : 0;
			if (value >= 100) /* sanity check + show "99" for 100% to not overflow col width */
				value = 99;
		}
		if ((col.mod & MOD_DECREMENT) && value)
			value--;
//FIXME: memory can easily overflow the columns:
// r  b   swpd   free   buff  cache   si   so    bi    bo   in   cs us sy id wa st gu
// 3  0      0 2019152 1029788 8602060    0    0    29   211 5699   13  9  5 86  0  0  0 //CURRENT CODE:BAD
// 3  0   0 2019152 1029788 8602060    0    0    29   211 5699   13  9  5 86  0  0  0 //SMARTER
//FIXME: cs (context switches) easily overflow too
		printf(" %*u" + is_first, col.width, value);
		is_first = 0;
	}
	bb_putchar('\n');
}

/* Print column header rows */
static void print_header(void)
{
#if FIXED_HEADER
	/* The header is constant yet and can be hardcoded instead,
	 * but adding options such as -wtd to match upstream will change that */
//TODO: remove grplabel/label from coldescs[], they are unused
	puts(
"procs -----------memory---------- ---swap-- -----io---- -system-- -------cpu-------""\n"
" r  b   swpd   free   buff  cache   si   so    bi    bo   in   cs us sy id wa st gu"
	);
#else
	const char *coli;
	struct col col;
	bool is_first;

	/* First header row is the group labels */
	coli = coldescs;
	next_col(&col, &coli);
	is_first = 1;
	while (col.label) {
		const char *grplabel = col.grplabel;
		unsigned gllen = strlen(grplabel);
		unsigned grpwidth = col.width;
		unsigned nhy = 0;

		/* Sum the column widths */
		next_col(&col, &coli);
		while (col.label && !col.grplabel) {
			grpwidth += 1 + col.width;
			next_col(&col, &coli);
		}

		if (is_first)
			is_first = 0;
		else
			bb_putchar(' ');

		/* Centre the grplabel within grpwidth hyphens. */
		while (gllen < grpwidth) {
			bb_putchar('-');
			grpwidth--;
			if (gllen < grpwidth)
				grpwidth--, nhy++;
		}
		while (*grplabel)
			bb_putchar(*grplabel++);
		while (nhy--)
			bb_putchar('-');
	}
	bb_putchar('\n');

	/* Second header row is right-justified column labels */
	coli = coldescs;
	is_first = 1;
	while (next_col(&col, &coli)) {
		printf(" %*s" + is_first, col.width, col.label);
		is_first = 0;
	}
	bb_putchar('\n');
#endif
}

int vmstat_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int vmstat_main(int argc UNUSED_PARAM, char **argv)
{
	int opt;
	unsigned interval = 0;
	int count = 1;
	unsigned height;
	unsigned rows;

	INIT_G();

	/* Parse and process arguments */
	opt = getopt32(argv, "n");
	argv += optind;
	if (*argv) {
		interval = xatoi_positive(*argv);
		count = (interval != 0 ? -1 : 1);
		argv++;
		if (*argv)
			count = xatoi_positive(*argv);
	}

	/* Prepare to re-print the header row after it scrolls off */
	height = 0;
	if (!(opt & OPT_n))
		get_terminal_width_height(STDOUT_FILENO, NULL, &height);

	/* Main loop */
	load_row(G.data1); /* prevents incorrect deltas in 1st sample */
	for (rows = 0;; rows++) {
		if (rows == 0 || (height > 5 && (rows % (height - 3)) == 0))
			print_header();

		/* Flip between using data1/2 and data2/1 for old/new */
		if (rows & 1) {
			load_row(G.data1);
			print_row(G.data1, G.data2);
		} else {
			load_row(G.data2);
			print_row(G.data2, G.data1);
		}

		if (count > 0 && --count == 0)
			break;

		sleep(interval);
	}

	return EXIT_SUCCESS;
}
