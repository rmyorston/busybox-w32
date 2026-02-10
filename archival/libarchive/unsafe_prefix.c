/* vi: set sw=4 ts=4: */
/*
 * Licensed under GPLv2 or later, see file LICENSE in this source tree.
 */
#include "libbb.h"
#include "bb_archive.h"

const char* FAST_FUNC skip_unsafe_prefix(const char *str)
{
	const char *cp = str;
	while (1) {
		const char *cp2;
		if (*cp == '/') {
			cp++;
			continue;
		}
		/* We are called lots of times.
		 * is_prefixed_with(cp, "../") is slower than open-coding it,
		 * with minimal code growth (~few bytes).
		 */
		if (cp[0] == '.' && cp[1] == '.' && cp[2] == '/') {
			cp += 3;
			continue;
		}
		cp2 = cp;
 find_dotdot:
		cp2 = strstr(cp2, "/..");
		if (!cp2)
			break; /* No (more) malicious components */

		/* We found "/..something" */
		cp2 += 3;
		if (*cp2 != '/') {
			if (*cp2 == '\0') {
				/* Trailing "/..": malicious, return "" */
				/* (causes harmless errors trying to create or hardlink a file named "") */
				return cp2;
			}
			/* "/..name" is not malicious, look for next "/.." */
			goto find_dotdot;
		}
		/* Found "/../": malicious, advance past it */
		cp = cp2 + 1;
	}
	if (cp != str) {
		static smallint warned = 0;
		if (!warned) {
			warned = 1;
			bb_error_msg("removing leading '%.*s' from member names",
				(int)(cp - str), str);
		}
	}
	return cp;
}

void FAST_FUNC strip_unsafe_prefix(char *str)
{
	overlapping_strcpy(str, skip_unsafe_prefix(str));
}
