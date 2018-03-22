#include "libbb.h"
#include "../shell/random.h"

/* call 'fn' to put data in 'dt' then copy it to generator state */
#define GET_DATA(fn, dt) \
	fn(&dt); \
	u = (uint32_t *)&dt; \
	for (j=0; j<sizeof(dt)/sizeof(uint32_t); ++j) { \
		state[i++%2] ^= *u++; \
	}

/*
 * Obtain a few bytes of random-ish data to initialise the generator.
 * This is unlikely to be very robust:  don't rely on it for
 * anything that needs to be secure.
 */
static void get_entropy(uint32_t state[2])
{
	int i, j;
	SYSTEMTIME tm;
	MEMORYSTATUS ms;
	SYSTEM_INFO si;
	LARGE_INTEGER pc;
	uint32_t *u;

	i = 0;
	state[i++%2] ^= (uint32_t)GetCurrentProcessId();
	state[i++%2] ^= (uint32_t)GetCurrentThreadId();
	state[i++%2] ^= (uint32_t)GetTickCount();

	GET_DATA(GetLocalTime, tm)
	GET_DATA(GlobalMemoryStatus, ms)
	GET_DATA(GetSystemInfo, si)
	GET_DATA(QueryPerformanceCounter, pc)

#if 0
	{
		unsigned char *p = (unsigned char *)state;

		for (j=0; j<8; ++j) {
			fprintf(stderr, "%02x", *p++);
			if ((j&3) == 3) {
				fprintf(stderr, " ");
			}
		}
		fprintf(stderr, "\n");
	}
#endif
}

ssize_t get_random_bytes(void *buf, ssize_t count)
{
	static random_t rnd;
	ssize_t save_count = count;
	uint32_t value;
	unsigned char *ptr = (unsigned char *)&value;

	if (buf == NULL || count < 0) {
		errno = EINVAL;
		return -1;
	}

	if (UNINITED_RANDOM_T(&rnd)) {
		uint32_t state[2] = {0, 0};

		get_entropy(state);
		INIT_RANDOM_T(&rnd, state[0] ? state[0] : 1, state[1]);
	}

	for (;count > 0; buf+=4, count-=4) {
		value = full_random(&rnd);
		memcpy(buf, ptr, count >= 4 ? 4 : count);
	}

	return save_count;
}
