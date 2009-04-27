#include "strbuf.h"

struct strbuf_file {
	struct strbuf input;
	int has_error;
	int fd;
};

struct strbuf_file *sfdopen(int fd, const char *mode);
int sfclose(struct strbuf_file *sf);
int sfprintf(struct strbuf_file *sf, const char *fmt, ...);
void sfclearerr(struct strbuf_file *sf);
int sfread(char *buf, int size, int n, struct strbuf_file *sf);
char *sfgets(char *buf, int size, struct strbuf_file *sf);
int sfgetc(struct strbuf_file *sf);

#ifdef REPLACE_STDIO

#ifdef FILE
#undef FILE
#endif
#define FILE struct strbuf_file

#ifdef fdopen
#undef fdopen
#endif
#define fdopen(fd,mode) sfdopen(fd,mode)

#ifdef fclose
#undef fclose
#endif
#define fclose(fp) sfclose(fp)

#ifdef fprintf
#undef fprintf
#endif
#define fprintf sfprintf

#ifdef fread
#undef fread
#endif
#define fread(buf,s,n,fp) sfread(buf,s,n,fp)

#ifdef clearerr
#undef clearerr
#endif
#define clearerr(fp) sfclearerr(fp)

#ifdef ferror
#undef ferror
#endif
#define ferror(fp) ((fp)->has_error)

#ifdef fgets
#undef fgets
#endif
#define fgets(buf,n,fp) sfgets(buf,n,fp)

#ifdef getc
#undef getc
#endif
#define getc(fp) sfgetc(fp)

#ifdef fflush
#undef fflush
#endif
#define fflush(fp) 0

#endif
