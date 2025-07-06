//kbuild:lib-$(CONFIG_USE_BB_CRYPT_YES) += y.o

#include <libbb.h>

#include <assert.h>

static inline void
cpu_to_le32 (unsigned char *buf, uint32_t n)
{
  buf[0] = (unsigned char)((n & 0x000000FFu) >>  0);
  buf[1] = (unsigned char)((n & 0x0000FF00u) >>  8);
  buf[2] = (unsigned char)((n & 0x00FF0000u) >> 16);
  buf[3] = (unsigned char)((n & 0xFF000000u) >> 24);
}
static inline uint32_t
le32_to_cpu (const unsigned char *buf)
{
  return ((((uint32_t)buf[0]) <<  0) |
          (((uint32_t)buf[1]) <<  8) |
          (((uint32_t)buf[2]) << 16) |
          (((uint32_t)buf[3]) << 24) );
}

/* Alternative names used in code derived from Colin Percival's
   cryptography libraries.  */
#define le32enc cpu_to_le32
#define le32dec le32_to_cpu
#define le64enc cpu_to_le64
#define le64dec le64_to_cpu

#define be32enc cpu_to_be32
#define be32dec be32_to_cpu
#define be64enc cpu_to_be64
#define be64dec be64_to_cpu

#define be32enc_vect cpu_to_be32_vect
#define be32dec_vect be32_to_cpu_vect
#define be64enc_vect cpu_to_be64_vect
#define be64dec_vect be64_to_cpu_vect


//USED ONY BY SHA256 for be32_to_cpu_vect():
static inline void
cpu_to_be32(unsigned char *buf, uint32_t n)
{
  buf[0] = (unsigned char)((n & 0xFF000000u) >> 24);
  buf[1] = (unsigned char)((n & 0x00FF0000u) >> 16);
  buf[2] = (unsigned char)((n & 0x0000FF00u) >>  8);
  buf[3] = (unsigned char)((n & 0x000000FFu) >>  0);
}
static inline void
cpu_to_be64 (unsigned char *buf, uint64_t n)
{
  buf[0] = (unsigned char)((n & 0xFF00000000000000ull) >> 56);
  buf[1] = (unsigned char)((n & 0x00FF000000000000ull) >> 48);
  buf[2] = (unsigned char)((n & 0x0000FF0000000000ull) >> 40);
  buf[3] = (unsigned char)((n & 0x000000FF00000000ull) >> 32);
  buf[4] = (unsigned char)((n & 0x00000000FF000000ull) >> 24);
  buf[5] = (unsigned char)((n & 0x0000000000FF0000ull) >> 16);
  buf[6] = (unsigned char)((n & 0x000000000000FF00ull) >>  8);
  buf[7] = (unsigned char)((n & 0x00000000000000FFull) >>  0);
}
static inline uint32_t
be32_to_cpu (const unsigned char *buf)
{
  return ((((uint32_t)buf[0]) << 24) |
          (((uint32_t)buf[1]) << 16) |
          (((uint32_t)buf[2]) <<  8) |
          (((uint32_t)buf[3]) <<  0) );
}
static inline uint64_t
be64_to_cpu (const unsigned char *buf)
{
  return ((((uint64_t)buf[0]) << 56) |
          (((uint64_t)buf[1]) << 48) |
          (((uint64_t)buf[2]) << 40) |
          (((uint64_t)buf[3]) << 32) |
          (((uint64_t)buf[4]) << 24) |
          (((uint64_t)buf[5]) << 16) |
          (((uint64_t)buf[6]) <<  8) |
          (((uint64_t)buf[7]) <<  0) );
}
/* Template: Define a function named cpu_to_<END><BITS>_vect that
   takes a vector SRC of LEN integers, each of type uint<BITS>_t, and
   writes them to the buffer DST in the endianness defined by END.
   Caution: LEN is the number of vector elements, not the total size
   of the buffers.  */
#define VECTOR_CPU_TO(end, bits) VECTOR_CPU_TO_(end##bits, uint##bits##_t)
#define VECTOR_CPU_TO_(prim, stype)                                     \
  static inline void                                                    \
  cpu_to_##prim##_vect(uint8_t *dst, const stype *src, size_t len)      \
  {                                                                     \
    while (len)                                                         \
      {                                                                 \
        cpu_to_##prim(dst, *src);                                       \
        src += 1;                                                       \
        dst += sizeof(stype);                                           \
        len -= 1;                                                       \
      }                                                                 \
  } struct _swallow_semicolon
/* Template: Define a function named <END><BITS>_to_cpu_vect that
   reads a vector of LEN integers, each of type uint<BITS>_t, from the
   buffer SRC, in the endianness defined by END, and writes them to
   the vector DST.  Caution: LEN is the number of vector elements, not
   the total size of the buffers.  */
#define VECTOR_TO_CPU(end, bits) VECTOR_TO_CPU_(end##bits, uint##bits##_t)
#define VECTOR_TO_CPU_(prim, dtype)                                     \
  static inline void                                                    \
  prim##_to_cpu_vect(dtype *dst, const uint8_t *src, size_t len)        \
  {                                                                     \
    while (len)                                                         \
      {                                                                 \
        *dst = prim##_to_cpu(src);                                      \
        src += sizeof(dtype);                                           \
        dst += 1;                                                       \
        len -= 1;                                                       \
      }                                                                 \
  } struct _swallow_semicolon
/* These are the vectorized endianness-conversion functions that are
   presently used.  Add more as necessary.  */
VECTOR_CPU_TO(be,32);
VECTOR_CPU_TO(be,64);
VECTOR_TO_CPU(be,32);
VECTOR_TO_CPU(be,64);


#define YESCRYPT_INTERNAL
#include "alg-sha256.h"
#include "alg-yescrypt.h"
#include "alg-sha256.c"
#include "alg-yescrypt-platform.c"
#include "alg-yescrypt-kdf.c"
#include "alg-yescrypt-common.c"
