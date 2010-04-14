#include "libbb.h"

int waitpid(pid_t pid, int *status, unsigned options)
{
	/* Windows does not understand parent-child */
	if (options == 0 && pid != -1)
		return _cwait(status, pid, 0);
	errno = EINVAL;
	return -1;
}
