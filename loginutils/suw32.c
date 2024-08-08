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
//usage:       "[-tW] [-N|-s SHELL] [root]\n"
//usage:       "or:    su [-tW] [-N|-s SHELL] -c CMD_STRING [[--] root [ARG0 [ARG...]]]\n"
//usage:       "or:    su [-tW] [-N|-s SHELL] [--] root [arbitrary sh arguments]"
//usage:#define suw32_full_usage "\n\n"
//usage:       "Run shell with elevated privileges\n"
//usage:     "\n    -c CMD    Command to pass to 'sh -c'"
//usage:     "\n    -s SHELL  Use specified shell"
//usage:     "\n    -N        Don't close console when shell exits"
//usage:     "\n    -t        Test mode, no elevation, implies -W"
//usage:     "\n    -W        Wait for shell exit code"

#include "libbb.h"
#include "lazyload.h"

enum {
	OPT_c = (1 << 0),
	OPT_s = (1 << 1),
	OPT_t = (1 << 2),
	OPT_N = (1 << 3),
	OPT_W = (1 << 4)
};

#define test_mode (opt & OPT_t)

int suw32_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int suw32_main(int argc UNUSED_PARAM, char **argv)
{
	int ret = 0;
	unsigned opt;
	char *opt_command = NULL;
	char *opt_shell = NULL;
	SHELLEXECUTEINFO info;
	char *bb_path, *cwd, *realcwd, *q, *args;
	DECLARE_PROC_ADDR(BOOL, ShellExecuteExA, SHELLEXECUTEINFOA *);

	opt = getopt32(argv, "^c:s:tNW" "\0" "s--N:N--s", &opt_command, &opt_shell);
	argv += optind;
	if (argv[0]) {
		if (!test_mode && strcmp(argv[0], "root") != 0) {
			bb_error_msg_and_die("unknown user '%s'", argv[0]);
		}
		++argv;
	}

#if ENABLE_DROP || ENABLE_CDROP || ENABLE_PDROP
	// If privilege has been dropped (ELEVATED_PRIVILEGE but not
	// ADMIN_ENABLED) ShellExecuteEx() thinks we already have elevated
	// privilege and doesn't raise privilege.  In that case, give up.
	if (!test_mode && elevation_state() == ELEVATED_PRIVILEGE) {
		xfunc_error_retval = 2;
		bb_error_msg_and_die("unable to restore privilege");
	}
#endif

	/* ShellExecuteEx() needs backslash as separator in UNC paths. */
	if (opt_shell) {
		bb_path = file_is_win32_exe(opt_shell);
		if (!bb_path)
			bb_error_msg_and_die("%s: Not found", opt_shell);
		args = NULL;
	} else {
		bb_path = xstrdup(bb_busybox_exec_path);
		args = xstrdup("--busybox ash");
		if (!test_mode)
			args = xappendword(args, "-t \"BusyBox ash (Admin)\"");
	}
	slash_to_bs(bb_path);

	memset(&info, 0, sizeof(SHELLEXECUTEINFO));
	info.cbSize = sizeof(SHELLEXECUTEINFO);
	/* info.fMask = SEE_MASK_DEFAULT; */
	if (opt & (OPT_t|OPT_W))
		info.fMask |= SEE_MASK_NOCLOSEPROCESS;
	if (test_mode)
		info.fMask |= SEE_MASK_NO_CONSOLE;
	/* info.hwnd = NULL; */
	info.lpVerb = !test_mode ? "runas" : "open";
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
	if (opt_shell == NULL) {
		cwd = getcwd(NULL, 0);
		realcwd = cwd ? xmalloc_realpath(cwd) : NULL;
		if (realcwd || cwd) {
			args = xappendword(args, "-d");
			q = quote_arg(realcwd ?: cwd);
			args = xappendword(args, q);
			free(q);
		}
	}

	if (opt & OPT_N)
		args = xappendword(args, "-N");

	if (opt_command) {
		args = xappendword(args,
			(opt_shell && strcasecmp(bb_basename(bb_path), "cmd.exe") == 0) ?
			"/c" : "-c");
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

	if (!INIT_PROC_ADDR(shell32.dll, ShellExecuteExA)) {
		ret = -1;
		goto end;
	}

	if (!ShellExecuteExA(&info)) {
		ret = 1;
		goto end;
	}

	if (opt & (OPT_t|OPT_W)) {
		DWORD r;

		WaitForSingleObject(info.hProcess, INFINITE);
		if (!GetExitCodeProcess(info.hProcess, &r))
			ret = 1;
		else
			ret = exit_code_to_posix(r);
		CloseHandle(info.hProcess);
	}
 end:
	if (ENABLE_FEATURE_CLEAN_UP) {
		free(bb_path);
		free(cwd);
		free(realcwd);
		free(args);
	}

	return ret;
}
