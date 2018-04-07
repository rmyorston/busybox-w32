#include <fcntl.h>
#include "libbb.h"
#include "NUM_APPLETS.h"

typedef struct {
	PROCESS_INFORMATION piProcInfo;
	HANDLE pipe[2];
	int fd;
} pipe_data;

static pipe_data *pipes = NULL;
static int num_pipes = 0;

static int mingw_popen_internal(pipe_data *p, const char *cmd,
					const char *mode, int fd0, pid_t *pid);

static int mingw_pipe(pipe_data *p, int bidi)
{
	SECURITY_ATTRIBUTES sa;

	sa.nLength = sizeof(sa);          /* Length in bytes */
	sa.bInheritHandle = 1;            /* the child must inherit these handles */
	sa.lpSecurityDescriptor = NULL;

	if (!bidi) {
		/* pipe[0] is the read handle, pipe[i] the write handle */
		if ( !CreatePipe (&p->pipe[0], &p->pipe[1], &sa, 1 << 13) ) {
			return -1;
		}
	}
	else {
		char *name;
		const int ip = 1; /* index of parent end of pipe */
		const int ic = 0; /* index of child end of pipe */
		static int count = 0;

		name = xasprintf("\\\\.\\pipe\\bb_pipe.%d.%d", getpid(), ++count);

		p->pipe[ip] = CreateNamedPipe(name,
							PIPE_ACCESS_DUPLEX|FILE_FLAG_OVERLAPPED,
							PIPE_TYPE_BYTE|PIPE_WAIT,
							1, 4096, 4096, 0, &sa);

		p->pipe[ic] = CreateFile(name, GENERIC_READ|GENERIC_WRITE, 0, &sa,
									OPEN_EXISTING,
									FILE_ATTRIBUTE_NORMAL|FILE_FLAG_OVERLAPPED,
									NULL);
		free(name);
	}

	return (p->pipe[0] == INVALID_HANDLE_VALUE ||
				p->pipe[1] == INVALID_HANDLE_VALUE) ? -1 : 0;
}

static void clear_pipe_data(pipe_data *p)
{
	memset(p, 0, sizeof(pipe_data));
	p->pipe[0] = INVALID_HANDLE_VALUE;
	p->pipe[1] = INVALID_HANDLE_VALUE;
	p->fd = -1;
}

static void close_pipe_data(pipe_data *p)
{
	if (p->pipe[0] != INVALID_HANDLE_VALUE)
		CloseHandle(p->pipe[0]);
	if (p->pipe[1] != INVALID_HANDLE_VALUE)
		CloseHandle(p->pipe[1]);
	clear_pipe_data(p);
}

/*
 * Search for a pipe_data structure with file descriptor fd.  If fd is
 * -1 and no empty slots are available the array is extended.  Return
 * NULL if the file descriptor can't be found or the array can't be
 * extended.
 */
static pipe_data *find_pipe(int fd)
{
	int i;
	pipe_data *p = NULL;

	/* find a matching pipe structure */
	for ( i=0; i<num_pipes; ++i ) {
		if (pipes[i].fd == fd) {
			p = pipes+i;
			break;
		}
	}

	/* if looking for valid file descriptor return now */
	if (fd != -1)
		return p;

	if ( p == NULL ) {
		/* need to extend array */
		if ( (p=realloc(pipes, sizeof(pipe_data)*(num_pipes+10))) == NULL ) {
			return NULL;
		}

		pipes = p;
		p = pipes + num_pipes;
		for ( i=0; i<10; ++i ) {
			clear_pipe_data(p+i);
		}
		num_pipes += 10;
	}
	clear_pipe_data(p);

	return p;
}

FILE *mingw_popen(const char *cmd, const char *mode)
{
	pipe_data *p;
	FILE *fptr = NULL;
	int fd;
	int len, count;
	char *cmd_buff = NULL;
	const char *s;
	char *t;

	if ( cmd == NULL || *cmd == '\0' || mode == NULL ||
			(*mode != 'r' && *mode != 'w') ) {
		return NULL;
	}

	/* find an unused pipe structure */
	if ((p=find_pipe(-1)) == NULL) {
		return NULL;
	}

	/* count double quotes */
	count = 0;
	for ( s=cmd; *s; ++s ) {
		if ( *s == '"' ) {
			++count;
		}
	}

	len = strlen(bb_busybox_exec_path) + strlen(cmd) + 32 + count;
	if ( (cmd_buff=malloc(len)) == NULL ) {
		return NULL;
	}
	cmd_buff[0] = '\0';

#if (ENABLE_FEATURE_PREFER_APPLETS || ENABLE_FEATURE_SH_STANDALONE) && \
		NUM_APPLETS > 1
	if (find_applet_by_name("sh") >= 0) {
		sprintf(cmd_buff, "%s --busybox ", bb_busybox_exec_path);
	}
#endif
	strcat(cmd_buff, "sh -c \"");

	/* escape double quotes */
	for ( s=cmd,t=cmd_buff+strlen(cmd_buff); *s; ++s ) {
		if ( *s == '"' ) {
			*t++ = '\\';
		}
		*t++ = *s;
	}
	*t++ = '"';
	*t = '\0';

	/* Create the pipe */
	if ((fd=mingw_popen_internal(p, cmd_buff, mode, -1, NULL)) != -1) {
		fptr = _fdopen(fd, *mode == 'r' ? "rb" : "wb");
	}

	free(cmd_buff);

	return fptr;
}

/*
 * Open a pipe to a command.
 *
 * - mode may be "r", "w" or "b" for read-only, write-only or
 *   bidirectional (from the perspective of the parent).
 * - if fd0 is a valid file descriptor it's used as input to the
 *   command ("r") or as the destination of the output from the
 *   command ("w").  Otherwise (and if not "b") use stdin or stdout.
 * - the pid of the command is returned in the variable pid, which
 *   can be NULL if the pid is not required.
 */
static int mingw_popen_internal(pipe_data *p, const char *cmd,
					const char *mode, int fd0, pid_t *pid)
{
	pipe_data pd;
	STARTUPINFO siStartInfo;
	int success;
	int fd = -1;
	int ip, ic, flags;

	if ( cmd == NULL || *cmd == '\0' || mode == NULL ) {
		return -1;
	}

	switch (*mode) {
	case 'r':
		ip = 0;
		flags = _O_RDONLY|_O_BINARY;
		break;
	case 'w':
		ip = 1;
		flags = _O_WRONLY|_O_BINARY;
		break;
	case 'b':
		ip = 1;
		flags = _O_RDWR|_O_BINARY;
		break;
	default:
		return -1;
	}
	ic = !ip;

	if (!p) {
		/* no struct provided, use a local one */
		p = &pd;
	}

	/* Create the pipe */
	if ( mingw_pipe(p, *mode == 'b') == -1 ) {
		goto finito;
	}

	/* Make the parent end of the pipe non-inheritable */
	SetHandleInformation(p->pipe[ip], HANDLE_FLAG_INHERIT, 0);

	/* Now create the child process */
	ZeroMemory(&siStartInfo, sizeof(STARTUPINFO));
	siStartInfo.cb = sizeof(STARTUPINFO);
	/* default settings for a bidirectional pipe */
	siStartInfo.hStdInput = p->pipe[ic];
	siStartInfo.hStdOutput = p->pipe[ic];
	/* override for read-only or write-only */
	if ( *mode == 'r' ) {
		siStartInfo.hStdInput = fd0 >= 0 ? (HANDLE)_get_osfhandle(fd0) :
											GetStdHandle(STD_INPUT_HANDLE);
	}
	else if ( *mode == 'w' ) {
		siStartInfo.hStdOutput = fd0 >= 0 ? (HANDLE)_get_osfhandle(fd0) :
											GetStdHandle(STD_OUTPUT_HANDLE);
	}
	siStartInfo.hStdError = GetStdHandle(STD_ERROR_HANDLE);
	siStartInfo.wShowWindow = SW_HIDE;
	siStartInfo.dwFlags = STARTF_USESTDHANDLES|STARTF_USESHOWWINDOW;

	success = CreateProcess(NULL,
				(LPTSTR)cmd,       /* command line */
				NULL,              /* process security attributes */
				NULL,              /* primary thread security attributes */
				TRUE,              /* handles are inherited */
				0,                 /* creation flags */
				NULL,              /* use parent's environment */
				NULL,              /* use parent's current directory */
				&siStartInfo,      /* STARTUPINFO pointer */
				&p->piProcInfo);   /* receives PROCESS_INFORMATION */

	if ( !success ) {
		goto finito;
	}

	/* close child end of pipe */
	CloseHandle(p->pipe[ic]);
	p->pipe[ic] = INVALID_HANDLE_VALUE;

	fd = _open_osfhandle((intptr_t)p->pipe[ip], flags);

finito:
	if ( fd == -1 ) {
		close_pipe_data(p);
	}
	else {
		p->fd = fd;
		if ( pid ) {
			*pid = (pid_t)p->piProcInfo.dwProcessId;
		}
	}

	return fd;
}

int mingw_popen_fd(const char *cmd, const char *mode, int fd0, pid_t *pid)
{
	return mingw_popen_internal(NULL, cmd, mode, fd0, pid);
}

int mingw_pclose(FILE *fp)
{
	int fd;
	pipe_data *p;
	DWORD ret;

	/* find struct containing fd */
	if (fp == NULL || (fd=fileno(fp)) == -1 || (p=find_pipe(fd)) == NULL)
		return -1;

	fclose(fp);

	ret = WaitForSingleObject(p->piProcInfo.hProcess, INFINITE);

	CloseHandle(p->piProcInfo.hProcess);
	CloseHandle(p->piProcInfo.hThread);
	close_pipe_data(p);

	return (ret == WAIT_OBJECT_0) ? 0 : -1;
}
