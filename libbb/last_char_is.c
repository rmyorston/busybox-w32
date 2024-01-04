/* vi: set sw=4 ts=4: */
/*
 * busybox library eXtended function
 *
 * Copyright (C) 2001 Larry Doolittle, <ldoolitt@recycle.lbl.gov>
 *
 * Licensed under GPLv2 or later, see file LICENSE in this source tree.
 */
#include "libbb.h"

/* Find out if the last character of a string matches the one given */
char* FAST_FUNC last_char_is(const char *s, int c)
{
	if (!s[0])
		return NULL;
	while (s[1])
		s++;
	return (*s == (char)c) ? (char *) s : NULL;
}

#if ENABLE_PLATFORM_MINGW32
char* FAST_FUNC last_char_is_dir_sep(const char *s)
{
	if (!s[0])
		return NULL;
	while (s[1])
		s++;
	return is_dir_sep(*s)? (char *) s : NULL;
}
#endif
