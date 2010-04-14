#include "libbb.h"

int mingw_main(int argc, char **argv);

int mingw_main(int argc, char **argv)
{
	if (argc < 1)
		return 1;

	if (!strcmp(argv[1], "spawn")) {
		int ret = spawn(argv+1);
		printf("ret = %d\n", ret);
		return ret;
	}
	return 1;
}
