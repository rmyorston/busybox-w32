#include "busybox.h"

int tcsetattr(int fd UNUSED_PARAM, int mode UNUSED_PARAM,  const struct termios *t UNUSED_PARAM)
{
	return -1;
}

int tcgetattr(int fd UNUSED_PARAM, struct termios *t UNUSED_PARAM)
{
	return -1;
}
