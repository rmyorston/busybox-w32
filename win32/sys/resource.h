#ifndef _SYS_RESOURCE_H
#define _SYS_RESOURCE_H 1

#include <time.h>

struct rusage {
	struct timeval ru_utime;
	struct timeval ru_stime;
};

#endif
