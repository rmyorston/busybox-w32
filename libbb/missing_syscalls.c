/*
 * Copyright 2012, Denys Vlasenko
 *
 * Licensed under GPLv2, see file LICENSE in this source tree.
 */

//kbuild:lib-$(CONFIG_PLATFORM_POSIX) += missing_syscalls.o

#include "libbb.h"

#if defined(ANDROID) || defined(__ANDROID__)
/*#include <linux/timex.h> - for struct timex, but may collide with <time.h> */
#include <sys/syscall.h>

pid_t getsid(pid_t pid)
{
	return syscall(__NR_getsid, pid);
}

int stime(const time_t *t)
{
	struct timeval tv;
	tv.tv_sec = *t;
	tv.tv_usec = 0;
	return settimeofday(&tv, NULL);
}

int sethostname(const char *name, size_t len)
{
	return syscall(__NR_sethostname, name, len);
}

struct timex;
int adjtimex(struct timex *buf)
{
	return syscall(__NR_adjtimex, buf);
}

int pivot_root(const char *new_root, const char *put_old)
{
	return syscall(__NR_pivot_root, new_root, put_old);
}
#endif

/* Copy from gnulib, strip for cygwin. */
#if defined(__CYGWIN__)
/* Native Windows API.  Also used on Cygwin.  */

/* Ensure that <windows.h> declares SetComputerNameEx.  */
#undef _WIN32_WINNT
#define _WIN32_WINNT 0x500

#define WIN32_LEAN_AND_MEAN

#include <windows.h>
/* The mingw header files don't define GetComputerNameEx, SetComputerNameEx.  */
#ifndef GetComputerNameEx
# define GetComputerNameEx GetComputerNameExA
#endif
#ifndef SetComputerNameEx
# define SetComputerNameEx SetComputerNameExA
#endif

/* Set up to LEN chars of NAME as system hostname.
   Return 0 if ok, set errno and return -1 on error. */

int
sethostname (const char *name, size_t len)
{
  char name_asciz[HOST_NAME_MAX + 1];
  char old_name[HOST_NAME_MAX + 1];
  DWORD old_name_len;

  /* Ensure the string isn't too long.  glibc does allow setting an
     empty hostname so no point in enforcing a lower bound. */
  if (len > HOST_NAME_MAX)
    {
      errno = EINVAL;
      return -1;
    }

  /* Prepare a NUL-terminated copy of name.  */
  memcpy (name_asciz, name, len);
  name_asciz[len] = '\0';

  /* Save the old NetBIOS name.  */
  old_name_len = sizeof (old_name) - 1;
  if (! GetComputerNameEx (ComputerNamePhysicalNetBIOS,
                           old_name, &old_name_len))
    old_name_len = 0;

  /* Set both the NetBIOS and the first part of the IP / DNS name.  */
  if (! SetComputerNameEx (ComputerNamePhysicalNetBIOS, name_asciz))
    {
      errno = (GetLastError () == ERROR_ACCESS_DENIED ? EPERM : EINVAL);
      return -1;
    }
  if (! SetComputerNameEx (ComputerNamePhysicalDnsHostname, name_asciz))
    {
      errno = (GetLastError () == ERROR_ACCESS_DENIED ? EPERM : EINVAL);
      /* Restore the old NetBIOS name.  */
      if (old_name_len > 0)
        {
          old_name[old_name_len] = '\0';
          SetComputerNameEx (ComputerNamePhysicalNetBIOS, old_name);
        }
      return -1;
    }

  /* Note that the new host name becomes effective only after a reboot!  */
  return 0;
}
#endif
