/* stubs and compat functions for missing stuff in the Watcom Linux libc */

#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#undef __OBSCURE_STREAM_INTERNALS
#include <stdio.h>
#include <errno.h>
#include <pwd.h>
#include <grp.h>
#include <arpa/inet.h>
#include <sys/socket.h>

/* stubs.... can we find better functions? */

int getpagesize(void)
{
	return 4096;
}

int mknod ( const char *filename,  int mode,  int dev)
{
  errno = ENOSYS;
  return -1;
}

int getgroups (int n, gid_t *groups)
{
  errno = ENOSYS;
  return -1;
}

struct group *getgrnam( const char *__name )
{
    errno = ENOSYS;
    return -1;
}

int statfs(const char *file, struct statfs *buf)
{
    return 0;
}

int getpwnam_r( const char *__name )
{
        return 0;
}

struct passwd *getpwnam( const char *__name )
{
        return NULL;
}

struct passwd *getpwent(void)
{
    return NULL;
}

struct passwd *getpwent_r(void)
{
    return NULL;
}

void endpwent(void)
{
}

void setpwent(void)
{
}

int ttyname_r(int fd, char *buf, size_t buflen)
{
	//strlcpy(buf,ttyname(fd),buflen);
	//return 0;
	return EBADF;
}

struct mntent {
               char *mnt_fsname;   /* name of mounted filesystem */
               char *mnt_dir;      /* filesystem path prefix */
               char *mnt_type;     /* mount type (see mntent.h) */
               char *mnt_opts;     /* mount options (see mntent.h) */
               int   mnt_freq;     /* dump frequency in days */
               int   mnt_passno;   /* pass number on parallel fsck */
           };

FILE *setmntent(const char *filename, const char *type)
{
        return NULL;
}

struct mntent *getmntent(FILE *streamp, struct mntent *mntbuf,
                                  char *buf, int buflen)
{
        return NULL;
}

struct mntent *getmntent_r(FILE *streamp, struct mntent *mntbuf,
                                  char *buf, int buflen)
{
        return NULL;
}

int endmntent(FILE *streamp)
{
        return 1;
}

int inet_aton(const char *cp, struct in_addr *addr)
{
        return 0; /*fail*/
}

int socketpair(int domain, int type, int protocol,
       int socket_vector[2])
{
        errno = ENOSYS;
        return -1;
}

struct group *getgrgid(gid_t gid)
{
        errno = ENOSYS;
        return NULL;
}

int setgroups(int gidsetsize, gid_t grouplist[])
{
    errno = ENOSYS;
    return -1;
}

long syscall(long n, ...)
{
        errno = ENOSYS;
        return 0;
}

ssize_t pread(int fildes, void *buf, size_t nbyte, off_t offset)
{
    return read(fildes, buf, nbyte);
}

/* end of stubs start of functions */

char *realpath(const char *path, char *resolved_path)
{
	/* FIXME: need normalization */
	return strcpy(resolved_path, path);
}

struct passwd *getpwuid(uid_t uid)
{
	static char user_name[100];
	static struct passwd p;
	long len = sizeof(user_name);

	user_name[0] = '\0';
	p.pw_name = user_name;
	p.pw_gecos = (char *)"unknown";
	p.pw_dir = NULL;
	p.pw_shell = NULL;
	p.pw_uid = 1000;
	p.pw_gid = 1000;

	return &p;
}

long sysconf(int name)
{
	if ( name == _SC_CLK_TCK ) {
		return 100;
	}
	errno = EINVAL;
	return -1;
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








