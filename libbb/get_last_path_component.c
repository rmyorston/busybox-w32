/* vi: set sw=4 ts=4: */
/*
 * bb_get_last_path_component implementation for busybox
 *
 * Copyright (C) 2001  Manuel Novoa III  <mjn3@codepoet.org>
 *
 * Licensed under GPLv2 or later, see file LICENSE in this source tree.
 */
#include "libbb.h"

const char* FAST_FUNC bb_basename(const char *name)
{
#if ENABLE_PLATFORM_MINGW32
	const char *cp;
	for (cp = name; *cp; cp++)
		if (*cp == '/' || *cp == '\\' || *cp == ':')
			name = cp + 1;
#else
	const char *cp = strrchr(name, '/');
	if (cp)
		return cp + 1;
#endif
	return name;
}

#if ENABLE_PLATFORM_MINGW32
char *get_last_slash(const char *path)
{
	const char *start = path + root_len(path);
	char *slash = strrchr(start, '/');
	char *bslash = strrchr(start, '\\');

	if (slash && bslash)
		slash = MAX(slash, bslash);
	else if (!slash)
		slash = bslash;
	return slash;
}
#endif

/*
 * "/"        -> "/"
 * "abc"      -> "abc"
 * "abc/def"  -> "def"
 * "abc/def/" -> ""
 */
char* FAST_FUNC bb_get_last_path_component_nostrip(const char *path)
{
#if ENABLE_PLATFORM_MINGW32
	const char *start = path + root_len(path);
	char *slash = get_last_slash(path);

	if (!slash && has_dos_drive_prefix(path) && path[2] != '\0')
		return (char *)path + 2;
	if (!slash || (slash == start && !slash[1]))
		return (char*)path;
#else
	char *slash = strrchr(path, '/');

	if (!slash || (slash == path && !slash[1]))
		return (char*)path;
#endif

	return slash + 1;
}

/*
 * "/"        -> "/"
 * "abc"      -> "abc"
 * "abc/def"  -> "def"
 * "abc/def/" -> "def" !!
 */
char* FAST_FUNC bb_get_last_path_component_strip(char *path)
{
#if ENABLE_PLATFORM_MINGW32
	char *slash = last_char_is_dir_sep(path);
	const char *start = has_dos_drive_prefix(path) ? path+2 : path;

	if (slash)
		while (is_dir_sep(*slash) && slash != start)
			*slash-- = '\0';
#else
	char *slash = last_char_is(path, '/');

	if (slash)
		while (*slash == '/' && slash != path)
			*slash-- = '\0';
#endif

	return bb_get_last_path_component_nostrip(path);
}
