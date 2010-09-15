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

int mingw_socket(int domain, int type, int protocol)
{
	int sockfd;
	SOCKET s = WSASocket(domain, type, protocol, NULL, 0, 0);
	if (s == INVALID_SOCKET) {
		/*
		 * WSAGetLastError() values are regular BSD error codes
		 * biased by WSABASEERR.
		 * However, strerror() does not know about networking
		 * specific errors, which are values beginning at 38 or so.
		 * Therefore, we choose to leave the biased error code
		 * in errno so that _if_ someone looks up the code somewhere,
		 * then it is at least the number that are usually listed.
		 */
		errno = WSAGetLastError();
		return -1;
	}
	/* convert into a file descriptor */
	if ((sockfd = _open_osfhandle(s, O_RDWR|O_BINARY)) < 0) {
		closesocket(s);
		bb_error_msg("unable to make a socket file descriptor: %s",
			     strerror(errno));
		return -1;
	}
	return sockfd;
}
