#include "libbb.h"
#include "win32.h"
#include "strbuf.h"
#include "git.h"

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
		return error("unable to make a socket file descriptor: %s",
			strerror(errno));
	}
	return sockfd;
}
#undef connect
int mingw_connect(int sockfd, struct sockaddr *sa, size_t sz)
{
	SOCKET s = (SOCKET)_get_osfhandle(sockfd);
	return connect(s, sa, sz);
}

void winsock_init()
{
	static int init = 0;
	if (!init) {
		WSADATA wsa;
		if (WSAStartup(MAKEWORD(2,2), &wsa))
		die("unable to initialize winsock subsystem, error %d",
			WSAGetLastError());
		atexit((void(*)(void)) WSACleanup);
		init = 1;
	}
}

/* this is the first function to call into WS_32; initialize it */
#undef gethostbyname
struct hostent *mingw_gethostbyname(const char *host)
{
	winsock_init();
	return gethostbyname(host);
}
