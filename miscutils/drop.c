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

//config:config CDROP
//config:	bool "cdrop"
//config:	default y
//config:	depends on PLATFORM_MINGW32
//config:	help
//config:	Run a command without elevated privileges using cmd.exe

//config:config PDROP
//config:	bool "pdrop"
//config:	default y
//config:	depends on PLATFORM_MINGW32
//config:	help
//config:	Run a command without elevated privileges using PowerShell

//applet:IF_DROP(APPLET(drop, BB_DIR_USR_BIN, BB_SUID_DROP))
//applet:IF_CDROP(APPLET_ODDNAME(cdrop, drop, BB_DIR_USR_BIN, BB_SUID_DROP, cdrop))
//applet:IF_PDROP(APPLET_ODDNAME(pdrop, drop, BB_DIR_USR_BIN, BB_SUID_DROP, pdrop))

//kbuild:lib-$(CONFIG_DROP) += drop.o
//kbuild:lib-$(CONFIG_CDROP) += drop.o
//kbuild:lib-$(CONFIG_PDROP) += drop.o

//usage:#define drop_trivial_usage
//usage:	"[COMMAND [ARG...] | -c CMD_STRING [ARG...]]"
//usage:#define drop_full_usage "\n\n"
//usage:	"Drop elevated privileges and run a command. If no command\n"
//usage:	"is provided run the BusyBox shell.\n"

//usage:#define cdrop_trivial_usage
//usage:	"[COMMAND [ARG...] | -c CMD_STRING [ARG...]]"
//usage:#define cdrop_full_usage "\n\n"
//usage:	"Drop elevated privileges and run a command. If no command\n"
//usage:	"is provided run cmd.exe.\n"

//usage:#define pdrop_trivial_usage
//usage:	"[COMMAND [ARG...] | -c CMD_STRING [ARG...]]"
//usage:#define pdrop_full_usage "\n\n"
//usage:	"Drop elevated privileges and run a command. If no command\n"
//usage:	"is provided run PowerShell.\n"

#include "libbb.h"
#include <winsafer.h>
#include <lazyload.h>
#include "NUM_APPLETS.h"

// Set an environment variable to the name of the unprivileged user,
// but only if it was previously unset or contained "root".
static void setenv_name(const char *key)
{
	const char *name = get_user_name();
	const char *oldname = getenv(key);

	if (name && (!oldname || strcmp(oldname, "root") == 0)) {
		setenv(key, name, 1);
	}
}

int drop_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int drop_main(int argc, char **argv)
{
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
			char *opt_command = NULL;
			char **a;
			const char *opt, *arg;
			char *exe, *cmd, *q;

			getopt32(argv, "c:", &opt_command);
			a = argv + optind;
			opt = "-c";

			if (argc == 1 || opt_command) {
				switch (*applet_name) {
#if ENABLE_PDROP
				case 'p':
					arg = "powershell.exe";
					exe = find_first_executable(arg);
					break;
#endif
#if ENABLE_CDROP
				case 'c':
					opt = "/c";
					arg = "cmd.exe";
					exe = find_first_executable(arg);
					break;
#endif
#if ENABLE_DROP
				case 'd':
					arg = "sh";
					exe = xstrdup(bb_busybox_exec_path);
					break;
#endif
				default:
					// Never executed, just to silence warnings.
					arg = argv[0];
					exe = NULL;
					break;
				}
			} else {
				arg = *a++;

#if ENABLE_FEATURE_PREFER_APPLETS && NUM_APPLETS > 1
				if (!has_path(arg) && find_applet_by_name(arg) >= 0) {
					exe = xstrdup(bb_busybox_exec_path);
				} else
#endif
				if (has_path(arg)) {
					exe = file_is_win32_exe(arg);
				} else {
					exe = find_first_executable(arg);
				}
			}

			if (exe == NULL) {
				xfunc_error_retval = 127;
				bb_error_msg_and_die("can't find '%s'", arg);
			}

			slash_to_bs(exe);
			cmd = quote_arg(arg);
			if (opt_command) {
				cmd = xappendword(cmd, opt);
				q = quote_arg(opt_command);
				cmd = xappendword(cmd, q);
				free(q);
			}

			// Build the command line
			while (*a) {
				q = quote_arg(*a++);
				cmd = xappendword(cmd, q);
				free(q);
			}

			ZeroMemory(&si, sizeof(STARTUPINFO));
			si.cb = sizeof(STARTUPINFO);
			si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
			si.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
			si.hStdError = GetStdHandle(STD_ERROR_HANDLE);
			si.dwFlags = STARTF_USESTDHANDLES;

			setenv_name("USER");
			setenv_name("LOGNAME");

			if (!CreateProcessAsUserA(token, exe, cmd, NULL, NULL, TRUE,
						0, NULL, NULL, &si, &pi)) {
				xfunc_error_retval = 126;
				bb_error_msg_and_die("can't execute '%s'", exe);
			}

			kill_child_ctrl_handler(pi.dwProcessId);
			SetConsoleCtrlHandler(kill_child_ctrl_handler, TRUE);
			WaitForSingleObject(pi.hProcess, INFINITE);
			if (GetExitCodeProcess(pi.hProcess, &code)) {
				return (int)code;
			}
		}
	}

	return EXIT_FAILURE;
}
