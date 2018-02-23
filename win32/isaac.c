/*
------------------------------------------------------------------------------
readable.c: My random number generator, ISAAC.
(c) Bob Jenkins, March 1996, Public Domain
You may use this code in any way you wish, and it is free.  No warrantee.
* May 2008 -- made it not depend on standard.h
------------------------------------------------------------------------------

  The original version of this file was downloaded from Bob Jenkins website:

     http://burtleburtle.net/bob/rand/isaacafa.html

  The isaac and randinit functions have been slightly modified to silence
  warnings in modern compilers; the get_entropy and get_random_bytes have
  been added.

  These changes were made by R M Yorston and are also dedicated to the
  public domain.
*/
#include "libbb.h"

/* external results */
static uint32_t randrsl[256];

/* internal state */
static uint32_t mm[256];
static uint32_t aa=0, bb=0, cc=0;


static void isaac(void)
{
   register uint32_t i,x,y;

   cc = cc + 1;    /* cc just gets incremented once per 256 results */
   bb = bb + cc;   /* then combined with bb */

   for (i=0; i<256; ++i)
   {
     x = mm[i];
     switch (i%4)
     {
     case 0: aa = aa^(aa<<13); break;
     case 1: aa = aa^(aa>>6); break;
     case 2: aa = aa^(aa<<2); break;
     case 3: aa = aa^(aa>>16); break;
     }
     aa              = mm[(i+128)%256] + aa;
     mm[i]      = y  = mm[(x>>2)%256] + aa + bb;
     randrsl[i] = bb = mm[(y>>10)%256] + x;

     /* Note that bits 2..9 are chosen from x but 10..17 are chosen
        from y.  The only important thing here is that 2..9 and 10..17
        don't overlap.  2..9 and 10..17 were then chosen for speed in
        the optimized version (rand.c) */
     /* See http://burtleburtle.net/bob/rand/isaac.html
        for further explanations and analysis. */
   }
}


/* if (flag!=0), then use the contents of randrsl[] to initialize mm[]. */
#define mix(a,b,c,d,e,f,g,h) \
{ \
   a^=b<<11; d+=a; b+=c; \
   b^=c>>2;  e+=b; c+=d; \
   c^=d<<8;  f+=c; d+=e; \
   d^=e>>16; g+=d; e+=f; \
   e^=f<<10; h+=e; f+=g; \
   f^=g>>4;  a+=f; g+=h; \
   g^=h<<8;  b+=g; h+=a; \
   h^=a>>9;  c+=h; a+=b; \
}

static void randinit(int flag)
{
   int i;
   uint32_t a,b,c,d,e,f,g,h;
   aa=bb=cc=0;
   a=b=c=d=e=f=g=h=0x9e3779b9;  /* the golden ratio */

   for (i=0; i<4; ++i)          /* scramble it */
   {
     mix(a,b,c,d,e,f,g,h);
   }

   for (i=0; i<256; i+=8)   /* fill in mm[] with messy stuff */
   {
     if (flag)                  /* use all the information in the seed */
     {
       a+=randrsl[i  ]; b+=randrsl[i+1]; c+=randrsl[i+2]; d+=randrsl[i+3];
       e+=randrsl[i+4]; f+=randrsl[i+5]; g+=randrsl[i+6]; h+=randrsl[i+7];
     }
     mix(a,b,c,d,e,f,g,h);
     mm[i  ]=a; mm[i+1]=b; mm[i+2]=c; mm[i+3]=d;
     mm[i+4]=e; mm[i+5]=f; mm[i+6]=g; mm[i+7]=h;
   }

   if (flag)
   {        /* do a second pass to make all of the seed affect all of mm */
     for (i=0; i<256; i+=8)
     {
       a+=mm[i  ]; b+=mm[i+1]; c+=mm[i+2]; d+=mm[i+3];
       e+=mm[i+4]; f+=mm[i+5]; g+=mm[i+6]; h+=mm[i+7];
       mix(a,b,c,d,e,f,g,h);
       mm[i  ]=a; mm[i+1]=b; mm[i+2]=c; mm[i+3]=d;
       mm[i+4]=e; mm[i+5]=f; mm[i+6]=g; mm[i+7]=h;
     }
   }

   isaac();            /* fill in the first set of results */
}

/* call 'fn' to put data in 'dt' then copy it to generator state */
#define GET_DATA(fn, dt) \
	fn(&dt); \
	u = (uint32_t *)&dt; \
	for (j=0; j<sizeof(dt)/sizeof(uint32_t); ++j) { \
		randrsl[i++] = *u++; \
	}

/*
 * Stuff a few bytes of random-ish data into the generator state.
 * This is unlikely to be very robust:  don't rely on it for
 * anything that needs to be secure.
 */
static void get_entropy(void)
{
	int i, j, len;
	SYSTEMTIME tm;
	MEMORYSTATUS ms;
	SYSTEM_INFO si;
	LARGE_INTEGER pc;
	uint32_t *u;
	char *env, *s;
	md5_ctx_t ctx;
	unsigned char buf[16];

	i = 0;
	randrsl[i++] = (uint32_t)GetCurrentProcessId();
	randrsl[i++] = (uint32_t)GetCurrentThreadId();
	randrsl[i++] = (uint32_t)GetTickCount();

	GET_DATA(GetLocalTime, tm)
	GET_DATA(GlobalMemoryStatus, ms)
	GET_DATA(GetSystemInfo, si)
	GET_DATA(QueryPerformanceCounter, pc)

	env = GetEnvironmentStringsA();

	/* get length of environment:  it ends with two nuls */
	for (s=env,len=0; *s && *(s+1); ++s,++len)
		;

	md5_begin(&ctx);
	md5_hash(&ctx, env, len);
	md5_end(&ctx, buf);

	FreeEnvironmentStringsA(env);

	u = (uint32_t *)buf;
	for (j=0; j<sizeof(buf)/sizeof(uint32_t); ++j) {
		randrsl[i++] = *u++;
	}

#if 0
	{
		unsigned char *p = (unsigned char *)randrsl;

		for (j=0; j<i*sizeof(uint32_t); ++j) {
			fprintf(stderr, "%02x", *p++);
			if ((j&31) == 31) {
				fprintf(stderr, "\n");
			}
			else if ((j&3) == 3) {
				fprintf(stderr, " ");
			}
		}
		fprintf(stderr, "\n");
	}
#endif
}

#define RAND_BYTES sizeof(randrsl)
#define RAND_WORDS (sizeof(randrsl)/sizeof(randrsl[0]))

/*
 * Place 'count' random bytes in the buffer 'buf'.  You're responsible
 * for ensuring the buffer is big enough.
 */
ssize_t get_random_bytes(void *buf, ssize_t count)
{
	static int initialised = 0;
	static int rand_index = 0;
	int i;
	ssize_t save_count = count;
	unsigned char *ptr = (unsigned char *)randrsl;

	if (buf == NULL || count < 0) {
		errno = EINVAL;
		return -1;
	}

	if (!initialised) {
		aa = bb = cc = (uint32_t)0;
		for (i=0; i<RAND_WORDS; ++i)
			mm[i] = randrsl[i] = (uint32_t)0;

		get_entropy();
		randinit(1);
		isaac();
		rand_index = 0;
		initialised = 1;
	}

	while (count > 0) {
		int bytes_left = RAND_BYTES - rand_index;

		if (bytes_left >= count) {
			/* we have enough random bytes */
			memcpy(buf, ptr+rand_index, count);
			rand_index += count;
			count = 0;
		}
		else {
			/* insufficient bytes, use what we have */
			memcpy(buf, ptr+rand_index, bytes_left);
			buf += bytes_left;
			count -= bytes_left;
			rand_index += bytes_left;
		}

		if (rand_index >= RAND_BYTES) {
			/* generate more */
			isaac();
			rand_index = 0;
		}
	}

	return save_count;
}
