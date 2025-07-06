//kbuild:lib-$(CONFIG_USE_BB_CRYPT_YES) += y.o

#include <libbb.h>

#define YESCRYPT_INTERNAL
#include "alg-sha256.h"
#include "alg-yescrypt.h"
#include "alg-sha256.c"
#include "alg-yescrypt-platform.c"
#include "alg-yescrypt-kdf.c"
#include "alg-yescrypt-common.c"
