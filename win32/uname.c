#include "libbb.h"
/* After libbb.h, since it needs sys/types.h on some systems */
#include <sys/utsname.h>

int uname(struct utsname *name)
{
	const char *unk = "unknown";
	OSVERSIONINFO os_info;
	SYSTEM_INFO sys_info;

	strcpy(name->sysname, "Windows_NT");

	if ( gethostname(name->nodename, sizeof(name->nodename)) != 0 ) {
		strcpy(name->nodename, unk);
	}

	memset(&os_info, 0, sizeof(OSVERSIONINFO));
	os_info.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);

	strcpy(name->release, unk);
	strcpy(name->version, unk);
	if (GetVersionEx(&os_info)) {
		sprintf(name->release, "%u.%u", (unsigned int)os_info.dwMajorVersion,
				(unsigned int)os_info.dwMinorVersion);
		sprintf(name->version, "%u", (unsigned int)os_info.dwBuildNumber);
	}

	GetSystemInfo(&sys_info);
	switch (sys_info.wProcessorArchitecture) {
	case PROCESSOR_ARCHITECTURE_AMD64:
		strcpy(name->machine, "x86_64");
		break;
	case PROCESSOR_ARCHITECTURE_INTEL:
		if (sys_info.wProcessorLevel < 6) {
			strcpy(name->machine, "i386");
		}
		else {
			strcpy(name->machine, "i686");
		}
		break;
	default:
		strcpy(name->machine, unk);
		break;
	}

	return 0;
}
