/*
 * fnmatch implementation in linear time for the C/POSIX locale.
 * Unsopported (not identified): collating symbols, equivalence class.
 * Copyright (C) 2026 Avi Halachmi (:avih)  avihpit@yahoo.com
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see https://www.gnu.org/licenses/ .
 *
 *            ---------------------------------------------
 *
 * Other than collating symbols and equivalence class expressions, this
 * implementation should be POSIX.2024 compliant for the C/POSIX locale.
 *
 * Implementation choices:
 * - Supports the gnu-extension flag FNM_LEADING_DIR (see flags below).
 * - Without FNM_NOESCAPE, unescaped trailing '\' in pattern is FNM_NOMATCH.
 * - Without FNM_NOESCAPE, '\' escapes also inside bracket expression.
 * - In bracket expression, leading '^' is negation - identical to leading '!'.
 * - In bracket expression, [A-B-C] is interpreted as if it was [A-BB-C] .
 * - In bracket expression, Unknown char-class is not-special, e.g. [[:foo:]]
 *   is interpreted as valid bracket expr. [[:foo:] followed by literal ']'
 *   (the application is required to use valid name - invalid is unspecified).
 * - Bracket expression is considered invalid (making the '[' not-special) if:
 *   - It's unterminated - missing later [unescaped] ']'.
 *   - It has a negative range, e.g. [b-a] (but [a-a] is allowed).
 *   - It has a range which starts/ends with char-class, e.g. [A-[:lower:]] .
 *
 * This implementation is non-recursive and linear time: O(len(pat)*len(str)).
 * When '*' is encountered, it effectively "locks" in place the current state
 * of prior '*' choices (if any), so every '*' matches the shortest sequence
 * in str which allows reaching the next '*' (if any) in pat. As a result,
 * we have single "backtrack state" (of last '*'), rather than state per '*'.
 *
 * This is the same concept as in musl-libc and go-lang, explained by Russ Cox:
 *   https://research.swtch.com/glob
 *
 *
 * Flags:
 *
 * FNM_NOESCAPE     Backslash doesn't quote the next char.
 * FNM_PATHNAME     '/' in str is matched only by '/' in pat.
 * FNM_PERIOD       Leading '.' in str is matched only by leading '.' in pat.
 *                  With FNM_PATHNAME: leading is also after '/'.
 * FNM_CASEFOLD     Case-insensitive (if STR is matched without FNM_CASEFOLD,
 *                  then STR with any upper/lower replaced with other-case
 *                  will be matched with FNM_CASEFOLD set).
 * FNM_IGNORECASE   Same as FNM_CASEFOLD (at fnmatch.h - we don't test it).
 * FNM_FILE_NAME    Same as FNM_PATHNAME (at fnmatch.h, gnu extension).
 * FNM_LEADING_DIR  Ignore "/..." in str after a match (gnu extension).
 */

/* enable/disable the whole file.
 * only one of win32/fnmatch.c and win32/fnmatch2.c should be enabled.
 */
#if 1

#include <ctype.h>  /* to{lower,upper} */

#include "actype.h"
#include "fnmatch.h"

typedef unsigned char uchar;


#define FLAG(x) (flags & FNM_ ## x)

/* assumes bot <= top */
#define INRANGE(c, bot, top) \
	((unsigned)(c) - (unsigned)(bot) <= (unsigned)((top) - (bot)))

/* evaluates to non-0 actype_t if p is known "[:CLASS:]...", else 0.
 * if not-0, *plen is set to the length of "CLASS:]".
 */
#define CCLASS2(p, plen) \
	((p)[0] == '[' && (p)[1] == ':' \
	 ? actail((const char*)(p)+2, plen) \
	 : 0)

/* returns true if and only if the bracket-expression starting at *pat ("[...")
 * matches testchar. In either case, *pat is updated to the last processed
 * byte in pattern - typically the final ']'. If the pattern is invalid,
 * i.e. due to missing final ']' or negative range or range with char-class,
 * then the opening '[' is considered non-special and the return value
 * reflects that, i.e. return testchar=='[', and *pat is unmodified ("[...").
 */
static int bracket_matched(const char **pat, char testchar, const int flags)
{
	const uchar *p = (const uchar*)(*pat) + 1;
	int c = (uchar)testchar, cfold = c;
	int inv = 0, len;
	int found = 0;
	actype_t t;

	if (FLAG(CASEFOLD)) {
		cfold = toupper(c);
		if (c == cfold)
			cfold = tolower(c);
	}

	if (*p == '!' || *p == '^') {
		inv = 1;
		++p;
	}

	if (*p == ']' || *p == '-') {
		if (c == *p)
			found = 1;
		++p;
	}

	for (; *p; ++p) {
		if (*p == ']') {
			*pat = (const char*)p;
			return inv ^ found;
		}

		if (*p == '-' && p[1] != ']') {  /* range, or invalid */
			/* we should test *p != 0 after (both) ++p, but both
			 * are covered by the final r0>*p (r0 is never 0) */
			int r0 = p[-1];
			++p;
			if (CCLASS2(p, &len))
				break;  /* ...-[:NAME:] */
			if (*p == '\\' && !FLAG(NOESCAPE))
				++p;
			if (r0 > *p)  /* negative or unterminated */
				break;

			if (!found)
				if (INRANGE(c, r0, *p) || INRANGE(cfold, r0, *p))
					found = 1;

		} else if ((t = CCLASS2(p, &len))) {  /* [:NAME:] */
			p += len + 1;  /* ']' (len is of "NAME:]") */
			if (p[1] == '-' && p[2] != ']')
				break;  /* [:NAME:]-... */

			if (!found)
				if (isactype_not0(c, t) || isactype_not0(cfold, t))
					found = 1;

		} else {  /* [escaped] literal */
			if (*p == '\\' && !FLAG(NOESCAPE) && !*++p)
				break;

			if (c == *p || cfold == *p)
				found = 1;
		}
	}

	/* invalid: pat is unmodified, only compare with the literal '[' */
	return c == '[';
}


#define FOLD_EQ(a, b) \
	(a == b || \
	 (FLAG(CASEFOLD) && (tolower((uchar)(a)) == tolower((uchar)(b)))))

/* tests whether *s cannot be matched by meta-pattern (* or ? or [...])
 * PERIOD without PATHNAME applies only to str[0] - which we exclude on init
 */
#define BAD_META(s) \
	(FLAG(PATHNAME) && (*s == '/' || \
	                    (FLAG(PERIOD) && *s == '.' && s[-1] == '/')))


int fnmatch(const char *pat, const char *str, int flags)
{
	/* backtracking state. s_head==0 means no prior '*' to backtrack-to */
	const char *p_star = 0, *s_head = 0;

	if (FLAG(PERIOD) && *str == '.' && *pat != '.')
		return FNM_NOMATCH;
	/* BAD_META is guaranteed to not test str[-1] at str[0] */

	for (;; ++pat, ++str) {
		if (*pat == '*') {  /* skip, setup retry (max once per '*') */
			while (*++pat == '*');  /* all consecutive '*' */

			p_star = pat - 1;
			s_head = str;  /* on failure -> p_star eats s_head */
		}

		if (!*pat) {
			if (!*str || (FLAG(LEADING_DIR) && *str == '/'))
				return 0;  /* success */

		} else if (!*str) {
			/* failed - *pat is not \0 and not '*' */

		} else if (*pat == '?') {
			if (!BAD_META(str))
				continue;

		} else if (*pat == '[') {
			if (!BAD_META(str) && bracket_matched(&pat, *str, flags))
				continue;

		} else {
			if (*pat == '\\' && !FLAG(NOESCAPE) && !*++pat)
				return FNM_NOMATCH;  /* trailing '\' */
			if (FOLD_EQ(*pat, *str))
				continue;
		}

		/* failed: retry if we had '*' and p_star can match s_head */
		if (!s_head || !*s_head || BAD_META(s_head))
			return FNM_NOMATCH;

		pat = p_star;
		str = s_head++;  /* pat/str increased by the loop */
	}
}

#endif  /* whole file */
