
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

struct netent
{
        char *n_name;
        char **n_aliases;
        int n_addrtype;
        uint32_t n_net;
};

struct netent *getnetbyaddr (uint32_t, int);



#define AI_CANONNAME 0x02
#define AI_NUMERICHOST 0x04
#define NI_NUMERICSERV 0x400
#define NI_NUMERICHOST  0x01
#define NI_NAMEREQD 0x08


#define AI_NUMERICSERV 0x400
#define AI_PASSIVE 0x01

#define EAI_BADFLAGS -1
#define EAI_FAMILY -6
#define EAI_SOCKTYPE -7
#define EAI_NONAME -2
#define EAI_NODATA -5
#define EAI_MEMORY -10
#define EAI_OVERFLOW -12

/* end of musl import */


