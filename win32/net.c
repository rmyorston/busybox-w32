#include "libbb.h"

int inet_aton(const char *cp, struct in_addr *inp)
{
	unsigned long val = inet_addr(cp);

	if (val == INADDR_NONE)
		return 0;
	inp->S_un.S_addr = val;
	return 1;
}
