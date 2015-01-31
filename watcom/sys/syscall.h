#ifndef _SYS_SYSCALL_H
#define _SYS_SYSCALL_H

#if defined(__386__)

#define __NR_getsid 147
#define __NR_sethostname 74
#define __NR_adjtimex 124
#define __NR_pivot_root 217

#endif


/* 64-bit
#define __NR_getsid 124
#define __NR_sethostname 170
#define __NR_adjtimex 159
#define __NR_pivot_root 155
*/


#endif