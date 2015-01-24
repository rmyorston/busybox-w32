#include <stdlib.h>
#include <stdio.h>
#include <errno.h>

typedef int gid_t;
typedef int uid_t;


#ifndef __NT__
int ttyname_r(int fd, char *buf, size_t buflen)
{
	//strlcpy(buf,ttyname(fd),buflen);
	//return 0;
	return EBADF;
}

/*
        mktemp.c        Create a temporary file name.
        Copyright (c) 1996,1997 by Christopher S L Heng. All rights reserved.

        $Id: mktemp.c 1.4 1997/10/08 17:47:58 chris Exp $

        This code is released under the terms of the GNU General Public
        License Version 2. You should have received a copy of the GNU
        General Public License along with this program; if not, write to the
        Free Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139,
        USA.

        The mktemp() function does not exist in the Watcom 10.[0-6] library.
*/

#include <unistd.h>         /* access() */
#include <string.h>     /* strlen(), strcmp() */

#define MKTEMP_TEMPLATE "XXXXXX"

#define MAXVAL  (65535u)        /* unsigned is at least this (ANSI) */

/*
        mktemp

        Returns a string containing a temporary filename.

        The caller must initialise "templ" to contain 6 trailing
        'X's. mktemp() will directly modify the templ contents,
        replacing the 'X's with the filename.

        WARNING: your prefix in "templ" + the six trailing 'X's,
        must not exceed the DOS filesystem naming limitations or
        unpredictable results may occur. For example, since a filename
        cannot exceed 8 characters in length, you cannot call
        mktemp() with a templ of "mytempXXXXXX". Note that
        "unpredictable results" does not mean that mktemp() will
        return NULL. It means exactly that _unpredictable_ results
        will occur (probably depending on your prefix).

        Returns: - pointer to "templ" on success;
                 - NULL on failure. Note that even if NULL is returned,
                the "templ" contents would have been changed
                as well.

        Example:
                char tempfilename[] = "\\tmp\\myXXXXXX" ;
*/
char * mktemp ( char * templ )
{
        static unsigned val ;
        static char fch = 'A' ;

        char *s ;
        char *startp ;
        size_t len ;
        unsigned tval ;

        /* do some sanity checks */
        /* make sure that templ is at least 6 characters long */
        /* and comprises the "XXXXXX" string at the end */
        if ((len = strlen(templ)) < 6 ||
                strcmp( (s = startp = templ + len - 6), MKTEMP_TEMPLATE ))
                return NULL ;
        for ( ; fch <= 'Z'; val = 0, fch++ ) {
                /* plug the first character */
                *startp = fch ;
                /* convert val to ascii */
                /* note that we skip the situation where val == MAXVAL */
                /* because if unsigned has a maximum value of MAXVAL */
                /* in an implementation, and we do a compare of */
                /* val <= MAXVAL, the test will always return true! */
                /* Our way, we have at least a cut-off point: MAXVAL. */
                for ( ; val < MAXVAL; ) {
                        tval = val++ ;
                        for (s = startp + 5; s > startp ; s--) {
                                *s = (char) ((tval % 10) + '0') ;
                                tval /= 10 ;
                        }
                        if (access( templ, 0 ) == -1 && errno == ENOENT)
                                return templ ;
                }
        }
        return NULL ;
}

/* Copyright (C) 1995 DJ Delorie, see COPYING.DJ for details */
#include <sys/time.h>
#include <utime.h>
int
utimes(const char *file, struct timeval tvp[2])
{
	struct utimbuf utb;

	utb.actime = tvp[0].tv_sec;
	utb.modtime = tvp[1].tv_sec;
	return utime(file, &utb);
}

#endif

#undef mknod
int
mknod ( const char *filename,  int mode,  int dev)
{
  errno = ENOSYS;
  return -1;
}

#undef chown
int
chown ( const char *filename,  int owner,  int group)
{
  errno = ENOSYS;
  return -1;
}


#undef symlink
int
symlink ( const char *oldname,  const char *newname)
{
  errno = ENOSYS;
  return -1;
}

#undef getgroups
int getgroups(int gidsetsize, gid_t grouplist[]) {
 errno = ENOSYS;
 return 0;
}

#undef getgid
gid_t getgid(void) {
  errno = ENOSYS;
  return 0;
}

#undef getuid
uid_t getuid(void) {
 errno = ENOSYS;
 return 0;
}

/* the following functions fail to link when debug build with CFLAGS -g2 is used */

void BUG_sizeof(void) {
}

void sed_free_and_close_stuff(void) {
}

void data_extract_to_command(void) {
}

void open_transformer(void) {
}

void check_errors_in_children(void) {
}








