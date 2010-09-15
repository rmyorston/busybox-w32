#include "libbb.h"

int inet_aton(const char *cp, struct in_addr *inp)
{
	unsigned long val = inet_addr(cp);

	if (val == INADDR_NONE)
		return 0;
	inp->S_un.S_addr = val;
	return 1;
}

void init_winsock(void)
{
	WSADATA wsa;
	if (WSAStartup(MAKEWORD(2,2), &wsa))
		bb_error_msg_and_die("unable to initialize winsock subsystem, error %d",
				     WSAGetLastError());
	atexit((void(*)(void)) WSACleanup); /* may conflict with other atexit? */
}
