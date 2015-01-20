/* a collection of watcom-specific hacks */

#ifndef WATCOM_HACKS
#define WATCOM_HACKS 1

#include <sys/socket.h>
#include <netinet/in.h>



/*various */
#ifndef BB_VER
#define BB_VER "1.23.0.watcom2"
#endif

#endif /* WATCOM_HACKS */