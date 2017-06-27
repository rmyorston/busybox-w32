#include "libbb.h"

int mingw_system(const char *cmd)
{
	const char *argv[4] = { "sh", "-c", cmd, NULL };
	intptr_t proc;
	HANDLE h;
	DWORD ret = 0;

	if (cmd == NULL)
		return 1;

	if ((proc=mingw_spawn_proc(argv)) == -1)
		return -1;

	h = (HANDLE)proc;
	WaitForSingleObject(h, INFINITE);
	GetExitCodeProcess(h, &ret);
	CloseHandle(h);

	return ret << 8;
}
