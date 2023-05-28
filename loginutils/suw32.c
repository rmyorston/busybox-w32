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
//usage:       "[-NW] [root]\n"
//usage:       "or:    su [-NW] -c CMD_STRING [[--] root [ARG0 [ARG...]]]\n"
//usage:       "or:    su [-NW] [--] root [arbitrary sh arguments]"
//usage:#define suw32_full_usage "\n\n"
//usage:       "Run shell with elevated privileges\n"
//usage:     "\n    -c CMD  Command to pass to 'sh -c'"
//usage:     "\n    -N      Don't close console when shell exits"
//usage:     "\n    -W      Wait for shell exit code"

#include "libbb.h"
#include "lazyload.h"

enum {
	OPT_c = (1 << 0),
	OPT_N = (1 << 1),
	OPT_W = (1 << 2)
};

int suw32_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int suw32_main(int argc UNUSED_PARAM, char **argv)
{
	unsigned opt;
	char *opt_command = NULL;
	SHELLEXECUTEINFO info;
	char *bb_path, *cwd, *q, *args;
	DECLARE_PROC_ADDR(BOOL, ShellExecuteExA, SHELLEXECUTEINFOA *);

	opt = getopt32(argv, "c:NW", &opt_command);
	argv += optind;
	if (argv[0]) {
		if (strcmp(argv[0], "root") != 0) {
			bb_error_msg_and_die("unknown user '%s'", argv[0]);
		}
		++argv;
	}

	/* ShellExecuteEx() needs backslash as separator in UNC paths. */
	bb_path = xstrdup(bb_busybox_exec_path);
	slash_to_bs(bb_path);

	memset(&info, 0, sizeof(SHELLEXECUTEINFO));
	info.cbSize = sizeof(SHELLEXECUTEINFO);
	/* info.fMask = SEE_MASK_DEFAULT; */
	if (opt & OPT_W)
		info.fMask |= SEE_MASK_NOCLOSEPROCESS;
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
	q = quote_arg(cwd);
	args = xasprintf("--busybox ash -d %s -t \"BusyBox ash (Admin)\"", q);
	free(q);
	free(cwd);

	if (opt & OPT_N)
		args = xappendword(args, "-N");

	if (opt_command) {
		args = xappendword(args, "-c");
		q = quote_arg(opt_command);
		args = xappendword(args, q);
		free(q);
	}

	while (*argv) {
		q = quote_arg(*argv++);
		args = xappendword(args, q);
		free(q);
	}

	info.lpParameters = args;
	/* info.lpDirectory = NULL; */
	info.nShow = SW_SHOWNORMAL;

	if (!INIT_PROC_ADDR(shell32.dll, ShellExecuteExA))
		return -1;

	if (!ShellExecuteExA(&info))
		return 1;

	if (opt & OPT_W) {
		DWORD r;

		WaitForSingleObject(info.hProcess, INFINITE);
		if (!GetExitCodeProcess(info.hProcess, &r))
			r = 1;
		CloseHandle(info.hProcess);
		return r;
	}

	return 0;
}
