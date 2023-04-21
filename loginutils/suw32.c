/* vi: set sw=4 ts=4: */
/*
 * Mini su implementation for busybox-w32
 *
 * Licensed under GPLv2 or later, see file LICENSE in this source tree.
 */
//config:config SUW32
//config:	bool "su for Microsoft Windows"
//config:	default y
//config:	depends on PLATFORM_MINGW32 && ASH
//config:	help
//config:	su runs a shell with elevated privileges.

//applet:IF_SUW32(APPLET_ODDNAME(su, suw32, BB_DIR_BIN, BB_SUID_DROP, suw32))

//kbuild:lib-$(CONFIG_SUW32) += suw32.o

//usage:#define suw32_trivial_usage
//usage:       "[-c \"CMD\" [ARG...]]"
//usage:#define suw32_full_usage "\n\n"
//usage:       "Run shell with elevated privileges\n"
//usage:     "\n    -c CMD [ARG...]  Command [args] to pass to 'sh -c'"

#include "libbb.h"
#include "lazyload.h"

// Windows (single) argument quoting
// MS docs:
//   https://learn.microsoft.com/en-us/cpp/c-language/parsing-c-command-line-arguments
// TL;DR (DQ: double-quote, BS: backslash):
// - Enclosing in DQ is always allowed.
// - N consecutive BS are taken literally, unless followed by DQ, at which case
//   they become N/2 literal BS, and, if N is odd - the DQ becomes literal too.
// - ^ is literal, and, inside DQ: presumably also all other non DQ/BS.
static char *xwinq(const char *arg)
{
	char *r = xmalloc(2 * strlen(arg) + 3);  // max-esc, enclosing DQ, \0
	char *o = r;
	int nbs = 0;  // n consecutive BS right before current char

	for (*o++ = '"'; *arg; *o++ = *arg++) {
		switch (*arg) {
		case '\\':
			++nbs;
			break;
		case '"':  // double consecutive-BS, plus one to escape the DQ
			for (++nbs; nbs; --nbs)
				*o++ = '\\';
			break;
		default:  // reset count if followed by not-DQ
			nbs = 0;
		}
	}

	while (nbs--)  // double consecutive-BS before the closing DQ
		*o++ = '\\';
	*o++ = '"';
	*o++ = 0;
	return xrealloc(r, o - r);
}

static char *xappend(char *a, const char *b)
{
	int alen = a ? strlen(a) : 0, blen = strlen(b);
	a = xrealloc(a, alen + blen + 1);
	memcpy(a + alen, b, blen + 1);
	return a;
}

int suw32_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int suw32_main(int argc UNUSED_PARAM, char **argv)
{
	SHELLEXECUTEINFO info;
	unsigned opts, c_opt;
	char *bb_path, *cwd;
	DECLARE_PROC_ADDR(BOOL, ShellExecuteExA, SHELLEXECUTEINFOA *);

	opts = getopt32(argv, "c");
	c_opt = opts & 1;
	if (!c_opt != !argv[optind])
		bb_show_usage();  // -c witout CMD, or operand without -c

	/* ShellExecuteEx() needs backslash as separator in UNC paths. */
	bb_path = xstrdup(bb_busybox_exec_path);
	slash_to_bs(bb_path);

	memset(&info, 0, sizeof(SHELLEXECUTEINFO));
	info.cbSize = sizeof(SHELLEXECUTEINFO);
	/* info.fMask = SEE_MASK_DEFAULT; */
	/* info.hwnd = NULL; */
	info.lpVerb = "runas";
	info.lpFile = bb_path;
	/*
	 * It seems that when ShellExecuteEx() runs binaries residing in
	 * certain 'system' directories it sets the current directory of
	 * the process to %SYSTEMROOT%\System32.  Override this by passing
	 * the directory we want to the shell.
	 *
	 * Canonicalise the directory now:  if it's in a drive mapped to
	 * a network share it may not be available once we have elevated
	 * privileges.
	 */
	cwd = xmalloc_realpath(getcwd(NULL, 0));
	info.lpParameters =
		xasprintf("--busybox ash -d \"%s\" -t \"BusyBox ash (Admin)\" ", cwd);

	if (c_opt) {
		info.lpParameters = xappend((char *)info.lpParameters, "-s -c --");
		while (argv[optind]) {
			char *a = xwinq(argv[optind++]);
			info.lpParameters = xappend((char *)info.lpParameters, " ");
			info.lpParameters = xappend((char *)info.lpParameters, a);
			free(a);
		}
	}

	/* info.lpDirectory = NULL; */
	info.nShow = SW_SHOWNORMAL;

	if (!INIT_PROC_ADDR(shell32.dll, ShellExecuteExA))
		return -1;

	return !ShellExecuteExA(&info);
}
