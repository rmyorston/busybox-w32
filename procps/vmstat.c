/* vi: set sw=4 ts=4: */
/*
 * Report virtual memory statistics.
 *
 * Copyright (C) 2025 David Leonard
 *
 * Licensed under GPLv2, see file LICENSE in this source tree.
 */
//config:config VMSTAT
//config:	bool "vmstat (2 kb)"
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
//TODO? "MemAvailable" in newer kernels may be a more useful indicator how much memory is "free"
//in the sense of being immediately availabe if allocation demand arises (e.g. by dropping cached filesystem data)
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
/* Number of columns defined in coldescs[] */
#define NCOLS (2+4+2+2+2+6)
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

/* There is no "support huge memory" option, use "large file support" as proxy */
#if ENABLE_LFS
typedef unsigned long long data_t;
# define SPU " %llu"
# define U   "llu"
#else
/* this mishandles memory sizes if >4TB of RAM */
typedef unsigned data_t;
# define SPU " %u"
# define U   "u"
#endif
#define DEBUG_SHOW_5T_MEMFREE 0

struct globals {
	data_t data[NCOLS];
	data_t prev[NCOLS];
	unsigned interval;
};
#define G (*(struct globals*)bb_common_bufsiz1)
#define INIT_G() do { \
	memset(&G, 0, sizeof G); \
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
// "since system boot" is false: 4.0.4 has code to read initial counts before calculating deltas, which prevents showing data "since system boot" which is often wrapped-around anyway (bogus)

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
static void load_row(data_t data[NCOLS])
{
	FILE *fp;
	char label[32];
	char line[256];
	int colnum;
	data_t SwapFree = 0;
	data_t SwapTotal = 0;
	data_t Cached = 0;
	data_t SReclaimable = 0;

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
		data_t num[_STAT_CPU_max];
		int n = sscanf(line,
			"%31s"SPU SPU SPU SPU SPU SPU SPU SPU SPU SPU,
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
		data_t num;

		if (sscanf(line, "%31s"SPU, label, &num) == 2) {
			colnum = find_col(*FROM_PROC_VMSTAT, label);
			if (colnum != -1)
				data[colnum] = num;
		}
	}
	fclose(fp);

	/* FROM_PROC_MEMINFO */
	fp = xfopen_for_read("/proc/meminfo");
	while (fgets(line, sizeof(line), fp)) {
		data_t num;
#if DEBUG_SHOW_5T_MEMFREE
		if (is_prefixed_with(line, "MemFree:"))
			strcpy(line, "MemFree:      4444444444 kB\n"); /* overflows 32-bit unsigned */
#endif
		if (sscanf(line, "%31[^:]:"SPU, label, &num) == 2) {
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
static void print_row(const data_t data[NCOLS],
		      const data_t old_data[NCOLS])
{
	const char *coli;
	struct col col;
	unsigned i;
	bool is_first;
	int overrun;
	/* Note: all DELTA counters are truncated to 32 bits here.
	 * Imagine small embedded arch. Kernel does not _need_
	 * to maintain 64-bit cpu stats, they work perfectly fine
	 * when they overflow: userspace is expected to only use diffs anyway.
	 * Therefore we must not assume they are wider than 32 bits
	 * (doing so would cause bugs at the moment a counter overflows).
	 */
	uint32_t percent_sum = 0;
	uint32_t delta32[NCOLS];
	unsigned vshift;

	vshift = 0;
	for (coli = coldescs, i = 0; next_col(&col, &coli); i++) {
		if (col.mod & MOD_DELTA) {
			uint32_t value = data[i];
			value -= old_data[i];
//For some counters, kernel does not guarantee that they never go backwards.
//Either due to bugs, or sometimes it's just too hard to 100.00% guarantee it
//(imagine the horrors of reliably and locklessly collecting per-CPU counters).
//The idea is that _userspace_ can semi-trivially work around rare small backward steps:
			if ((int32_t)value < 0)
				value = 0;
			delta32[i] = value;
			if (col.mod & MOD_PERCENT) {
				/* Ensure deltas < 2^32 / 128 (0x2000000) and percent_sum can't overflow */
//If we don't do this, if running "vmstat MANY_SECS", percent_sum is ~MANY_SECS*1000*NUM_CPUS,
//on 100-cpu machine, at ~400 second-long vmstat periods, counters may be > 0x2000000 here.
				while ((value >> vshift) >= 0x2000000) {
					percent_sum >>= 1;
					vshift++;
				}
				percent_sum += (value >> vshift);
			}
		}
	}
//debug: reduce 0x2000000 to 0x2000 and try "vmstat 60" or so:
//bb_error_msg("percent_sum:%u vshift:%u", (unsigned)percent_sum, vshift);

	overrun = 0;
	is_first = 1;
	for (coli = coldescs, i = 0; next_col(&col, &coli); i++) {
		data_t value = (col.mod & MOD_DELTA) ? delta32[i] : data[i];

		/* the !0 check is needed in MOD_DECREMENT, might as well do it sooner */
		if (value != 0) {
			if ((col.mod & MOD_PERCENT) && percent_sum != 0) {
				value = 100 * ((uint32_t)value >> vshift) / percent_sum;
				/* narrow 32x32->32 multiply is ok since (value >> vshift) is < 2^32 / 128 */
				if (value >= 100) /* sanity check + show "99" for 100% to not overflow col width */
					value = 99;
			} /* else: if percent_sum = 0, all values are 0 too, no need to calculate % */

			if (col.mod & MOD_DECREMENT)
				value--;
		}

		if (!is_first)
			bb_putchar(' ');
		is_first = 0;
		/* memory can easily overflow the columns:
		 * r  b   swpd   free   buff  cache   si   so    bi    bo   in   cs us sy id wa st gu
		 * 3  0      0 2019152 1029788 8602060 0    0    29   211 5699   13  9  5 86  0  0  0
		 *                     ^-------^-------^-attempt to shorten width here
		 * To improve this, shorten the next field width if previous field overflowed.
		 */
//TODO: more improvements:
//Shift left entire set of memory fields if it has space padding and overruns at the right,
//as in the example above (zero bytes in swap: can replace "   0" with "0" to make data fit).
		{
			int chars_printed, width;
			width = (col.width - overrun >= 0 ? col.width - overrun : 0);
			/* For large intervals such as "vmstat 60" (one minute), or
			 * for cases with a lot of block I/O (try 'grep -rF noT_eXisting /usr'),
			 * can use smart_ulltoa4() to format "si", "so", "in" and "cs";
			 * smart_ulltoa5() to format "bi" and "bo".
			 * This way, we can show arbitrarily large accumulated counts for these. Example:
			 *  r  b   swpd   free   buff  cache   si   so    bi    bo   in   cs us sy id wa st gu
			 *  1  0      0 947636  76548 15373248  0    0  657k 23284 2.3m 4.1m 24  9 65  0  0  0
			 * Downside: incompatible output format. Do we need to make this conditional on an option?
			 */
			if ((col.width>>1) == 4/2) {
				char buf45[6];
				/* si/so/bi/bo/in/cs display rate per second (checked on 4.0.4): */
				if (G.interval != 0)
					value /= G.interval;
				if (col.width == 4)
					smart_ulltoa4(value, buf45, " kmgtpezy")[0] = '\0';
				else
					smart_ulltoa5(value, buf45, " kmgtpezy")[0] = '\0';
				chars_printed = printf("%*s", width, skip_whitespace(buf45));
			} else {
				chars_printed = printf("%*"U, width, value);
			}
			overrun += (chars_printed - col.width);
		}
	}

	bb_putchar('\n');
}

/* Print column header rows */
static void print_header(void)
{
#if FIXED_HEADER
	/* The header is constant yet and can be hardcoded instead,
	 * but adding options such as -wtd to match upstream will change that */
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
	int count;
	unsigned height;
	int row;

	INIT_G();

	/* Parse and process arguments */
	opt = getopt32(argv, "n");
	argv += optind;

	/*G.interval = 0;*/
	count = 1;
	if (*argv) {
		G.interval = xatoi_positive(*argv); /* 4.0.4 requires nonzero, we don't care */
		count = 0; /* "infinity" */
		argv++;
		if (*argv) {
			count = xatoi_positive(*argv);
			/* 4.0.4 (unless -y) treats explicit count of zero as if it's 1 */
			count += (count == 0);
		}
	}

	/* Prepare to re-print the header row after it scrolls off */
	height = 0;
	if (!(opt & OPT_n)) {
		get_terminal_width_height(STDOUT_FILENO, NULL, &height);
		/* match 4.0.4 behavior: re-print header if terminal has 4 or more lines */
		height -= 3; /* can become zero or negative */
	}

	load_row(G.prev); /* prevents incorrect deltas in 1st sample */
	row = 0;
	/* Main loop */
	for (;;) {
		if (row == 0) {
			print_header();
			row = (int)height;
		}
		row--;
		//if (row < 0) /* height <= 0: -n, or 3 or fewer lines in terminal */
		//	row = -1; /* do not count down, never become zero */
		/* equivalent to above: */
		row |= (row >> (sizeof(row)*8 - 1));

		load_row(G.data);
		print_row(G.data, G.prev);

		count--;
		count |= (count >> (sizeof(count)*8 - 1));
		if (count == 0) /* count was >0 and become zero? */
			break;

		memcpy(G.prev, G.data, sizeof(G.prev));
		sleep(G.interval);
	}

	return EXIT_SUCCESS;
}
