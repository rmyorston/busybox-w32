#include "libbb.h"
/* After libbb.h, since it needs sys/types.h on some systems */
#include <sys/utsname.h>

int uname(struct utsname *name)
{
	const char *unk = "unknown";
	OSVERSIONINFO os_info;
	SYSTEM_INFO sys_info;
	DWORD len;

	strcpy(name->sysname, "Windows_NT");

	len = sizeof(name->nodename) - 1;
	if ( !GetComputerName(name->nodename, &len) ) {
		strcpy(name->nodename, unk);
	}

	memset(&os_info, 0, sizeof(OSVERSIONINFO));
	os_info.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);

	strcpy(name->release, unk);
	strcpy(name->version, unk);
	if (GetVersionEx(&os_info)) {
		sprintf(name->release, "%d.%d", os_info.dwMajorVersion,
				os_info.dwMinorVersion);
		sprintf(name->version, "%d", os_info.dwBuildNumber);
	}

	strcpy(name->machine, unk);
	GetSystemInfo(&sys_info);
	switch (sys_info.wProcessorArchitecture) {
	case PROCESSOR_ARCHITECTURE_AMD64:
		strcpy(name->machine, "x86_64");
		break;
	case PROCESSOR_ARCHITECTURE_IA64:
		strcpy(name->machine, "ia64");
		break;
	case PROCESSOR_ARCHITECTURE_INTEL:
		if (sys_info.wProcessorLevel < 6) {
			strcpy(name->machine, "i386");
		}
		else {
			strcpy(name->machine, "i686");
		}
		break;
	}

	return 0;
}
