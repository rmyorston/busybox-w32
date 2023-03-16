/* vi: set sw=4 ts=4: */
/*
 * runuser - run a shell without elevated privileges.
 * This is a much restricted, Windows-specific reimplementation of
 * runuser from util-linux.
 *
 * Copyright (c) 2023 Ronald M Yorston
 *
 * Licensed under GPLv2 or later, see file LICENSE in this source tree.
 */
//config:config RUNUSER
//config:	bool "runuser"
//config:	default y
//config:	depends on PLATFORM_MINGW32 && SH_IS_ASH
//config:	help
//config:	Run a shell without elevated privileges
//config:
//config:config DROP
//config:	bool "drop"
//config:	default y
//config:	depends on PLATFORM_MINGW32 && SH_IS_ASH
//config:	help
//config:	Run a command without elevated privileges

//applet:IF_RUNUSER(APPLET(runuser, BB_DIR_USR_BIN, BB_SUID_DROP))
//applet:IF_DROP(APPLET_ODDNAME(drop, runuser, BB_DIR_USR_BIN, BB_SUID_DROP, drop))

//kbuild:lib-$(CONFIG_RUNUSER) += runuser.o
//kbuild:lib-$(CONFIG_DROP) += runuser.o

//usage:#define runuser_trivial_usage
//usage:	"USER [ARG...]"
//usage:#define runuser_full_usage "\n\n"
//usage:	"Run a shell without elevated privileges. The user name\n"
//usage:	"must be that of the user who was granted those privileges.\n"
//usage:	"Any arguments are passed to the shell.\n"

//usage:#define drop_trivial_usage
//usage:	"[COMMAND [ARG...]]"
//usage:#define drop_full_usage "\n\n"
//usage:	"Run a command without elevated privileges. Run the BusyBox\n"
//usage:	"shell if no COMMAND is provided. Any arguments are passed\n"
//usage:	"to the command.\n"

#include "libbb.h"
#include <winsafer.h>
#include <lazyload.h>

int runuser_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int runuser_main(int argc, char **argv)
{
#if ENABLE_RUNUSER && ENABLE_DROP
	int is_runuser = strcmp(applet_name, "runuser") == 0;
#else
	const int is_runuser = ENABLE_RUNUSER;
#endif
	const char *user, *exe;
	SAFER_LEVEL_HANDLE safer;
	HANDLE token;
	STARTUPINFO si;
	PROCESS_INFORMATION pi;
	TOKEN_MANDATORY_LABEL TIL;
	// Medium integrity level S-1-16-8192
	unsigned char medium[12] = {
		0x01, 0x01, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x10,
		0x00, 0x20, 0x00, 0x00
	};
	char *cmd, **a;
	DWORD code;
	// This shouldn't be necessary but without it the binary complains
	// it can't find CreateProcessAsUserA on older versions of Windows.
	DECLARE_PROC_ADDR(BOOL, CreateProcessAsUserA, HANDLE, LPCSTR, LPSTR,
			LPSECURITY_ATTRIBUTES, LPSECURITY_ATTRIBUTES, BOOL, DWORD,
			LPVOID, LPCSTR, LPSTARTUPINFOA, LPPROCESS_INFORMATION);

	if (!INIT_PROC_ADDR(advapi32.dll, CreateProcessAsUserA))
		bb_simple_error_msg_and_die("not supported");

	if (is_runuser) {
		if (getuid() != 0)
			bb_simple_error_msg_and_die("may not be used by non-root users");

		if (argc < 2)
			bb_show_usage();

		user = get_user_name();
		if (user == NULL || strcmp(argv[1], user) != 0)
			bb_simple_error_msg_and_die("invalid user");
	}

	/*
	 * Run a shell using a token with reduced privilege.  Hints from:
	 *
	 *    https://stackoverflow.com/questions/17765568/
	 */
	if (SaferCreateLevel(SAFER_SCOPEID_USER, SAFER_LEVELID_NORMALUSER,
					SAFER_LEVEL_OPEN, &safer, NULL) &&
			SaferComputeTokenFromLevel(safer, NULL, &token, 0, NULL)) {

		// Set medium integrity
		TIL.Label.Sid = (PSID)medium;
		TIL.Label.Attributes = SE_GROUP_INTEGRITY;
		if (SetTokenInformation(token, TokenIntegrityLevel, &TIL,
						sizeof(TOKEN_MANDATORY_LABEL))) {

			if (is_runuser || argc == 1) {
				exe = bb_busybox_exec_path;
				cmd = xstrdup("sh");
			} else {
				char *file;

#if ENABLE_FEATURE_PREFER_APPLETS
				if (!has_path(argv[1]) && find_applet_by_name(argv[1]) >= 0) {
					file = xstrdup(bb_busybox_exec_path);
					cmd = argv[1];
				} else
#endif
				if (has_path(argv[1])) {
					file = cmd = file_is_win32_exe(argv[1]);
				} else {
					file = cmd = find_first_executable(argv[1]);
				}

				if (file == NULL) {
					xfunc_error_retval = 127;
					bb_error_msg_and_die("can't find '%s'", argv[1]);
				}

				slash_to_bs(file);
				exe = file;
				cmd = xstrdup(cmd);
				file = quote_arg(cmd);
				if (file != cmd)
					free(cmd);
				cmd = file;
			}

			// Build the command line
			for (a = argv + 1 + (argc != 1); *a; ++a) {
				char *q = quote_arg(*a);
				char *newcmd = xasprintf("%s %s", cmd, q);
				if (q != *a)
					free(q);
				free(cmd);
				cmd = newcmd;
			}

			ZeroMemory(&si, sizeof(STARTUPINFO));
			si.cb = sizeof(STARTUPINFO);
			si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
			si.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
			si.hStdError = GetStdHandle(STD_ERROR_HANDLE);
			si.dwFlags = STARTF_USESTDHANDLES;

			if (!CreateProcessAsUserA(token, exe, cmd, NULL, NULL, TRUE,
						0, NULL, NULL, &si, &pi)) {
				xfunc_error_retval = 126;
				bb_error_msg_and_die("can't execute '%s'", exe);
			}

			WaitForSingleObject(pi.hProcess, INFINITE);
			if (GetExitCodeProcess(pi.hProcess, &code)) {
				return (int)code;
			}
		}
	}

	return EXIT_FAILURE;
}
