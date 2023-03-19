/* vi: set sw=4 ts=4: */
/*
 * drop - run a command without elevated privileges.
 *
 * Copyright (c) 2023 Ronald M Yorston
 *
 * Licensed under GPLv2 or later, see file LICENSE in this source tree.
 */
//config:config DROP
//config:	bool "drop"
//config:	default y
//config:	depends on PLATFORM_MINGW32 && SH_IS_ASH
//config:	help
//config:	Run a command without elevated privileges

//applet:IF_DROP(APPLET(drop, BB_DIR_USR_BIN, BB_SUID_DROP))

//kbuild:lib-$(CONFIG_DROP) += drop.o

//usage:#define drop_trivial_usage
//usage:	"[COMMAND | -c [ARG...]]"
//usage:#define drop_full_usage "\n\n"
//usage:	"Drop elevated privileges and run a command. If no COMMAND\n"
//usage:	"is provided run the BusyBox shell.\n"

#include "libbb.h"
#include <winsafer.h>
#include <lazyload.h>

int drop_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int drop_main(int argc, char **argv)
{
	const char *exe;
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
	char *cmd, *q, *newcmd, **a;
	DWORD code;
	// This shouldn't be necessary but without it the binary complains
	// it can't find CreateProcessAsUserA on older versions of Windows.
	DECLARE_PROC_ADDR(BOOL, CreateProcessAsUserA, HANDLE, LPCSTR, LPSTR,
			LPSECURITY_ATTRIBUTES, LPSECURITY_ATTRIBUTES, BOOL, DWORD,
			LPVOID, LPCSTR, LPSTARTUPINFOA, LPPROCESS_INFORMATION);

	if (!INIT_PROC_ADDR(advapi32.dll, CreateProcessAsUserA))
		bb_simple_error_msg_and_die("not supported");

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
			int skip = 1;

			if (argc == 1 || strcmp(argv[1], "-c") == 0) {
				exe = bb_busybox_exec_path;
				cmd = xstrdup("sh");
				skip = 0;
			} else {
				char *file;

#if ENABLE_FEATURE_PREFER_APPLETS
				if (!has_path(argv[1]) && find_applet_by_name(argv[1]) >= 0) {
					file = xstrdup(bb_busybox_exec_path);
				} else
#endif
				if (has_path(argv[1])) {
					file = file_is_win32_exe(argv[1]);
				} else {
					file = find_first_executable(argv[1]);
				}

				if (file == NULL) {
					xfunc_error_retval = 127;
					bb_error_msg_and_die("can't find '%s'", argv[1]);
				}

				slash_to_bs(file);
				exe = file;
				cmd = quote_arg(argv[1]);
			}

			// Build the command line
			for (a = argv + 1 + skip; *a; ++a) {
				q = quote_arg(*a);
				newcmd = xasprintf("%s %s", cmd, q);
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
