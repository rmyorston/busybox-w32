/* vi: set sw=4 ts=4: */
/*
 * cut.c - minimalist version of cut
 *
 * Copyright (C) 1999,2000,2001 by Lineo, inc.
 * Written by Mark Whitley <markw@codepoet.org>
 * debloated by Bernhard Reutner-Fischer
 *
 * Licensed under GPLv2 or later, see file LICENSE in this source tree.
 */
//config:config CUT
//config:	bool "cut (6.7 kb)"
//config:	default y
//config:	help
//config:	cut is used to print selected parts of lines from
//config:	each file to stdout.
//config:
//config:config FEATURE_CUT_REGEX
//config:	bool "cut -F"
//config:	default y
//config:	depends on CUT
//config:	help
//config:	Allow regex based delimiters.

//applet:IF_CUT(APPLET_NOEXEC(cut, cut, BB_DIR_USR_BIN, BB_SUID_DROP, cut))

//kbuild:lib-$(CONFIG_CUT) += cut.o

//usage:#define cut_trivial_usage
//usage:       "[OPTIONS] [FILE]..."
//usage:#define cut_full_usage "\n\n"
//usage:       "Print selected fields from FILEs to stdout\n"
//usage:     "\n	-b LIST	Output only bytes from LIST"
//usage:     "\n	-c LIST	Output only characters from LIST"
//usage:     "\n	-d SEP	Field delimiter for input (default -f TAB, -F run of whitespace)"
//usage:     "\n	-s	Drop lines with no delimiter"
//usage:     "\n	-O SEP	Field delimeter for output (default = -d for -f, one space for -F)"
//TODO: --output-delimiter=SEP
//usage:     "\n	-f LIST	Print only these fields (-d is single char)"
//usage:     IF_FEATURE_CUT_REGEX(
//usage:     "\n	-F LIST	Print only these fields (-d is regex)"
//usage:     )
//usage:     "\n	-D	Don't sort/collate sections or match -fF lines without delimeter"
//usage:     "\n	-n	Ignored"
//(manpage:-n	with -b: don't split multibyte characters)
//usage:
//usage:#define cut_example_usage
//usage:       "$ echo \"Hello world\" | cut -f 1 -d ' '\n"
//usage:       "Hello\n"
//usage:       "$ echo \"Hello world\" | cut -f 2 -d ' '\n"
//usage:       "world\n"

#include "libbb.h"

#if ENABLE_FEATURE_CUT_REGEX
#include "xregex.h"
#endif

/* This is a NOEXEC applet. Be very careful! */


/* option vars */
#define OPT_STR "b:c:f:d:O:sD"IF_FEATURE_CUT_REGEX("F:")"n"
#define OPT_BYTE     (1 << 0)
#define OPT_CHAR     (1 << 1)
#define OPT_FIELDS   (1 << 2)
#define OPT_DELIM    (1 << 3)
#define OPT_ODELIM   (1 << 4)
#define OPT_SUPPRESS (1 << 5)
#define OPT_NOSORT   (1 << 6)
#define OPT_REGEX    ((1 << 7) * ENABLE_FEATURE_CUT_REGEX)

#define opt_REGEX (option_mask32 & OPT_REGEX)

struct cut_list {
	int startpos;
	int endpos;
};

static int cmpfunc(const void *a, const void *b)
{
	return (((struct cut_list *) a)->startpos -
			((struct cut_list *) b)->startpos);
}

static void cut_file(FILE *file, const char *delim, const char *odelim,
		const struct cut_list *cut_list, unsigned nlists)
{
	char *line;
	unsigned linenum = 0;	/* keep these zero-based to be consistent */

	/* go through every line in the file */
	while ((line = xmalloc_fgetline(file)) != NULL) {

		/* set up a list so we can keep track of what's been printed */
		int linelen = strlen(line);
		unsigned cl_pos = 0;

		/* cut based on chars/bytes XXX: only works when sizeof(char) == byte */
		if (option_mask32 & (OPT_CHAR | OPT_BYTE)) {
			char *printed = xzalloc(linelen + 1);

			/* print the chars specified in each cut list */
			for (; cl_pos < nlists; cl_pos++) {
				int spos;
				for (spos = cut_list[cl_pos].startpos; spos < linelen;) {
					if (!printed[spos]) {
						printed[spos] = 'X';
						putchar(line[spos]);
					}
					if (++spos > cut_list[cl_pos].endpos) {
						break;
					}
				}
			}
			free(printed);
		/* Cut by lines */
		} else if (!opt_REGEX && *delim == '\n') {
			int spos = cut_list[cl_pos].startpos;

			/* get out if we have no more lists to process or if the lines
			 * are lower than what we're interested in */
			if (((int)linenum < spos) || (cl_pos >= nlists))
				goto next_line;

			/* if the line we're looking for is lower than the one we were
			 * passed, it means we displayed it already, so move on */
			while (spos < (int)linenum) {
				spos++;
				/* go to the next list if we're at the end of this one */
				if (spos > cut_list[cl_pos].endpos) {
					cl_pos++;
					/* get out if there's no more lists to process */
					if (cl_pos >= nlists)
						goto next_line;
					spos = cut_list[cl_pos].startpos;
					/* get out if the current line is lower than the one
					 * we just became interested in */
					if ((int)linenum < spos)
						goto next_line;
				}
			}

			/* If we made it here, it means we've found the line we're
			 * looking for, so print it */
			puts(line);
			goto next_line;
		/* Cut by fields */
		} else {
			unsigned next = 0, start = 0, end = 0;
			int dcount = 0; /* we saw Nth delimiter (0 - didn't see any yet) */
			int first_print = 1;

			/* Blank line? Check -s (later check for -s does not catch empty lines) */
			if (linelen == 0) {
				if (option_mask32 & OPT_SUPPRESS)
					goto next_line;
			}

			/* Loop through bytes, finding next delimiter */
			for (;;) {
				/* End of current range? */
				if (end == linelen || dcount > cut_list[cl_pos].endpos) {
 end_of_range:
					if (++cl_pos >= nlists)
						break;
					if (option_mask32 & OPT_NOSORT)
						start = dcount = next = 0;
					end = 0; /* (why?) */
					//bb_error_msg("End of current range");
				}
				/* End of current line? */
				if (next == linelen) {
					end = linelen; /* print up to end */
					/* If we've seen no delimiters, check -s */
					if (cl_pos == 0 && dcount == 0 && !opt_REGEX) {
						if (option_mask32 & OPT_SUPPRESS)
							goto next_line;
						/* else: will print entire line */
					} else if (dcount < cut_list[cl_pos].startpos) {
						/* echo 1.2 | cut -d. -f1,3: prints "1", not "1." */
						//break;
						/* ^^^ this fails a case with -D:
						 * echo 1 2 | cut -DF 1,3,2:
						 * do not end line processing when didn't find field#3
						 */
						//if (option_mask32 & OPT_NOSORT) - no, just do it always
						goto end_of_range;
					}
					//bb_error_msg("End of current line: s:%d e:%d", start, end);
				} else {
					/* Find next delimiter */
#if ENABLE_FEATURE_CUT_REGEX
					if (opt_REGEX) {
						regmatch_t rr = {-1, -1};
						regex_t *reg = (void*) delim;

						if (regexec(reg, line + next, 1, &rr, REG_NOTBOL|REG_NOTEOL) != 0) {
							/* not found, go to "end of line" logic */
							next = linelen;
							continue;
						}
						end = next + rr.rm_so;
						next += rr.rm_eo;
					} else
#endif
					{
						end = next++;
						if (line[end] != *delim)
							continue;
					}
					/* Got delimiter */
					if (dcount++ < cut_list[cl_pos].startpos) {
						/* Not yet within range - loop */
						start = next;
						continue;
					}
				}
				if (end != start || !opt_REGEX) {
					if (first_print) {
						first_print = 0;
						printf("%.*s", end - start, line + start);
					} else
						printf("%s%.*s", odelim, end - start, line + start);
				}
				start = next;
				//if (dcount == 0)
				//	break; - why?
			} /* byte loop */
		}
		/* if we printed anything, finish with newline */
		putchar('\n');
 next_line:
		linenum++;
		free(line);
	} /* while (got line) */
}

int cut_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int cut_main(int argc UNUSED_PARAM, char **argv)
{
	/* growable array holding a series of lists */
	struct cut_list *cut_list = NULL;
	unsigned nlists = 0;	/* number of elements in above list */
	char *sopt, *ltok;
	const char *delim = NULL;
	const char *odelim = NULL;
	unsigned opt;
#if ENABLE_FEATURE_CUT_REGEX
	regex_t reg;
#endif

#define ARG "bcf"IF_FEATURE_CUT_REGEX("F")
	opt = getopt32(argv, "^"
			OPT_STR  // = "b:c:f:d:O:sD"IF_FEATURE_CUT_REGEX("F:")"n"
			"\0" "b--"ARG":c--"ARG":f--"ARG IF_FEATURE_CUT_REGEX("F--"ARG),
			&sopt, &sopt, &sopt, &delim, &odelim IF_FEATURE_CUT_REGEX(, &sopt)
	);
	if (!delim || !*delim)
		delim = (opt & OPT_REGEX) ? "[[:space:]]+" : "\t";
	if (!odelim)
		odelim = (opt & OPT_REGEX) ? " " : delim;

//	argc -= optind;
	argv += optind;
	if (!(opt & (OPT_BYTE | OPT_CHAR | OPT_FIELDS | OPT_REGEX)))
		bb_simple_error_msg_and_die("expected a list of bytes, characters, or fields");

	/*  non-field (char or byte) cutting has some special handling */
	if (!(opt & (OPT_FIELDS|OPT_REGEX))) {
		static const char _op_on_field[] ALIGN1 = " only when operating on fields";

		if (opt & OPT_SUPPRESS) {
			bb_error_msg_and_die
				("suppressing non-delimited lines makes sense%s", _op_on_field);
		}
		if (opt & OPT_DELIM) {
			bb_error_msg_and_die
				("a delimiter may be specified%s", _op_on_field);
		}
	}

	/*
	 * parse list and put values into startpos and endpos.
	 * valid list formats: N, N-, N-M, -M
	 * more than one list can be separated by commas
	 */
	{
		char *ntok;
		int s = 0, e = 0;

		/* take apart the lists, one by one (they are separated with commas) */
		while ((ltok = strsep(&sopt, ",")) != NULL) {

			/* it's actually legal to pass an empty list */
			if (!ltok[0])
				continue;

			/* get the start pos */
			ntok = strsep(&ltok, "-");
			if (!ntok[0]) {
				s = 0;
			} else {
				/* account for the fact that arrays are zero based, while
				 * the user expects the first char on the line to be char #1 */
				s = xatoi_positive(ntok) - 1;
			}

			/* get the end pos */
			if (ltok == NULL) {
				e = s;
			} else if (!ltok[0]) {
				/* if the user specified no end position,
				 * that means "til the end of the line" */
				e = INT_MAX;
			} else {
				/* again, arrays are zero based, lines are 1 based */
				e = xatoi_positive(ltok) - 1;
			}

			if (s < 0 || e < s)
				bb_error_msg_and_die("invalid range %s-%s", ntok, ltok ?: ntok);

			/* add the new list */
			cut_list = xrealloc_vector(cut_list, 4, nlists);
			/* NB: startpos is always >= 0 */
			cut_list[nlists].startpos = s;
			cut_list[nlists].endpos = e;
			nlists++;
		}

		/* make sure we got some cut positions out of all that */
		if (nlists == 0)
			bb_simple_error_msg_and_die("missing list of positions");

		/* now that the lists are parsed, we need to sort them to make life
		 * easier on us when it comes time to print the chars / fields / lines
		 */
		if (!(opt & OPT_NOSORT))
			qsort(cut_list, nlists, sizeof(cut_list[0]), cmpfunc);
	}

#if ENABLE_FEATURE_CUT_REGEX
	if (opt & OPT_REGEX) {
		xregcomp(&reg, delim, REG_EXTENDED);
		delim = (void*) &reg;
	}
#endif

	{
		exitcode_t retval = EXIT_SUCCESS;

		if (!*argv)
			*--argv = (char *)"-";

		do {
			FILE *file = fopen_or_warn_stdin(*argv);
			if (!file) {
				retval = EXIT_FAILURE;
				continue;
			}
			cut_file(file, delim, odelim, cut_list, nlists);
			fclose_if_not_stdin(file);
		} while (*++argv);

		if (ENABLE_FEATURE_CLEAN_UP)
			free(cut_list);
		fflush_stdout_and_exit(retval);
	}
}
