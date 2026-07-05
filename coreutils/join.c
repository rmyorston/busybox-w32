/* vi: set sw=4 ts=4: */
/*
 * join -- equality join on two files
 *
 * Written by Morgan Bartlett
 * Copyright (C) 2025 Morgan Bartlett
 *
 * Licensed under GPLv2 or later, see file LICENSE in this source tree.
 */
//config:config JOIN
//config:	bool "join (6.6 kb)"
//config:	default n
//config:	help
//config:	Equality join on two files

//applet:IF_JOIN(APPLET_NOEXEC(join, join, BB_DIR_USR_BIN, BB_SUID_DROP, join))

//kbuild:lib-$(CONFIG_JOIN) += join.o

//usage:#define join_trivial_usage
//usage:       "[-a file_number | -v file_number] [-e empty] [-o list] [-t sep] [-1 field] [-2 field] file1 file2"
//usage:#define join_full_usage "\n\n"
//usage:       "Perform a join on file1 and file2, writing to stdout\n"
//usage:     "\n	-a FNUM	Also write unpaired lines from file FNUM"
//usage:     "\n	-v FNUM	Only write unpaired lines from file FNUM"
//usage:     "\n	-e STR	Replace empty fields with STR"
//usage:     "\n	-o LIST	Write the fields in LIST"
//usage:     "\n	-t SEP	Use a different field separator"
//usage:     "\n	-1 N	Join on field N from file1"
//usage:     "\n	-2 N	Join on field N from file2"
//usage:     "\n"
//usage:     "\nLIST is a space or comma separated list of FNUM.FIELD or 0 (the join field)"

#include "libbb.h"
#include "unicode.h"

/* This is a NOEXEC applet. Be very careful! */

/* Must match getopt32 call */
#define FLAG_UNPAIRED_ADD  1
#define FLAG_UNPAIRED_ONLY 2
#define FLAG_EMPTY_STRING  4
#define FLAG_LIST_OUTPUT   8
#define FLAG_SEP_USED     16
#define FLAG_FIELD_1      32
#define FLAG_FIELD_2      64

typedef struct {
	char *line;
	char **fields;
	int fieldcount;
} LINE;

typedef struct {
	FILE *fp;
	int idx;			/* index of join field in fields array */
	const char *field;	/* pointer to lines[0].fields[idx] or "" */
	LINE *lines;
	LINE pushback;
	int linecount;		/* number of lines currently cached */
	int linecap;		/* current capacity of lines array */
} FDAT;

/* split s by sep, and put the results into *curr */
static void field_split(char *s, char sep, LINE *curr)
{
	/* compare awk_split from editors/awk.c */
	int n;
	char *ps = s;
	char *s1 = s;

	char **sl;
	size_t sn;

	/* in worst case, every character is a separator */
	curr->fields = sl = xzalloc(sizeof(char*) * (strlen(s) + 2));
	curr->line = s;

	n = 0;
	if (sep != '\0') {  /* single-character split */
		while ((s1 = strchr(ps, sep)) != NULL) {
			sl[n] = ps;
			*s1 = '\0';
			ps = s1 + 1;
			n++;
		}
		/* Add the last field */
		sl[n] = ps;
		n++;
		curr->fieldcount = n;
	}
	/* default split: skip the initial whitespace and then any run
	   of non-whitespace characters is a field */
	while (*s) {
		/* blanks are space and tab in the POSIX locale */
		while (*s == ' ' || *s == '\t')
			s++;
		if (!*s)
			break;

		s1 = strpbrk(s, " \t");
		if (s1 == NULL) {
			/* last field */
			sn = strlen(s);
		} else {
			sn = s1 - s;
			*s1 = '\0';
		}

		sl[n] = s;
		n++;
		s += sn + 1;
	}
	curr->fieldcount = n;
}

static inline void freefields(LINE *lp)
{
	free(lp->line);
	free(lp->fields);
	lp->line = NULL;
	lp->fields = NULL;
}

static void freelines(FDAT *f)
{
	for (int n = 0; n < f->linecount; n ++) {
		freefields(f->lines + n);
	}
	f->linecount = 0;
	f->field = "";
}

static void readfields(char sep, FDAT *f)
{
	LINE curr = { .line = NULL, .fields = NULL, .fieldcount = 0 };
	char *line;
	const char *field2;
	bool first = TRUE;

	freelines(f);

	while (true) {
		if (first && f->pushback.fields) {
			/* first check for a pushed back line */
			curr = f->pushback;
			f->pushback = (LINE) { .line = NULL, .fields = NULL, .fieldcount = 0 };
		} else {
			/* read a line and split */
			line = xmalloc_fgetline(f->fp);
			if (line == NULL) {
				return;
			}

			field_split(line, sep, &curr);
		}

		if (f->idx >= curr.fieldcount)
			field2 = "";
		else
			field2 = curr.fields[f->idx];

		/* Ensure strcmp() matches on first pass through loop */
		if (first)
			f->field = field2;

		if (strcmp(f->field, field2) == 0) {
			/* add to the stack */
			if (f->linecount == f->linecap) {
				f->linecap = f->linecap ? f->linecap * 2 : 8;
				f->lines = xrealloc(f->lines, sizeof(LINE) * f->linecap);
			}
			f->lines[f->linecount++] = curr;
		} else {
			f->pushback = curr;
			return;
		}
		first = FALSE;
	}
}

static inline const char *fieldorempty(const char *field, const char *empty_str)
{
	if (*field == '\0')
		return empty_str;
	else
		return field;
}

static void printfields(int *format, const char *empty_str, char sep, FDAT *f1, FDAT *f2)
{
	const char *field = (f1 == NULL) ? f2->field : f1->field;

	int linef1;
	int linef2;
	int fn;
	int format_fl;
	int format_idx;
	int *formatcurr;
	bool first;

	LINE *l1;
	LINE *l2;

	if (sep == '\0')
		sep = ' ';

	for (linef1 = 0; linef1 < (f1 ? f1->linecount : 1); linef1 ++) {
		l1 = f1 ? &f1->lines[linef1] : NULL;
		for (linef2 = 0; linef2 < (f2 ? f2->linecount : 1); linef2 ++) {
			l2 = f2 ? &f2->lines[linef2] : NULL;

			if (format) {
				/*
				Format is a sort of null-terminated array:
				They are indexed by [n] for file number and [n + 1] for field number.
				If file number is 0 then that is the null-terminator.
				If file number is neither 1 nor 2 then we have the join field.
				*/
				first = true;
				formatcurr = format;
				while (formatcurr[0]) {
					if (first)
						first = false;
					else
						bb_putchar(sep);

					format_fl = formatcurr[0];
					format_idx = formatcurr[1];

					if (format_fl == 1) {
						if (l1 != NULL && l1->fieldcount > format_idx)
							fputs_stdout(fieldorempty(l1->fields[format_idx], empty_str));
						else
							fputs_stdout(empty_str);
					} else if (format_fl == 2) {
						if (l2 != NULL && l2->fieldcount > format_idx)
							fputs_stdout(fieldorempty(l2->fields[format_idx], empty_str));
						else
							fputs_stdout(empty_str);
					} else
						fputs_stdout(fieldorempty(field, empty_str));

					formatcurr += 2;
				}
			} else {
				fputs_stdout(fieldorempty(field, empty_str));

				fn = 0;
				if (l1 != NULL)
					while (l1->fields[fn]) {
						if (fn != f1->idx) {
							bb_putchar(sep);
							fputs_stdout(fieldorempty(l1->fields[fn], empty_str));
						}
						fn++;
					}

				fn = 0;
				if (l2 != NULL)
					while (l2->fields[fn]) {
						if (fn != f2->idx) {
							bb_putchar(sep);
							fputs_stdout(fieldorempty(l2->fields[fn], empty_str));
						}
						fn++;
					}
			}
			bb_putchar('\n');
		}
	}
}

static void parsejformat(int **format_p, const char *format_str)
{
	int *format;
	int field_idx;

	char scache[20] = { 0 };

	int n = 0;
	const char *s1 = format_str;

	size_t sn;

	/* The most formats we can have is if the format_str is noted as 0,0,0,0
	   which ends up being (strlen + 1) / 2
	   and then we need to have two for each entry plus one for the terminator */
	*format_p = format = xzalloc(sizeof(int) * ((strlen(format_str) + 1) / 2) * 2 + 1);

	/* default split: skip the initial whitespace and then any run
	   of non-whitespace characters is a field */
	while (*format_str) {
		/* blanks are space and tab in the POSIX locale */
		while (*format_str == ' ' || *format_str == '\t' || *format_str == ',')
			format_str++;
		if (!*format_str)
			break;

		s1 = strpbrk(format_str, " \t,");
		if (s1 == NULL)
			/* last field */
			sn = strlen(format_str);
		else
			sn = s1 - format_str;

		if (sn == 1 && *format_str == '0') {
			format[n * 2] = 3;
		} else if (sn >= 3 && (*format_str == '1' || *format_str == '2') && *(format_str + 1) == '.') {
			if (sn > 21) /* res_str > 19 */
				bb_simple_error_msg_and_die("field specifier too large");
			memcpy(scache, format_str + 2, sn - 2);
			scache[sn - 2] = '\0';
			field_idx = xatoi_positive(scache);
			if (--field_idx < 0)
				bb_simple_error_msg_and_die("field number can't be 0");
			format[n * 2] = *format_str - '0';
			format[n * 2 + 1] = field_idx;
		} else {
			bb_simple_error_msg_and_die("field specifier must be 0, 1.x or 2.x");
		}

		n++;
		format_str += sn;
	}
}

int join_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int join_main(int argc, char **argv)
{
	llist_t *unpaired_list = NULL;
	const char *empty_str = "";
	const char *format_str = NULL;
	char *separator;
	uint32_t opts;

	bool print1unpaired = false;
	bool print2unpaired = false;
	bool printpaired = true;

	int *format = NULL;

	FDAT f1 = {
		.fp = NULL,
		.idx = 0,
		.field = NULL,
		.lines = NULL,
		.pushback = { .line = NULL, .fields = NULL, .fieldcount = 0 },
		.linecount = 0,
		.linecap = 0,
	};
	FDAT f2 = {
		.fp = NULL,
		.idx = 0,
		.field = NULL,
		.lines = NULL,
		.pushback = { .line = NULL, .fields = NULL, .fieldcount = 0 },
		.linecount = 0,
		.linecap = 0,
	};

	char *unpaired_str;

	/* We can't use \0 as a real separator, so this stands in for the whitespace+ pattern */
	char sep = 0;

	init_unicode();

	opts = getopt32(argv,
		"^a:*v:*"
		"e:"
		"o:"
		"t:"
		"1:+2:+"
		"\0""=2:a--v:v--a",
		&unpaired_list,
		&unpaired_list,
		&empty_str,
		&format_str,
		&separator,
		&f1.idx,
		&f2.idx);
	argc -= optind; argv += optind;

	if ((opts & FLAG_UNPAIRED_ADD) || (opts & FLAG_UNPAIRED_ONLY)) {
		printpaired = !(opts & FLAG_UNPAIRED_ONLY);
		while ((unpaired_str = llist_pop(&unpaired_list)) != NULL) {
			if (*unpaired_str == '1')
				print1unpaired = true;
			else if (*unpaired_str == '2')
				print2unpaired = true;
			else if (unpaired_str[1] != '\0')
				bb_simple_error_msg_and_die("-a and -v take either 1 or 2");
		}
	}

	if (((opts & FLAG_FIELD_1) && --f1.idx < 0) ||
			((opts & FLAG_FIELD_2) && --f2.idx < 0))
		bb_simple_error_msg_and_die("field 0 doesn't exist");

	if (opts & FLAG_SEP_USED) {
		if (separator[1] != '\0')
			bb_simple_error_msg_and_die("separators are single characters");

		sep = *separator;
	}

	if (opts & FLAG_LIST_OUTPUT)
		parsejformat(&format, format_str);

	f1.fp = xfopen_stdin(argv[0]);
	f2.fp = xfopen_stdin(argv[1]);
	if (f1.fp == f2.fp)
		bb_simple_error_msg_and_die("can't combine stdin with itself");

	/*

	Outline of the program:

	- Read a line set from each file into cache

	- While there are cached current lines from both files:
		- If the two indexing fields are the same:
			- If no unpaired_only options:
				- Print joined line set formatted with fields
			- Read line sets from both files
		- Elseif indexing1 < indexing2 then
			- If print1unpaired:
				- Print line set from file 1
			- Read another line set from file 1
		- Elseif indexing1 > indexing2 then
			- If print2unpaired:
				- Print line set from file 2
			- Read another line set from file 2

	- If there are cached lines from file 1 AND print1unpaired:
		- Print the rest of the lines from file 1
	- If there are cached lines from file 2 AND print2unpaired:
		- Print the rest of the lines from file 2

	*/

	/*

	Notes for POSIX:

	https://pubs.opengroup.org/onlinepubs/9699919799/utilities/join.html

	Multiple instances of the same key are meant to give combinatorial results.

	E.g:
		file1:
			a 1
			a 2

		file2:
			a A
			a B
			a C

		result:
			a 1 A
			a 1 B
			a 1 C
			a 2 A
			a 2 B
			a 2 C

	This is done in this implementation by reading all the lines that have the same key
	and then iterating through the combinations when printing them.

	(test with `./busybox join <(printf 'a 1\na 2\n';) <(printf 'a A\na B\na C\n')`)

	*/

	readfields(sep, &f1);
	readfields(sep, &f2);

	while (f1.linecount && f2.linecount) {
		int res = strcmp(f1.field, f2.field);

		if (res == 0) {
			if (printpaired)
				printfields(format, empty_str, sep, &f1, &f2);

			readfields(sep, &f1);
			readfields(sep, &f2);
		} else if (res < 0) {
			if (print1unpaired)
				printfields(format, empty_str, sep, &f1, NULL);

			readfields(sep, &f1);
		} else {
			if (print2unpaired)
				printfields(format, empty_str, sep, NULL, &f2);

			readfields(sep, &f2);
		}
	}

	if (f1.linecount && print1unpaired) {
		do {
			printfields(format, empty_str, sep, &f1, NULL);
			readfields(sep, &f1);
		} while (f1.linecount);
	}

	if (f2.linecount && print2unpaired) {
		do {
			printfields(format, empty_str, sep, NULL, &f2);
			readfields(sep, &f2);
		} while (f2.linecount);
	}

#if ENABLE_FEATURE_CLEAN_UP
	if (f1.linecap) {
		freelines(&f1);
		free(f1.lines);
	}

	if (f2.linecap) {
		freelines(&f2);
		free(f2.lines);
	}

	free(format);

	fclose_if_not_stdin(f1.fp);
	fclose_if_not_stdin(f2.fp);
#endif

	fflush_stdout_and_exit_SUCCESS();
}
