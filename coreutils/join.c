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
//config:	bool "join (3 kb)"
//config:	default y
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
	char **fields;
	int fieldcount;
} LINE;

typedef struct {
	FILE *fp;
	int idx;
	const char *field; /* this is a pointer to lines[0].fields[idx - 1] or "" */
	LINE *lines;
	LINE pushback;
	int linecount;
	int linecap;
} FDAT;

static int field_split(const char *s, char sep, char ***fields)
{
	/* compare awk_split from editors/awk.c */
	int n;
	const char *ps = s;
	const char *s1 = s;

	char **sl;
	size_t sn;

	/* in worst case, every character is a separator */
	*fields = sl = xzalloc(sizeof(char*) * (strlen(s) + 2));

	n = 0;
	if (sep != '\0') {  /* single-character split */
		while ((s1 = strchr(ps, sep)) != NULL) {
			size_t count = s1 - ps;
			sl[n] = xstrndup(ps, count);
			ps = s1 + 1;
			n++;
		}
		/* Add the last field */
		sl[n] = xstrdup(ps);
		n++;
		return n;
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
		if (s1 == NULL)
			/* last field */
			sn = strlen(s);
		else
			sn = s1 - s;

		sl[n] = xstrndup(s, s1 - s);
		n++;
		s += sn;
	}
	return n;
}

static inline void freefields(char **fields)
{
	char **fieldsiter = fields;
	while (*fieldsiter) {
		free(*fieldsiter);
		fieldsiter++;
	}
	free(fields);
}

static void readfields(char sep, FDAT *f)
{
	LINE curr = { .fields = NULL, .fieldcount = 0 };
	char *line;
	const char *field2;
	int n;

	for (n = 0; n < f->linecount; n ++) {
		if (f->lines[n].fields) {
			freefields(f->lines[n].fields);
			f->lines[n].fields = NULL;
		}
	}
	f->linecount = 0;
	f->field = "";

	/* read a line and split, checking the pushed back line */
	if (f->pushback.fields) {
		memcpy(&curr, &f->pushback, sizeof(LINE));
		f->pushback.fields = NULL;
		f->pushback.fieldcount = 0;
	} else {
		line = xmalloc_fgetline(f->fp);
		if (line == NULL) {
			return;
		}

		n = field_split(line, sep, &curr.fields);
		free(line);
		curr.fieldcount = n;
	}

	if (f->idx > curr.fieldcount)
		f->field = "";
	else
		f->field = (curr.fields)[f->idx - 1];

	if (f->linecount == f->linecap) {
		f->linecap = f->linecap ? f->linecap * 2 : 8;
		f->lines = xrealloc(f->lines, sizeof(LINE) * f->linecap);
	}
	memcpy(&f->lines[f->linecount], &curr, sizeof(LINE));
	f->linecount ++;

	while (true) {
		/* read a line and split */
		line = xmalloc_fgetline(f->fp);
		if (line == NULL) {
			return;
		}

		n = field_split(line, sep, &curr.fields);
		free(line);
		curr.fieldcount = n;

		if (f->idx > n)
			field2 = "";
		else
			field2 = (curr.fields)[f->idx - 1];

		if (strcmp(f->field, field2) == 0) {
			/* add to the stack */
			if (f->linecount == f->linecap) {
				f->linecap = f->linecap ? 8 : f->linecap * 2;
				f->lines = xrealloc(f->lines, sizeof(LINE) * f->linecap);
			}
			memcpy(&f->lines[f->linecount], &curr, sizeof(LINE));
			f->linecount ++;
		} else {
			memcpy(&f->pushback, &curr, sizeof(LINE));
			return;
		}
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
	int format_no;
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
				while (format[0]) {
					if (first)
						first = false;
					else
						bb_putchar(sep);

					format_fl = format[0];
					format_no = format[1];

					if (format_fl == 1) {
						if (l1 != NULL && l1->fieldcount >= format_no && format_no > 0)
							fputs_stdout(fieldorempty(l1->fields[format_no - 1], empty_str));
						else
							fputs_stdout(empty_str);
					} else if (format_fl == 2) {
						if (l2 != NULL && l2->fieldcount >= format_no && format_no > 0)
							fputs_stdout(fieldorempty(l2->fields[format_no - 1], empty_str));
						else
							fputs_stdout(empty_str);
					} else
						fputs_stdout(fieldorempty(field, empty_str));

					format += 2;
				}
			} else {
				fputs_stdout(fieldorempty(field, empty_str));

				fn = 1;
				if (l1 != NULL)
					while (l1->fields[fn - 1]) {
						if (fn != f1->idx) {
							bb_putchar(sep);
							fputs_stdout(fieldorempty(l1->fields[fn - 1], empty_str));
						}
						fn++;
					}

				fn = 1;
				if (l2 != NULL)
					while (l2->fields[fn - 1]) {
						if (fn != f2->idx) {
							bb_putchar(sep);
							fputs_stdout(fieldorempty(l2->fields[fn - 1], empty_str));
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
	int field_no;

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
			field_no = xatoi_positive(scache);
			if (field_no <= 0)
				bb_simple_error_msg_and_die("field number can't be 0");
			format[n * 2] = *format_str - '0';
			format[n * 2 + 1] = field_no;
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
	exitcode_t exitcode = EXIT_SUCCESS;

	llist_t *unpaired_add = NULL;
	llist_t *unpaired_only = NULL;
	const char *empty_str = "";
	const char *format_str = NULL;
	char *separator = NULL;
	uint32_t opts;

	bool print1unpaired = false;
	bool print2unpaired = false;
	bool printpaired = true;

	const char *filename1;
	const char *filename2;

	int *format = NULL;

	FDAT f1 = {
		.fp = NULL,
		.idx = 1,
		.field = NULL,
		.lines = NULL,
		.pushback = { .fields = NULL, .fieldcount = 0 },
		.linecount = 0,
		.linecap = 0,
	};
	FDAT f2 = {
		.fp = NULL,
		.idx = 1,
		.field = NULL,
		.lines = NULL,
		.pushback = { .fields = NULL, .fieldcount = 0 },
		.linecount = 0,
		.linecap = 0,
	};

	char *unpaired_str;

	/* We can't use \0 as a real separator, so this stands in for the whitespace+ pattern */
	char sep = 0;
	int i;

	init_unicode();

	opts = getopt32(argv,
		"^a:*v:*"
		"e:"
		"o:"
		"t:"
		"1:+2:+"
		"\0""=2",
		&unpaired_add,
		&unpaired_only,
		&empty_str,
		&format_str,
		&separator,
		&f1.idx,
		&f2.idx);
	argc -= optind; argv += optind;

	if ((opts & FLAG_UNPAIRED_ADD) || (opts & FLAG_UNPAIRED_ONLY)) {
		if ((opts & FLAG_UNPAIRED_ADD) && (opts & FLAG_UNPAIRED_ONLY))
			bb_simple_error_msg_and_die("-a and -v are exclusive");

		printpaired = !(opts & FLAG_UNPAIRED_ONLY);

		while ((unpaired_str =
				llist_pop((opts & FLAG_UNPAIRED_ONLY) ? &unpaired_only : &unpaired_add)) != NULL) {
			if (*unpaired_str == '\0' || unpaired_str[1] != '\0')
				bb_simple_error_msg_and_die("-a and -v take either 1 or 2");
			else if (*unpaired_str == '1')
				print1unpaired = true;
			else if (*unpaired_str == '2')
				print2unpaired = true;
			else
				bb_simple_error_msg_and_die("-a and -v take either 1 or 2");
		}
		unpaired_str = NULL;
	}

	if (f1.idx < 1 || f2.idx < 1)
		bb_simple_error_msg_and_die("field 0 doesn't exist");

	if (opts & FLAG_SEP_USED) {
		if (strlen(separator) != 1)
			bb_simple_error_msg_and_die("separators are single characters");

		sep = *separator;
	}

	if (opts & FLAG_LIST_OUTPUT)
		parsejformat(&format, format_str);

	filename1 = argv[0];
	filename2 = argv[1];

	f1.fp = xfopen_stdin(filename1);
	f2.fp = xfopen_stdin(filename2);
	if (f1.fp == f2.fp)
		bb_simple_error_msg_and_die("can't combine stdin with itself");

	/*

	Outline of the program:

	- Read a line from each file into cache

	- While there are cached current lines from both files:
		- If the two indexing fields are the same:
			- If no unpaired_only options:
				- Print joined line formatted with fields
			- Read lines from both files
		- Elseif indexing1 < indexing2 then
			- If print1unpaired:
				- Print line from file 1
			- Read another line from file 1
		- Elseif indexing1 > indexing2 then
			- If print2unpaired:
				- Print line from file 2
			- Read another line from file 2

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

	if (f1.linecap) {
		for (i = 0; i < f1.linecount; i ++) {
			if (f1.lines[i].fields)
				freefields(f1.lines[i].fields);
		}
		free(f1.lines);
	}

	if (f2.linecap) {
		for (i = 0; i < f2.linecount; i ++) {
			if (f2.lines[i].fields)
				freefields(f2.lines[i].fields);
		}
		free(f2.lines);
	}


	if (format)
		free(format);

	fclose_if_not_stdin(f1.fp);
	fclose_if_not_stdin(f2.fp);

	fflush_stdout_and_exit(exitcode);
}
