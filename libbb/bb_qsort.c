/* vi: set sw=4 ts=4: */
/*
 * Wrapper for common string vector sorting operation
 *
 * Copyright (c) 2008 Denys Vlasenko
 *
 * Licensed under GPLv2, see file LICENSE in this source tree.
 */
#include "libbb.h"

int /* not FAST_FUNC! */ bb_pstrcmp(const void *a, const void *b)
{
	return strcmp(*(char**)a, *(char**)b);
}

void FAST_FUNC qsort_string_vector(char **sv, unsigned count)
{
	qsort(sv, count, sizeof(char*), bb_pstrcmp);
}

#if ENABLE_PLATFORM_MINGW32
static int bb_pstrcasecmp(const void *a, const void *b)
{
	return strcasecmp(*(char**)a, *(char**)b);
}

void FAST_FUNC qsort_string_vector_case(char **sv, unsigned count)
{
	qsort(sv, count, sizeof(char*), bb_pstrcasecmp);
}
#endif
