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
//usage:       "[-c \"CMD\"]"
//usage:#define suw32_full_usage "\n\n"
//usage:       "Run shell with elevated privileges\n"
//usage:     "\n    -c CMD  Command to pass to 'sh -c'"

#include "libbb.h"

int suw32_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int suw32_main(int argc UNUSED_PARAM, char **argv)
{
	char *opt_command = NULL;
	SHELLEXECUTEINFO info;
	char *cwd;

	getopt32(argv, "c:", &opt_command);
	if (argv[optind])
		bb_show_usage();

	memset(&info, 0, sizeof(SHELLEXECUTEINFO));
	info.cbSize = sizeof(SHELLEXECUTEINFO);
	/* info.fMask = SEE_MASK_DEFAULT; */
	/* info.hwnd = NULL; */
	info.lpVerb = "runas";
	info.lpFile = bb_busybox_exec_path;
	cwd = getcwd(NULL, 0);
	if (opt_command)
		info.lpParameters =
			xasprintf("ash -d \"%s\" -s -c \"%s\"", cwd, opt_command);
	else
		info.lpParameters = xasprintf("ash -d \"%s\"", cwd);
	/* info.lpDirectory = NULL; */
	info.nShow = SW_SHOWNORMAL;

	return !ShellExecuteEx(&info);
}
