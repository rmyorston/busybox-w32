/* vi: set sw=4 ts=4: */
/*
 * Utility routines.
 *
 * Copyright (C) 2006 Gabriel Somlo <somlo at cmu.edu>
 *
 * Licensed under GPLv2 or later, see file LICENSE in this source tree.
 */

#include "libbb.h"

/* check if path points to an executable file;
 * return 1 if found;
 * return 0 otherwise;
 */
int FAST_FUNC execable_file(const char *name)
{
	struct stat s;
	return (!access(name, X_OK) && !stat(name, &s) && S_ISREG(s.st_mode));
}

/* search (*PATHp) for an executable file;
 * return allocated string containing full path if found;
 *  PATHp points to the component after the one where it was found
 *  (or NULL),
 *  you may call find_execable again with this PATHp to continue
 *  (if it's not NULL).
 * return NULL otherwise; (PATHp is undefined)
 * in all cases (*PATHp) contents will be trashed (s/:/NUL/).
 */
#if !ENABLE_PLATFORM_MINGW32
#define next_path_sep(s) strchr(s, ':')
#endif

char* FAST_FUNC find_execable(const char *filename, char **PATHp)
{
	char *p, *n;

	p = *PATHp;
	while (p) {
		n = (char*)next_path_sep(p);
		if (n)
			*n++ = '\0';
		if (*p != '\0') { /* it's not a PATH="foo::bar" situation */
			p = concat_path_file(p, filename);
			if (execable_file(p)) {
				*PATHp = n;
				return p;
			}
			if (ENABLE_PLATFORM_MINGW32) {
				int len = strlen(p);
				if (len > 4 &&
				    (!strcasecmp(p+len-4, ".exe") ||
				     !strcasecmp(p+len-4, ".com")))
					; /* nothing, already tested by execable_file() */
				else {
					char *np = xmalloc(len+4+1);
					memcpy(np, p, len);
					memcpy(np+len, ".exe", 5);
					if (execable_file(np)) {
						*PATHp = n;
						free(p);
						return np;
					}
					memcpy(np+len, ".com", 5);
					if (execable_file(np)) {
						*PATHp = n;
						free(p);
						return np;
					}
				}
			}
			free(p);
		}
		p = n;
	} /* on loop exit p == NULL */
	return p;
}

/* search $PATH for an executable file;
 * return 1 if found;
 * return 0 otherwise;
 */
int FAST_FUNC exists_execable(const char *filename)
{
	char *path = xstrdup(getenv("PATH"));
	char *tmp = path;
	char *ret = find_execable(filename, &tmp);
	free(path);
	if (ret) {
		free(ret);
		return 1;
	}
	return 0;
}

#if ENABLE_FEATURE_PREFER_APPLETS
/* just like the real execvp, but try to launch an applet named 'file' first */
int FAST_FUNC BB_EXECVP(const char *file, char *const argv[])
{
	if (find_applet_by_name(file) >= 0)
		execvp(bb_busybox_exec_path, argv);
	return execvp(file, argv);
}
#endif

int FAST_FUNC BB_EXECVP_or_die(char **argv)
{
	BB_EXECVP(argv[0], argv);
	/* SUSv3-mandated exit codes */
	xfunc_error_retval = (errno == ENOENT) ? 127 : 126;
	bb_perror_msg_and_die("can't execute '%s'", argv[0]);
}
