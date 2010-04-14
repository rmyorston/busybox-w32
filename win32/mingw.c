#include "libbb.h"

unsigned int _CRT_fmode = _O_BINARY;

unsigned int sleep (unsigned int seconds)
{
	Sleep(seconds*1000);
	return 0;
}
