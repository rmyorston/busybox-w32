#include "libbb.h"
#include <ntsecapi.h>
#include "../shell/random.h"

/*
 * Obtain a few bytes of random-ish data to initialise the generator.
 * This is unlikely to be very robust:  don't rely on it for
 * anything that needs to be secure.
 */
static void get_entropy(uint32_t state[2])
{
#if defined(__MINGW64_VERSION_MAJOR) && \
		(__MINGW64_VERSION_MAJOR >= 7 || defined(__MINGW64__))
	if (!RtlGenRandom(state, sizeof(state[0])*2))
#endif
		GetSystemTimeAsFileTime((FILETIME *)state);

#if 0
	{
		unsigned char *p = (unsigned char *)state;
		int j;

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
