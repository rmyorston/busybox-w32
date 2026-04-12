/*
Copyright (C) 2026 Avi Halachmi (:avih)  avihpit@yahoo.com

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, see https://www.gnu.org/licenses/ .
*/

#include <ctype.h>
#include <string.h>

/* don't include libbb.h which redefines isalpha etc - we use the platform's */
#include "actype.h"

/* no generic busybox size/speed optimization option, so piggyback ash */
#ifndef ACTYPE_OPTIMIZE_FOR_SIZE
  #if defined(ASH_OPTIMIZE_FOR_SIZE) && ASH_OPTIMIZE_FOR_SIZE
    #define ACTYPE_OPTIMIZE_FOR_SIZE 1
  #else
    #define ACTYPE_OPTIMIZE_FOR_SIZE 0
  #endif
#endif

#if ACTYPE_OPTIMIZE_FOR_SIZE
  char *is_prefixed_with(const char*, const char*);  /* libbb */
  #define IS_PREFIXED_WITH_CLASS is_prefixed_with
#else
  /* c[0]-c[4] are not 0, so we can skip these 0-tests, and do it inline.
   * in x64 this adds ~ 40 bytes, and actype/actail are almost x2 faster.
   */
  #define IS_PREFIXED_WITH_CLASS(s, c) \
        (s[0]==c[0] && s[1]==c[1] && s[2]==c[2] && s[3]==c[3] && s[4]==c[4] \
         ? c[5] ? /* xdigit */ s[5] == 't' ? s+6 : 0 \
                : s+5 \
         : 0)
#endif


/* isblank is c99 and not always available. just provide our own. */
#define isblank ac_isblank
static int ac_isblank(int c)
{
	return c == ' ' || c == '\t';
}


int isactype(int c, actype_t t)
{
	return t ? isactype_not0(c, t) : 0;
}


/* perfect hash table from the 3-chars prefix of string s to the exact
 * single potential class name to test (using strcmp etc).
 * the hash function H(...) can use all 3 chars, but currently it only
 * uses s[1] and s[2]. It was chosen to work mod32. it might be possible
 * to find a simple function which works mod16, and cut idx size to 16.
 * (#define HSIZE 16 and play with H - clang warns on collisions).
 *
 * this hash is only guaranteed for ASCII a-x values. see below for non-ascii.
 */
#define HSIZE 32  /* power of 2 hash buckets */
#define H(s0,s1,s2) (((unsigned)(s1) - (unsigned)(s2)) & (HSIZE - 1))

static const char idx[HSIZE] = {
	[H('a','l','n')] = 0,  [H('a','l','p')] = 1,  [H('b','l','a')] = 2,
	[H('c','n','t')] = 3,  [H('d','i','g')] = 4,  [H('g','r','a')] = 5,
	[H('l','o','w')] = 6,  [H('p','r','i')] = 7,  [H('p','u','n')] = 8,
	[H('s','p','a')] = 9,  [H('u','p','p')] = 10, [H('x','d','i')] = 11
};

/* the hash uses s[2] to distinguish alnum/alpha, but can't read past '\0' */
#define POTENTIAL_CLASS_IDX(s) (s[0] && s[1] ? idx[H(s[0], s[1], s[2])] : 0)


/* with enum, isactype_not0 access this table - can't be static */
#if !ACTYPE_ENUM
  static
#endif
const _isactype_t _actype_fns[] = {
        0,
        isalnum, isalpha, isblank, iscntrl, isdigit, isgraph,
        islower, isprint, ispunct, isspace, isupper, isxdigit
};

#if ACTYPE_ENUM
  #define ACTYPE_FROM_IDX(i) (1 + (i))
#else
  #define ACTYPE_FROM_IDX(i) ((uintptr_t)_actype_fns[1 + (i)])
#endif

#define CHAR_CLASSES \
        "alnum\0alpha\0blank\0cntrl\0digit\0graph\0" \
        "lower\0print\0punct\0space\0upper\0xdigit"

actype_t actail(const char *s, int *len)
{
	int i = POTENTIAL_CLASS_IDX(s);
	const char *c = CHAR_CLASSES + i * 6;

	s = IS_PREFIXED_WITH_CLASS(s, c);
	if (!s)
		return 0;

	/* done: s is just past the matched name, verify tail based on len */
	if (len) {
		if (*s++ != ':' || *s != ']')
			return 0;
		*len = 7 + (i == 11);  /* xdigit is 6, others are 5 */
	} else if (*s) {
		return 0;
	}
	return ACTYPE_FROM_IDX(i);
}


/* for arbitrary monotonic (up) charsets, we could use non-prefect "hash" of
 * simply the s[0] value, so that a... and p... need one or two comparisons:
 *   #define HSIZE ('x'-'a'+1)  // (last-first+1)
 *   #define H(s0) ((unsigned)(s0) % HSIZE)
 * and the values "1" and "8" (alpha, punct) are removed as collisions:
 *   idx[HSIZE] = { [H('a')]=0, [H('b')]=2..., [H('p')]=7, [H('s')]=9,... }
 * and the code should loop on i as long as s[0] == c[0], which is
 * 1 or 2 iterations for a... and p..., and 0 or 1 iteration otherwise.
 * such solution also doesn't need the initial s[0] and s[1] test:
 *   for (i=idx[H(*s)], c=...+i*6; *s==*c; ++i, c+=6)
 *     if ((s=is_prefixed_with(s+1,c+1))) { <found - finalize> }
 *   return 0;
 */
