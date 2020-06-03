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
	static int initialized = 0;

	if (initialized)
		return;

	if (WSAStartup(MAKEWORD(2,2), &wsa))
		bb_error_msg_and_die("WSAStartup failed, error %d", WSAGetLastError());

	atexit((void(*)(void)) WSACleanup);
	initialized = 1;
}

#undef gethostname
int mingw_gethostname(char *name, int namelen)
{
	init_winsock();
	return gethostname(name, namelen);
}

#undef gethostbyaddr
struct hostent *mingw_gethostbyaddr(const void *addr, socklen_t len, int type)
{
	init_winsock();
	return gethostbyaddr(addr, len, type);
}

#undef getaddrinfo
int mingw_getaddrinfo(const char *node, const char *service,
				const struct addrinfo *hints, struct addrinfo **res)
{
	init_winsock();
	return getaddrinfo(node, service, hints, res);
}

int mingw_socket(int domain, int type, int protocol)
{
	int sockfd;
	SOCKET s;

	init_winsock();
	s = WSASocket(domain, type, protocol, NULL, 0, 0);
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
	if ((sockfd = _open_osfhandle((intptr_t)s, O_RDWR|O_BINARY)) < 0) {
		closesocket(s);
		bb_error_msg("unable to make a socket file descriptor: %s",
			     strerror(errno));
		return -1;
	}
	return sockfd;
}

#undef connect
int mingw_connect(int sockfd, const struct sockaddr *sa, size_t sz)
{
	SOCKET s = (SOCKET)_get_osfhandle(sockfd);
	return connect(s, sa, sz);
}

#undef bind
int mingw_bind(int sockfd, struct sockaddr *sa, size_t sz)
{
	SOCKET s = (SOCKET)_get_osfhandle(sockfd);
	return bind(s, sa, sz);
}

#undef setsockopt
int mingw_setsockopt(int sockfd, int lvl, int optname, void *optval, int optlen)
{
	SOCKET s = (SOCKET)_get_osfhandle(sockfd);
	return setsockopt(s, lvl, optname, (const char*)optval, optlen);
}

#undef shutdown
int mingw_shutdown(int sockfd, int how)
{
	SOCKET s = (SOCKET)_get_osfhandle(sockfd);
	return shutdown(s, how);
}

#undef listen
int mingw_listen(int sockfd, int backlog)
{
	SOCKET s = (SOCKET)_get_osfhandle(sockfd);
	return listen(s, backlog);
}

#undef accept
int mingw_accept(int sockfd1, struct sockaddr *sa, socklen_t *sz)
{
	int sockfd2;

	SOCKET s1 = (SOCKET)_get_osfhandle(sockfd1);
	SOCKET s2 = accept(s1, sa, sz);

	/* convert into a file descriptor */
	if ((sockfd2 = _open_osfhandle((intptr_t)s2, O_RDWR|O_BINARY)) < 0) {
		int err = errno;
		closesocket(s2);
		bb_error_msg("unable to make a socket file descriptor: %s",
			strerror(err));
		return -1;
	}
	return sockfd2;
}

#undef getpeername
int mingw_getpeername(int fd, struct sockaddr *sa, socklen_t *sz)
{
	SOCKET sock;

	init_winsock();
	sock = (SOCKET)_get_osfhandle(fd);
	if (sock == INVALID_SOCKET) {
		errno = EBADF;
		return -1;
	}
	return getpeername(sock, sa, sz);
}
