#include <fcntl.h>
#include "libbb.h"

typedef struct {
	PROCESS_INFORMATION piProcInfo;
	HANDLE pipe[2];
	char mode;
	int fd;
} pipe_data;

static pipe_data *pipes = NULL;
static int num_pipes = 0;

static int mingw_pipe(HANDLE *readwrite)
{
	SECURITY_ATTRIBUTES sa;

	sa.nLength = sizeof(sa);          /* Length in bytes */
	sa.bInheritHandle = 1;            /* the child must inherit these handles */
	sa.lpSecurityDescriptor = NULL;

	if ( !CreatePipe (&readwrite[0], &readwrite[1], &sa, 1 << 13) ) {
		return -1;
	}

	return 0;
}

static pipe_data *find_pipe(void)
{
	int i;
	pipe_data *p = NULL;

	/* find an unused pipe structure */
	for ( i=0; i<num_pipes; ++i ) {
		if ( pipes[i].mode == '\0' ) {
			p = pipes+i;
			break;
		}
	}

	if ( p == NULL ) {
		/* need to extend array */
		if ( (p=realloc(pipes, sizeof(pipe_data)*(num_pipes+10))) == NULL ) {
			return NULL;
		}

		pipes = p;
		for ( i=0; i<10; ++i ) {
			memset(pipes+num_pipes+i, 0, sizeof(pipe_data));
		}
		p = pipes + num_pipes;
		num_pipes += 10;
	}

	p->pipe[0] = INVALID_HANDLE_VALUE;
	p->pipe[1] = INVALID_HANDLE_VALUE;

	return p;
}

FILE *mingw_popen(const char *cmd, const char *mode)
{
	pipe_data *p;
	FILE *fptr = NULL;
	STARTUPINFO siStartInfo;
	int success;
	int fd;
	int len, count;
	int ip, ic;
	char *cmd_buff = NULL;
	const char *s;
	char *t;

	if ( cmd == NULL || *cmd == '\0' || mode == NULL ||
			(*mode != 'r' && *mode != 'w') ) {
		return NULL;
	}

	/* find an unused pipe structure */
	if ( (p=find_pipe()) == NULL ) {
		return NULL;
	}

	/* count double quotes */
	count = 0;
	for ( s=cmd; *s; ++s ) {
		if ( *s == '"' ) {
			++count;
		}
	}

	len = strlen(cmd) + 10 + count;
	if ( (cmd_buff=malloc(len)) == NULL ) {
		return NULL;
	}

	/* escape double quotes */
	strcpy(cmd_buff, "sh -c \"");
	for ( s=cmd,t=cmd_buff+strlen(cmd_buff); *s; ++s ) {
		if ( *s == '"' ) {
			*t++ = '\\';
		}
		*t++ = *s;
	}
	*t++ = '"';
	*t = '\0';

	/* Create the pipe */
	if ( mingw_pipe(p->pipe) == -1 ) {
		goto finito;
	}

	/* index of parent end of pipe */
	ip = !(*mode == 'r');
	/* index of child end of pipe */
	ic = (*mode == 'r');

	/* Make the parent end of the pipe non-inheritable */
	SetHandleInformation(p->pipe[ip], HANDLE_FLAG_INHERIT, 0);

	/* Now create the child process */
	ZeroMemory(&siStartInfo, sizeof(STARTUPINFO));
	siStartInfo.cb = sizeof(STARTUPINFO);
	if ( *mode == 'r' ) {
		siStartInfo.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
		siStartInfo.hStdOutput = p->pipe[ic];
	}
	else {
		siStartInfo.hStdInput = p->pipe[ic];
		siStartInfo.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
	}
	siStartInfo.hStdError = GetStdHandle(STD_ERROR_HANDLE);
	siStartInfo.wShowWindow = SW_HIDE;
	siStartInfo.dwFlags = STARTF_USESTDHANDLES|STARTF_USESHOWWINDOW;

	success = CreateProcess(NULL,
				(LPTSTR)cmd_buff,  /* command line */
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

	if ( *mode == 'r' ) {
		fd = _open_osfhandle((long)p->pipe[ip], _O_RDONLY|_O_BINARY);
		fptr = _fdopen(fd, "rb");
	}
	else {
		fd = _open_osfhandle((long)p->pipe[ip], _O_WRONLY|_O_BINARY);
		fptr = _fdopen(fd, "wb");
	}

finito:
	if ( !fptr ) {
		if ( p->pipe[0] != INVALID_HANDLE_VALUE ) {
			CloseHandle(p->pipe[0]);
		}
		if ( p->pipe[1] != INVALID_HANDLE_VALUE ) {
			CloseHandle(p->pipe[1]);
		}
	}
	else {
		p->mode = *mode;
		p->fd = fd;
	}
	free(cmd_buff);

	return fptr;
}

/*
 * Open a pipe to a command where the file descriptor fd0 is used
 * as input to the command (read mode) or as the destination of the
 * output from the command (write mode).  The pid of the command is
 * returned in the variable pid, which can be NULL.
 */
int mingw_popen_fd(const char *cmd, const char *mode, int fd0, pid_t *pid)
{
	pipe_data *p;
	STARTUPINFO siStartInfo;
	int success;
	int fd = -1;
	int ip, ic;

	if ( cmd == NULL || *cmd == '\0' || mode == NULL ||
			(*mode != 'r' && *mode != 'w') ) {
		return -1;
	}

	/* find an unused pipe structure */
	if ( (p=find_pipe()) == NULL ) {
		return -1;
	}

	/* Create the pipe */
	if ( mingw_pipe(p->pipe) == -1 ) {
		goto finito;
	}

	/* index of parent end of pipe */
	ip = !(*mode == 'r');
	/* index of child end of pipe */
	ic = (*mode == 'r');

	/* Make the parent end of the pipe non-inheritable */
	SetHandleInformation(p->pipe[ip], HANDLE_FLAG_INHERIT, 0);

	/* Now create the child process */
	ZeroMemory(&siStartInfo, sizeof(STARTUPINFO));
	siStartInfo.cb = sizeof(STARTUPINFO);
	if ( *mode == 'r' ) {
		siStartInfo.hStdInput = (HANDLE)_get_osfhandle(fd0);
		siStartInfo.hStdOutput = p->pipe[ic];
	}
	else {
		siStartInfo.hStdInput = p->pipe[ic];
		siStartInfo.hStdOutput = (HANDLE)_get_osfhandle(fd0);
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

	if ( *mode == 'r' ) {
		fd = _open_osfhandle((long)p->pipe[ip], _O_RDONLY|_O_BINARY);
	}
	else {
		fd = _open_osfhandle((long)p->pipe[ip], _O_WRONLY|_O_BINARY);
	}

finito:
	if ( fd == -1 ) {
		if ( p->pipe[0] != INVALID_HANDLE_VALUE ) {
			CloseHandle(p->pipe[0]);
		}
		if ( p->pipe[1] != INVALID_HANDLE_VALUE ) {
			CloseHandle(p->pipe[1]);
		}
	}
	else {
		p->mode = *mode;
		p->fd = fd;
		if ( pid ) {
			*pid = (pid_t)p->piProcInfo.dwProcessId;
		}
	}

	return fd;
}

int mingw_pclose(FILE *fp)
{
	int i, ip, fd;
	pipe_data *p = NULL;
	DWORD ret;

	if ( fp == NULL ) {
		return -1;
	}

	/* find struct containing fd */
	fd = fileno(fp);
	for ( i=0; i<num_pipes; ++i ) {
		if ( pipes[i].mode && pipes[i].fd == fd ) {
			p = pipes+i;
			break;
		}
	}

	if ( p == NULL ) {
		/* no pipe data, maybe fd isn't a pipe? */
		return -1;
	}

	fclose(fp);

	ip = !(p->mode == 'r');
	CloseHandle(p->pipe[ip]);

	ret = WaitForSingleObject(p->piProcInfo.hProcess, INFINITE);

	CloseHandle(p->piProcInfo.hProcess);
	CloseHandle(p->piProcInfo.hThread);

	p->mode = '\0';

	return (ret == WAIT_OBJECT_0) ? 0 : -1;
}
