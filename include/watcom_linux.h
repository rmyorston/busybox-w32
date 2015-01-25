/* a collection of watcom-specific hacks */

#ifndef WATCOM_HACKS
#define WATCOM_HACKS 1

#include <netdb.h> /*h_error*/
#include <netinet/in.h> /* sockaddr_in*/

/* getting rid of GCC-isms */
#undef __volatile__
#define __volatile__ /* nothing */
#undef __attribute__
#define __attribute__(x) /*nothing*/

#define NOIMPL(name,...) static inline int name(__VA_ARGS__) { errno = ENOSYS; return -1; }
#define IMPL(name,ret,retval,...) static inline ret name(__VA_ARGS__) { return retval; }

#define ENABLE_USE_PORTABLE_CODE 1


/*various */
#ifndef BB_VER
#define BB_VER "1.23.0.watcom2"
#endif

#define NSIG 12

int utimes(const char, struct timeval *);
typedef unsigned char* RE_TRANSLATE_TYPE;
typedef void (*sighandler_t)(int);

/* from musl netdb.h */

struct addrinfo
{
	int ai_flags;
	int ai_family;
	int ai_socktype;
	int ai_protocol;
	socklen_t ai_addrlen;
	struct sockaddr *ai_addr;
	char *ai_canonname;
	struct addrinfo *ai_next;
};

#define AI_CANONNAME 0x02
#define AI_NUMERICHOST 0x04
#define NI_NUMERICSERV 0x400
#define NI_NUMERICHOST  0x01
#define NI_NAMEREQD 0x08


#endif /* WATCOM_HACKS */



