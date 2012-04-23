#include "libbb.h"

int ioctl(int fd UNUSED_PARAM, int code, ...)
{
	va_list ap;
	void *arg;
	int ret = -1;

	va_start(ap, code);

	switch (code) {
	case TIOCGWINSZ:
		arg = va_arg(ap, void *);
		ret = winansi_get_terminal_width_height((struct winsize *)arg);
		break;
	default:
		ret = -1;
		errno = EINVAL;
		break;
	}

	va_end(ap);
	return ret;
}
