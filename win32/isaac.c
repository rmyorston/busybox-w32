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

typedef struct {
	/* external results */
	uint32_t randrsl[256];

	/* internal state */
	uint32_t mm[256];
	uint32_t aa, bb, cc;
} isaac_t;


static void isaac(isaac_t *t)
{
   register uint32_t i,x,y;

   t->cc = t->cc + 1;    /* cc just gets incremented once per 256 results */
   t->bb = t->bb + t->cc;   /* then combined with bb */

   for (i=0; i<256; ++i)
   {
     x = t->mm[i];
     switch (i%4)
     {
     case 0: t->aa = t->aa^(t->aa<<13); break;
     case 1: t->aa = t->aa^(t->aa>>6); break;
     case 2: t->aa = t->aa^(t->aa<<2); break;
     case 3: t->aa = t->aa^(t->aa>>16); break;
     }
     t->aa              = t->mm[(i+128)%256] + t->aa;
     t->mm[i]      = y  = t->mm[(x>>2)%256] + t->aa + t->bb;
     t->randrsl[i] = t->bb = t->mm[(y>>10)%256] + x;

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

static void randinit(isaac_t *t, int flag)
{
   int i;
   uint32_t a,b,c,d,e,f,g,h;
   t->aa = t->bb = t->cc = 0;
   a=b=c=d=e=f=g=h=0x9e3779b9;  /* the golden ratio */

   for (i=0; i<4; ++i)          /* scramble it */
   {
     mix(a,b,c,d,e,f,g,h);
   }

   for (i=0; i<256; i+=8)   /* fill in mm[] with messy stuff */
   {
     if (flag)                  /* use all the information in the seed */
     {
       a+=t->randrsl[i  ]; b+=t->randrsl[i+1]; c+=t->randrsl[i+2];
       d+=t->randrsl[i+3]; e+=t->randrsl[i+4]; f+=t->randrsl[i+5];
       g+=t->randrsl[i+6]; h+=t->randrsl[i+7];
     }
     mix(a,b,c,d,e,f,g,h);
     t->mm[i  ]=a; t->mm[i+1]=b; t->mm[i+2]=c; t->mm[i+3]=d;
     t->mm[i+4]=e; t->mm[i+5]=f; t->mm[i+6]=g; t->mm[i+7]=h;
   }

   if (flag)
   {        /* do a second pass to make all of the seed affect all of mm */
     for (i=0; i<256; i+=8)
     {
       a+=t->mm[i  ]; b+=t->mm[i+1]; c+=t->mm[i+2]; d+=t->mm[i+3];
       e+=t->mm[i+4]; f+=t->mm[i+5]; g+=t->mm[i+6]; h+=t->mm[i+7];
       mix(a,b,c,d,e,f,g,h);
       t->mm[i  ]=a; t->mm[i+1]=b; t->mm[i+2]=c; t->mm[i+3]=d;
       t->mm[i+4]=e; t->mm[i+5]=f; t->mm[i+6]=g; t->mm[i+7]=h;
     }
   }

   isaac(t);            /* fill in the first set of results */
}

/* call 'fn' to put data in 'dt' then copy it to generator state */
#define GET_DATA(fn, dt) \
	fn(&dt); \
	u = (uint32_t *)&dt; \
	for (j=0; j<sizeof(dt)/sizeof(uint32_t); ++j) { \
		t->randrsl[i++] = *u++; \
	}

/*
 * Stuff a few bytes of random-ish data into the generator state.
 * This is unlikely to be very robust:  don't rely on it for
 * anything that needs to be secure.
 */
static void get_entropy(isaac_t *t)
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
	t->randrsl[i++] = (uint32_t)GetCurrentProcessId();
	t->randrsl[i++] = (uint32_t)GetCurrentThreadId();
	t->randrsl[i++] = (uint32_t)GetTickCount();

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
		t->randrsl[i++] = *u++;
	}

#if 0
	{
		unsigned char *p = (unsigned char *)t->randrsl;

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

#define RAND_BYTES sizeof(t->randrsl)
#define RAND_WORDS (sizeof(t->randrsl)/sizeof(t->randrsl[0]))

/*
 * Place 'count' random bytes in the buffer 'buf'.  You're responsible
 * for ensuring the buffer is big enough.
 */
ssize_t get_random_bytes(void *buf, ssize_t count)
{
	static isaac_t *t = NULL;
	static int rand_index = 0;
	ssize_t save_count = count;
	unsigned char *ptr;

	if (buf == NULL || count < 0) {
		errno = EINVAL;
		return -1;
	}

	if (!t) {
		t = xzalloc(sizeof(isaac_t));

		get_entropy(t);
		randinit(t, 1);
		isaac(t);
		rand_index = 0;
	}

	ptr = (unsigned char *)t->randrsl;
	while (count > 0) {
		int bytes_left = RAND_BYTES - rand_index;
		ssize_t delta = MIN(bytes_left, count);

		memcpy(buf, ptr+rand_index, delta);
		buf += delta;
		count -= delta;
		rand_index += delta;

		if (rand_index >= RAND_BYTES) {
			/* generate more */
			isaac(t);
			rand_index = 0;
		}
	}

	return save_count;
}
