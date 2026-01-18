/* vi: set sw=4 ts=4: */
/*
 * Mini cmp implementation for busybox
 *
 * Copyright (C) 2000,2001 by Matt Kraai <kraai@alumni.carnegiemellon.edu>
 *
 * Licensed under GPLv2 or later, see file LICENSE in this source tree.
 */
//config:config CMP
//config:	bool "cmp (5.3 kb)"
//config:	default y
//config:	help
//config:	cmp is used to compare two files and returns the result
//config:	to standard output.

//applet:IF_CMP(APPLET(cmp, BB_DIR_USR_BIN, BB_SUID_DROP))

//kbuild:lib-$(CONFIG_CMP) += cmp.o

//usage:#define cmp_trivial_usage
//usage:       "[-l|s] [-n NUM] FILE1 [FILE2" IF_DESKTOP(" [SKIP1 [SKIP2]]") "]"
//usage:#define cmp_full_usage "\n\n"
//usage:       "Compare FILE1 with FILE2 (or stdin)\n"
//usage:     "\n	-l	Show decimal offset and octal byte value for differing bytes,"
//usage:     "\n		don't stop on first mismatch"
//usage:     "\n	-s	Quiet"
//usage:     "\n	-n NUM	Compare at most NUM bytes"
//TODO?
// -b, --print-bytes              print differing bytes
//Prints the first difference:
//FILE1 FILE2 differ: byte 5, line 2 is <OCTAL1> <char1> <OCTAL2> <char2>
//                                  ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^ -b adds this
//and exits with exitcode 1.
// -i, --ignore-initial=SKIP1[:SKIP2]
//Same as SKIP1 [SKIP2] in argv, but here SKIP2 defaults to SKIP1, not to zero.

/* BB_AUDIT SUSv3 (virtually) compliant -- uses nicer GNU format for -l. */
/* http://www.opengroup.org/onlinepubs/007904975/utilities/cmp.html */

#include "libbb.h"

static const char fmt_eof[] ALIGN1 = "cmp: EOF on %s\n";
static const char fmt_differ[] ALIGN1 = "%s %s differ: byte %llu, line %u\n";
// This fmt_l_opt is gnu-ism. SUSv3 is "%.0s%.0s%llu %o %o\n"
static const char fmt_l_opt[] ALIGN1 = "%.0s%.0s%llu %3o %3o\n";

#define OPT_STR "sln:+"
#define CMP_OPT_s (1<<0)
#define CMP_OPT_l (1<<1)
#define CMP_OPT_n (1<<2)

int cmp_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int cmp_main(int argc UNUSED_PARAM, char **argv)
{
	FILE *fp1, *fp2, *outfile = stdout;
	const char *filename1, *filename2 = "-";
	unsigned long long skip1 = 0, skip2 = 0, char_pos = 0;
	int line_pos = 1; /* Hopefully won't overflow... */
	const char *fmt;
	int c1, c2;
	unsigned opt;
	int retval = 0;
	int max_count = -1;

#if !ENABLE_LONG_OPTS
	opt = getopt32(argv, "^"
			OPT_STR
			"\0" "-1"
			IF_DESKTOP(":?4")
			IF_NOT_DESKTOP(":?2")
			":l--s:s--l",
//TODO: -n MAXCOUNT should allow KMG suffixes
			&max_count
	);
#else
	static const char cmp_longopts[] ALIGN1 =
		"bytes\0"          Required_argument  "n"
		"quiet\0"          No_argument        "s"
		"silent\0"         No_argument        "s"
		"verbose\0"        No_argument        "l"
		;
	opt = getopt32long(argv, "^"
			OPT_STR
			"\0" "-1"
			IF_DESKTOP(":?4")
			IF_NOT_DESKTOP(":?2")
			":l--s:s--l",
			cmp_longopts,
			&max_count
	);
#endif
	argv += optind;

	filename1 = *argv;
	if (*++argv) {
		filename2 = *argv;
		if (ENABLE_DESKTOP && *++argv) {
			skip1 = xatoull_sfx(*argv, kmg_i_suffixes);
			if (*++argv) {
				skip2 = xatoull_sfx(*argv, kmg_i_suffixes);
			}
		}
	}

	xfunc_error_retval = 2;  /* missing file results in exitcode 2 */
	if (opt & CMP_OPT_s)
		logmode = 0;  /* -s suppresses open error messages */
	fp1 = xfopen_stdin(filename1);
	fp2 = xfopen_stdin(filename2);
	if (fp1 == fp2) {		/* Paranoia check... stdin == stdin? */
		/* Note that we don't bother reading stdin.  Neither does gnu wc.
		 * But perhaps we should, so that other apps down the chain don't
		 * get the input.  Consider 'echo hello | (cmp - - && cat -)'.
		 */
		return 0;
	}
	logmode = LOGMODE_STDIO;

	if (opt & CMP_OPT_l)
		fmt = fmt_l_opt;
	else
		fmt = fmt_differ;

	if (ENABLE_DESKTOP) {
		while (skip1) { if (getc(fp1) == EOF) break; skip1--; }
		while (skip2) { if (getc(fp2) == EOF) break; skip2--; }
	}
	do {
		if (max_count >= 0 && --max_count < 0)
			break;
		c1 = getc(fp1);
		c2 = getc(fp2);
		++char_pos;
		if (c1 != c2) {			/* Remember: a read error may have occurred. */
			retval = 1;		/* But assume the files are different for now. */
			if (c2 == EOF) {
				/* We know that fp1 isn't at EOF or in an error state.  But to
				 * save space below, things are setup to expect an EOF in fp1
				 * if an EOF occurred.  So, swap things around.
				 */
				fp1 = fp2;
				filename1 = filename2;
				c1 = c2;
			}
			if (c1 == EOF) {
				die_if_ferror(fp1, filename1);
				fmt = fmt_eof;	/* Well, no error, so it must really be EOF. */
				outfile = stderr;
				/* There may have been output to stdout (option -l), so
				 * make sure we fflush before writing to stderr. */
				fflush_all();
			}
			if (!(opt & CMP_OPT_s)) {
				if (opt & CMP_OPT_l) {
					line_pos = c1;	/* line_pos is unused in the -l case. */
				}
				fprintf(outfile, fmt, filename1, filename2, char_pos, line_pos, c2);
				if (opt & CMP_OPT_l) {
					/* -l: do not stop on first mismatch.
					 * If we encountered an EOF,
					 * the while check will catch it. */
					continue;
				}
			}
			break;
		}
		if (c1 == '\n') {
			++line_pos;
		}
	} while (c1 != EOF);

	die_if_ferror(fp1, filename1);
	die_if_ferror(fp2, filename2);

	fflush_stdout_and_exit(retval);
}
