#include "libbb.h"
#include <tlhelp32.h>

int waitpid(pid_t pid, int *status, int options)
{
	HANDLE proc;
	intptr_t ret;

	/* Windows does not understand parent-child */
	if (pid > 0 && options == 0) {
		if ( (proc=OpenProcess(SYNCHRONIZE|PROCESS_QUERY_INFORMATION,
						FALSE, pid)) != NULL ) {
			ret = _cwait(status, (intptr_t)proc, 0);
			CloseHandle(proc);
			return ret == -1 ? -1 : pid;
		}
	}
	errno = pid < 0 ? ENOSYS : EINVAL;
	return -1;
}

const char *
next_path_sep(const char *path)
{
	static const char *from = NULL, *to;
	static int has_semicolon;
	int len = strlen(path);

	if (!from || !(path >= from && path+len <= to)) {
		from = path;
		to = from+len;
		has_semicolon = strchr(path, ';') != NULL;
	}

	/* Semicolons take precedence, it's Windows PATH */
	if (has_semicolon)
		return strchr(path, ';');
	/* PATH=C:, not really a separator */
	return strchr(has_dos_drive_prefix(path) ? path+2 : path, ':');
}

typedef struct {
	char *path;
	char *name;
	char *opts;
	char buf[100];
} interp_t;

static int
parse_interpreter(const char *cmd, interp_t *interp)
{
	char *path, *t;
	int n, fd;

	fd = open(cmd, O_RDONLY);
	if (fd < 0)
		return 0;
	n = read(fd, interp->buf, sizeof(interp->buf)-1);
	close(fd);
	if (n < 4)	/* at least '#!/x' and not error */
		return 0;

	/*
	 * See http://www.in-ulm.de/~mascheck/various/shebang/ for trivia
	 * relating to '#!'.
	 */
	if (interp->buf[0] != '#' || interp->buf[1] != '!')
		return 0;
	interp->buf[n] = '\0';
	if ((t=strchr(interp->buf, '\n')) == NULL)
		return 0;
	t[1] = '\0';

	if ((path=strtok(interp->buf+2, " \t\r\n")) == NULL)
		return 0;

	t = (char *)bb_basename(path);
	if (*t == '\0')
		return 0;

	interp->path = path;
	interp->name = t;
	interp->opts = strtok(NULL, "\r\n");

	return 1;
}

/*
 * See http://msdn2.microsoft.com/en-us/library/17w5ykft(vs.71).aspx
 * (Parsing C++ Command-Line Arguments)
 */
static char *
quote_arg(const char *arg)
{
	int len = 0, n = 0;
	int force_quotes = 0;
	char *q, *d;
	const char *p = arg;

	/* empty arguments must be quoted */
	if (!*p) {
		force_quotes = 1;
	}

	while (*p) {
		if (isspace(*p)) {
			/* arguments containing whitespace must be quoted */
			force_quotes = 1;
		}
		else if (*p == '"') {
			/* double quotes in arguments need to be escaped */
			n++;
		}
		else if (*p == '\\') {
			/* count contiguous backslashes */
			int count = 0;
			while (*p == '\\') {
				count++;
				p++;
				len++;
			}

			/*
			 * Only escape backslashes before explicit double quotes or
			 * or where the backslashes are at the end of an argument
			 * that is scheduled to be quoted.
			 */
			if (*p == '"' || (force_quotes && *p == '\0')) {
				n += count*2 + 1;
			}

			if (*p == '\0') {
				break;
			}
			continue;
		}
		len++;
		p++;
	}

	if (!force_quotes && n == 0) {
		return (char*)arg;
	}

	/* insert double quotes and backslashes where necessary */
	d = q = xmalloc(len+n+3);
	if (force_quotes) {
		*d++ = '"';
	}

	while (*arg) {
		if (*arg == '"') {
			*d++ = '\\';
		}
		else if (*arg == '\\') {
			int count = 0;
			while (*arg == '\\') {
				count++;
				*d++ = *arg++;
			}

			if (*arg == '"' || (force_quotes && *arg == '\0')) {
				while (count-- > 0) {
					*d++ = '\\';
				}
				if (*arg == '"') {
					*d++ = '\\';
				}
			}
		}
		if (*arg != '\0') {
			*d++ = *arg++;
		}
	}
	if (force_quotes) {
		*d++ = '"';
	}
	*d = '\0';

	return q;
}

static char *
find_first_executable(const char *name)
{
	char *tmp, *path = getenv("PATH");
	char *exe_path = NULL;

	if (path) {
		tmp = path = xstrdup(path);
		exe_path = find_executable(name, &tmp);
		free(path);
	}

	return exe_path;
}

static intptr_t
spawnveq(int mode, const char *path, char *const *argv, char *const *env)
{
	char **new_argv;
	char *new_path = NULL;
	int i, argc = -1;
	intptr_t ret;
	struct stat st;

	/*
	 * Require that the file exists, is a regular file and is executable.
	 * It may still contain garbage but we let spawnve deal with that.
	 */
	if (stat(path, &st) == 0) {
		if (!S_ISREG(st.st_mode) || !(st.st_mode&S_IXUSR)) {
			errno = EACCES;
			fprintf(stderr, "spawnveq: %s: %s\n", path, strerror(errno));
			return -1;
		}
	}
	else {
		fprintf(stderr, "spawnveq: %s: %s\n", path, strerror(errno));
		return -1;
	}

	while (argv[++argc])
		;

	new_argv = xmalloc(sizeof(*argv)*(argc+1));
	for (i = 0;i < argc;i++)
		new_argv[i] = quote_arg(argv[i]);
	new_argv[argc] = NULL;

	/*
	 * Special case:  spawnve won't execute a batch file when the path
	 * starts with a '.' and contains forward slashes.
	 */
	if (new_argv[0][0] == '.') {
		char *s, *p;

		if (has_bat_suffix(new_argv[0])) {
			p = strdup(new_argv[0]);
		}
		else {
			p = add_win32_extension(new_argv[0]);
		}

		if (p != NULL && has_bat_suffix(p)) {
			for (s=p; *s; ++s) {
				if (*s == '/')
					*s = '\\';
			}
			if (new_argv[0] != argv[0])
				free(new_argv[0]);
			new_argv[0] = p;
		}
		else {
			free(p);
		}
	}

	/*
	 * Another special case:  if a file doesn't have an extension add
	 * a '.' at the end.  This forces spawnve to use precisely the
	 * file specified without trying to add an extension.
	 */
	if (!strchr(bb_basename(path), '.')) {
		new_path = xasprintf("%s.", path);
	}

	ret = spawnve(mode, new_path ? new_path : path, new_argv, env);

	for (i = 0;i < argc;i++)
		if (new_argv[i] != argv[i])
			free(new_argv[i]);
	free(new_argv);
	free(new_path);

	return ret;
}

#if ENABLE_FEATURE_PREFER_APPLETS || ENABLE_FEATURE_SH_STANDALONE
static intptr_t
mingw_spawn_applet(int mode,
		   char *const *argv,
		   char *const *envp)
{
	return spawnveq(mode, bb_busybox_exec_path, argv, envp);
}
#endif

static intptr_t
mingw_spawn_interpreter(int mode, const char *prog, char *const *argv, char *const *envp)
{
	intptr_t ret;
	int nopts;
	interp_t interp;
	char **new_argv;
	int argc = -1;
	char *fullpath = NULL;

	if (!parse_interpreter(prog, &interp))
		return spawnveq(mode, prog, argv, envp);

	nopts = interp.opts != NULL;
	while (argv[++argc])
		;

	new_argv = xmalloc(sizeof(*argv)*(argc+nopts+2));
	new_argv[1] = interp.opts;
	new_argv[nopts+1] = (char *)prog; /* pass absolute path */
	memcpy(new_argv+nopts+2, argv+1, sizeof(*argv)*argc);

	if ((fullpath=add_win32_extension(interp.path)) != NULL ||
			file_is_executable(interp.path)) {
		new_argv[0] = fullpath ? fullpath : interp.path;
		ret = spawnveq(mode, new_argv[0], new_argv, envp);
	} else
#if ENABLE_FEATURE_PREFER_APPLETS || ENABLE_FEATURE_SH_STANDALONE
	if (find_applet_by_name(interp.name) >= 0) {
		new_argv[0] = interp.name;
		ret = mingw_spawn_applet(mode, new_argv, envp);
	} else
#endif
	if ((fullpath=find_first_executable(interp.name)) != NULL) {
		new_argv[0] = fullpath;
		ret = spawnveq(mode, fullpath, new_argv, envp);
	}
	else {
		errno = ENOENT;
		ret = -1;
	}

	free(fullpath);
	free(new_argv);
	return ret;
}

static intptr_t
mingw_spawn_1(int mode, const char *cmd, char *const *argv, char *const *envp)
{
	char *prog;

#if ENABLE_FEATURE_PREFER_APPLETS || ENABLE_FEATURE_SH_STANDALONE
	if (find_applet_by_name(cmd) >= 0)
		return mingw_spawn_applet(mode, argv, envp);
	else
#endif
	if (strchr(cmd, '/') || strchr(cmd, '\\')) {
		return mingw_spawn_interpreter(mode, cmd, argv, envp);
	}
	else if ((prog=find_first_executable(cmd)) != NULL) {
		intptr_t ret = mingw_spawn_interpreter(mode, prog, argv, envp);
		free(prog);
		return ret;
	}

	errno = ENOENT;
	return -1;
}

pid_t FAST_FUNC
mingw_spawn(char **argv)
{
	intptr_t ret;

	ret = mingw_spawn_1(P_NOWAIT, argv[0], (char *const *)argv, environ);

	return ret == -1 ? -1 : GetProcessId((HANDLE)ret);
}

intptr_t FAST_FUNC
mingw_spawn_proc(const char **argv)
{
	return mingw_spawn_1(P_NOWAIT, argv[0], (char *const *)argv, environ);
}

int
mingw_execvp(const char *cmd, char *const *argv)
{
	int ret = (int)mingw_spawn_1(P_WAIT, cmd, argv, environ);
	if (ret != -1)
		exit(ret);
	return ret;
}

int
mingw_execve(const char *cmd, char *const *argv, char *const *envp)
{
	int ret = (int)mingw_spawn_interpreter(P_WAIT, cmd, argv, envp);
	if (ret != -1)
		exit(ret);
	return ret;
}

int
mingw_execv(const char *cmd, char *const *argv)
{
	return mingw_execve(cmd, argv, environ);
}

static inline long long filetime_to_ticks(const FILETIME *ft)
{
	return (((long long)ft->dwHighDateTime << 32) + ft->dwLowDateTime)/
				HNSEC_PER_TICK;
}

/* POSIX version in libbb/procps.c */
procps_status_t* FAST_FUNC procps_scan(procps_status_t* sp, int flags
#if !ENABLE_FEATURE_PS_TIME && !ENABLE_FEATURE_PS_LONG
UNUSED_PARAM
#endif
)
{
	PROCESSENTRY32 pe;

	pe.dwSize = sizeof(pe);
	if (!sp) {
		sp = xzalloc(sizeof(struct procps_status_t));
		sp->snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
		if (sp->snapshot == INVALID_HANDLE_VALUE) {
			free(sp);
			return NULL;
		}
		if (!Process32First(sp->snapshot, &pe)) {
			CloseHandle(sp->snapshot);
			free(sp);
			return NULL;
		}
	}
	else {
		if (!Process32Next(sp->snapshot, &pe)) {
			CloseHandle(sp->snapshot);
			free(sp);
			return NULL;
		}
	}

#if ENABLE_FEATURE_PS_TIME || ENABLE_FEATURE_PS_LONG
	if (flags & (PSSCAN_STIME|PSSCAN_UTIME|PSSCAN_START_TIME)) {
		HANDLE proc;
		FILETIME crTime, exTime, keTime, usTime;

		if ((proc=OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,
					FALSE, pe.th32ProcessID))) {
			if (GetProcessTimes(proc, &crTime, &exTime, &keTime, &usTime)) {
				/* times in ticks since 1 January 1601 */
				static long long boot_time = 0;
				long long start_time;

				if (boot_time == 0) {
					long long ticks_since_boot;
					FILETIME now;

					ticks_since_boot = GetTickCount64()/MS_PER_TICK;
					GetSystemTimeAsFileTime(&now);
					boot_time = filetime_to_ticks(&now) - ticks_since_boot;
				}

				start_time = filetime_to_ticks(&crTime);
				sp->start_time = (unsigned long)(start_time - boot_time);

				sp->stime = (unsigned long)filetime_to_ticks(&keTime);
				sp->utime = (unsigned long)filetime_to_ticks(&usTime);
			}
			else {
				sp->start_time = sp->stime = sp->utime = 0;
			}
			CloseHandle(proc);
		}
	}
#endif

	sp->pid = pe.th32ProcessID;
	sp->ppid = pe.th32ParentProcessID;
	safe_strncpy(sp->comm, pe.szExeFile, COMM_LEN);
	return sp;
}

/**
 * If the process ID is positive invoke the callback for that process
 * only.  If negative or zero invoke the callback for all descendants
 * of the indicated process.  Zero indicates the current process; negative
 * indicates the process with process ID -pid.
 */
typedef int (*kill_callback)(pid_t pid, int exit_code);

static int kill_pids(pid_t pid, int exit_code, kill_callback killer)
{
	DWORD pids[16384];
	int max_len = sizeof(pids) / sizeof(*pids), i, len, ret = 0;

	if(pid > 0)
		pids[0] = (DWORD)pid;
	else if (pid == 0)
		pids[0] = (DWORD)getpid();
	else
		pids[0] = (DWORD)-pid;
	len = 1;

	/*
	 * Even if Process32First()/Process32Next() seem to traverse the
	 * processes in topological order (i.e. parent processes before
	 * child processes), there is nothing in the Win32 API documentation
	 * suggesting that this is guaranteed.
	 *
	 * Therefore, run through them at least twice and stop when no more
	 * process IDs were added to the list.
	 */
	if (pid <= 0) {
		HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);

		if (snapshot == INVALID_HANDLE_VALUE) {
			errno = err_win_to_posix(GetLastError());
			return -1;
		}

		for (;;) {
			PROCESSENTRY32 entry;
			int orig_len = len;

			memset(&entry, 0, sizeof(entry));
			entry.dwSize = sizeof(entry);

			if (!Process32First(snapshot, &entry))
				break;

			do {
				for (i = len - 1; i >= 0; i--) {
					if (pids[i] == entry.th32ProcessID)
						break;
					if (pids[i] == entry.th32ParentProcessID)
						pids[len++] = entry.th32ProcessID;
				}
			} while (len < max_len && Process32Next(snapshot, &entry));

			if (orig_len == len || len >= max_len)
				break;
		}

		CloseHandle(snapshot);
	}

	for (i = len - 1; i >= 0; i--) {
		if (killer(pids[i], exit_code)) {
			errno = err_win_to_posix(GetLastError());
			ret = -1;
		}
	}

	return ret;
}

/**
 * Determine whether a process runs in the same architecture as the current
 * one. That test is required before we assume that GetProcAddress() returns
 * a valid address *for the target process*.
 */
static inline int process_architecture_matches_current(HANDLE process)
{
	static BOOL current_is_wow = -1;
	BOOL is_wow;

	if (current_is_wow == -1 &&
	    !IsWow64Process (GetCurrentProcess(), &current_is_wow))
		current_is_wow = -2;
	if (current_is_wow == -2)
		return 0; /* could not determine current process' WoW-ness */
	if (!IsWow64Process (process, &is_wow))
		return 0; /* cannot determine */
	return is_wow == current_is_wow;
}

/**
 * This function tries to terminate a Win32 process, as gently as possible,
 * by injecting a thread that calls ExitProcess().
 *
 * Note: as kernel32.dll is loaded before any process, the other process and
 * this process will have ExitProcess() at the same address.
 *
 * The idea comes from the Dr Dobb's article "A Safer Alternative to
 * TerminateProcess()" by Andrew Tucker (July 1, 1999),
 * http://www.drdobbs.com/a-safer-alternative-to-terminateprocess/184416547
 *
 */
static int exit_process(pid_t pid, int exit_code)
{
	HANDLE process;
	DWORD code;
	int ret = 0;

	if (!(process = OpenProcess(SYNCHRONIZE | PROCESS_CREATE_THREAD |
			PROCESS_QUERY_INFORMATION |
			PROCESS_VM_OPERATION | PROCESS_VM_WRITE |
			PROCESS_VM_READ, FALSE, pid))) {
		return -1;
	}

	if (GetExitCodeProcess(process, &code) && code == STILL_ACTIVE) {
		static int initialized;
		static LPTHREAD_START_ROUTINE exit_process_address;
		PVOID arg = (PVOID)(intptr_t)exit_code;
		DWORD thread_id;
		HANDLE thread;

		if (!initialized) {
			HINSTANCE kernel32 = GetModuleHandle("kernel32");
			if (!kernel32) {
				fprintf(stderr, "BUG: cannot find kernel32");
				ret = -1;
				goto finish;
			}
			exit_process_address = (LPTHREAD_START_ROUTINE)
				GetProcAddress(kernel32, "ExitProcess");
			initialized = 1;
		}
		if (!exit_process_address ||
		    !process_architecture_matches_current(process)) {
			ret = -1;
			goto finish;
		}

		if ((thread = CreateRemoteThread(process, NULL, 0,
					    exit_process_address,
					    arg, 0, &thread_id))) {
			CloseHandle(thread);
		}
	}

 finish:
	CloseHandle(process);
	return ret;
}

/*
 * This way of terminating processes is not gentle: they get no chance to
 * clean up after themselves (closing file handles, removing .lock files,
 * terminating spawned processes (if any), etc).
 */
static int terminate_process(pid_t pid, int exit_code)
{
	HANDLE process;
	int ret;

	if (!(process=OpenProcess(PROCESS_TERMINATE, FALSE, pid))) {
		return -1;
	}

	ret = !TerminateProcess(process, exit_code);
	CloseHandle(process);

	return ret;
}

static int test_process(pid_t pid, int exit_code UNUSED_PARAM)
{
	HANDLE process;

	if (!(process=OpenProcess(PROCESS_TERMINATE, FALSE, pid))) {
		return -1;
	}

	CloseHandle(process);
	return 0;
}

int kill(pid_t pid, int sig)
{
	if (sig == SIGTERM) {
		return kill_pids(pid, 128+sig, exit_process);
	}
	else if (sig == SIGKILL) {
		return kill_pids(pid, 128+sig, terminate_process);
	}
	else if (sig == 0) {
		return kill_pids(pid, 128+sig, test_process);
	}

	errno = EINVAL;
	return -1;
}
