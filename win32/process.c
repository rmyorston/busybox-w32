#include "libbb.h"
#include <tlhelp32.h>
#include <psapi.h>
#include "lazyload.h"

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
			if (ret == -1) {
				return -1;
			}
			*status <<= 8;
			return pid;
		}
	}
	errno = pid < 0 ? ENOSYS : EINVAL;
	return -1;
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
	int n;

	while (TRUE) {
		n = open_read_close(cmd, interp->buf, sizeof(interp->buf)-1);
		if (n < 4)	/* at least '#!/x' and not error */
			break;

		/*
		 * See http://www.in-ulm.de/~mascheck/various/shebang/ for trivia
		 * relating to '#!'.
		 */
		if (interp->buf[0] != '#' || interp->buf[1] != '!')
			break;
		interp->buf[n] = '\0';
		if ((t=strchr(interp->buf, '\n')) == NULL)
			break;
		t[1] = '\0';

		if ((path=strtok(interp->buf+2, " \t\r\n")) == NULL)
			break;

		t = (char *)bb_basename(path);
		if (*t == '\0')
			break;

		interp->path = path;
		interp->name = t;
		interp->opts = strtok(NULL, "\r\n");
		/* Trim leading and trailing whitespace from the options.
		 * If the resulting string is empty return a NULL pointer. */
		if (interp->opts && trim(interp->opts) == interp->opts)
			interp->opts = NULL;
		return 1;
	}

	if (n >= 0 && is_suffixed_with_case(cmd, ".sh")) {
		interp->path = (char *)DEFAULT_SHELL;
		interp->name = (char *)DEFAULT_SHELL_SHORT_NAME;
		interp->opts = NULL;
		return 1;
	}
	return 0;
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
	int i, argc;
	intptr_t ret;
	struct stat st;

	/*
	 * Require that the file exists, is a regular file and is executable.
	 * It may still contain garbage but we let spawnve deal with that.
	 */
	if (stat(path, &st) == 0) {
		if (!S_ISREG(st.st_mode) || !(st.st_mode&S_IXUSR)) {
			errno = EACCES;
			return -1;
		}
	}
	else {
		return -1;
	}

	argc = string_array_len((char **)argv);
	new_argv = xmalloc(sizeof(*argv)*(argc+1));
	for (i = 0;i < argc;i++)
		new_argv[i] = quote_arg(argv[i]);
	new_argv[argc] = NULL;

	/* Special case:  spawnve won't execute a batch file if the first
	 * argument is a relative path containing forward slashes.  Absolute
	 * paths are fine but there's no harm in converting them too. */
	if (has_bat_suffix(path)) {
		slash_to_bs(new_argv[0]);

		/* Another special case:  spawnve returns ENOEXEC when passed an
		 * empty batch file.  Pretend it worked. */
		if (st.st_size == 0) {
			ret = 0;
			goto done;
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

 done:
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
	int argc;
	char *fullpath = NULL;

	if (!parse_interpreter(prog, &interp))
		return spawnveq(mode, prog, argv, envp);

	nopts = interp.opts != NULL;
	argc = string_array_len((char **)argv);
	new_argv = xmalloc(sizeof(*argv)*(argc+nopts+2));
	new_argv[1] = interp.opts;
	new_argv[nopts+1] = (char *)prog; /* pass absolute path */
	memcpy(new_argv+nopts+2, argv+1, sizeof(*argv)*argc);

	if ((fullpath=alloc_win32_extension(interp.path)) != NULL ||
			file_is_executable(interp.path)) {
		new_argv[0] = fullpath ? fullpath : interp.path;
		ret = spawnveq(mode, new_argv[0], new_argv, envp);
	} else
#if ENABLE_FEATURE_PREFER_APPLETS || ENABLE_FEATURE_SH_STANDALONE
	if (find_applet_by_name(interp.name) >= 0) {
		/* the fake path indicates the index of the script */
		new_argv[0] = fullpath = xasprintf("%d:/%s", nopts+1, interp.name);
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

	ret = mingw_spawn_proc((const char **)argv);

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

/*
 * Attempt to get a string from another instance of busybox.exe.
 * This will only work if the other process is using the same binary
 * as the current process.  If anything goes wrong just give up.
 */
static char *get_bb_string(DWORD pid, const char *exe, char *string)
{
	HANDLE proc;
	HMODULE mlist[32];
	DWORD needed;
	void *address;
	char *my_base;
	char buffer[128];
	char exepath[PATH_MAX];
	char *name = NULL;
	int i;

	if (!(proc=OpenProcess(PROCESS_QUERY_INFORMATION|PROCESS_VM_READ,
							FALSE, pid))) {
		return NULL;
	}

	if (exe == NULL) {
		if (GetProcessImageFileName(proc, exepath, PATH_MAX) != 0) {
			exe = bb_basename(exepath);
		}
	}

	/*
	 * Search for the module that matches the name of the executable.
	 * The values returned in mlist are actually the base address of
	 * the module in the other process (as noted in the documentation
	 * for the MODULEINFO structure).
	 */
	if (!EnumProcessModules(proc, mlist, sizeof(mlist), &needed)) {
		goto finish;
	}

	for (i=0; exe != NULL && i<needed/sizeof(HMODULE); ++i) {
		char modname[MAX_PATH];
		if (GetModuleFileNameEx(proc, mlist[i], modname, sizeof(modname))) {
			if (strcasecmp(bb_basename(modname), exe) == 0) {
				break;
			}
		}
	}

	if (i == needed/sizeof(HMODULE)) {
		goto finish;
	}

	/* attempt to read the BusyBox version string */
	my_base = (char *)GetModuleHandle(NULL);
	address = (char *)mlist[i] + ((char *)bb_banner - my_base);
	if (!ReadProcessMemory(proc, address, buffer, 128, NULL)) {
		goto finish;
	}

	if (memcmp(buffer, bb_banner, strlen(bb_banner)) != 0) {
		/* version mismatch (or not BusyBox at all) */
		goto finish;
	}

	/* attempt to read the required string */
	address = (char *)mlist[i] + ((char *)string - my_base);
	if (!ReadProcessMemory(proc, address, buffer, 128, NULL)) {
		goto finish;
	}

	buffer[127] = '\0';
	name = auto_string(xstrdup(buffer));

 finish:
	CloseHandle(proc);
	return name;
}

/* POSIX version in libbb/procps.c */
procps_status_t* FAST_FUNC procps_scan(procps_status_t* sp, int flags
#if !ENABLE_FEATURE_PS_TIME && !ENABLE_FEATURE_PS_LONG
UNUSED_PARAM
#endif
)
{
	PROCESSENTRY32 pe;
	HANDLE proc;
	const char *comm, *name;
	BOOL ret;

	pe.dwSize = sizeof(pe);
	if (!sp) {
		sp = xzalloc(sizeof(struct procps_status_t));
		sp->snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
		if (sp->snapshot == INVALID_HANDLE_VALUE) {
			free(sp);
			return NULL;
		}
		ret = Process32First(sp->snapshot, &pe);
	}
	else {
		ret = Process32Next(sp->snapshot, &pe);
	}

	if (!ret) {
		CloseHandle(sp->snapshot);
		free(sp);
		return NULL;
	}

	memset(&sp->vsz, 0, sizeof(*sp) - offsetof(procps_status_t, vsz));
#if !ENABLE_DESKTOP
	strcpy(sp->state, "   ");
#endif

#if ENABLE_FEATURE_PS_TIME || ENABLE_FEATURE_PS_LONG
	if (flags & (PSSCAN_STIME|PSSCAN_UTIME|PSSCAN_START_TIME)) {
		FILETIME crTime, exTime, keTime, usTime;

		if ((proc=OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,
					FALSE, pe.th32ProcessID))) {
			if (GetProcessTimes(proc, &crTime, &exTime, &keTime, &usTime)) {
				long long ticks_since_boot, boot_time, create_time;
				FILETIME now;

				ticks_since_boot = GetTickCount64()/MS_PER_TICK;
				GetSystemTimeAsFileTime(&now);
				boot_time = filetime_to_ticks(&now) - ticks_since_boot;
				create_time = filetime_to_ticks(&crTime);

				sp->start_time = (unsigned long)(create_time - boot_time);
				sp->stime = (unsigned long)filetime_to_ticks(&keTime);
				sp->utime = (unsigned long)filetime_to_ticks(&usTime);
			}
			CloseHandle(proc);
		}
	}
#endif

	if (flags & PSSCAN_UIDGID) {
		/* if we can open the process it belongs to us */
		if ((proc=OpenProcess(PROCESS_ALL_ACCESS, FALSE, pe.th32ProcessID))) {
			sp->uid = DEFAULT_UID;
			sp->gid = DEFAULT_GID;
			CloseHandle(proc);
		}
	}

	sp->pid = pe.th32ProcessID;
	sp->ppid = pe.th32ParentProcessID;

	if (sp->pid == GetProcessId(GetCurrentProcess())) {
		comm = applet_name;
	}
	else if ((name=get_bb_string(sp->pid, pe.szExeFile, bb_comm)) != NULL) {
		comm = name;
	}
	else {
		comm = pe.szExeFile;
	}
	safe_strncpy(sp->comm, comm, COMM_LEN);

	return sp;
}

void FAST_FUNC read_cmdline(char *buf, int col, unsigned pid, const char *comm)
{
	const char *str, *cmdline;

	*buf = '\0';
	if (pid == GetProcessId(GetCurrentProcess()))
		cmdline = bb_command_line;
	else if ((str=get_bb_string(pid, NULL, bb_command_line)) != NULL)
		cmdline = str;
	else
		cmdline = comm;
	safe_strncpy(buf, cmdline, col);
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
			errno = err_win_to_posix();
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
		SetLastError(0);
		if (killer(pids[i], exit_code)) {
			errno = err_win_to_posix();
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
int kill_SIGTERM_by_handle(HANDLE process, int exit_code)
{
	DWORD code;
	int ret = 0;

	if (GetExitCodeProcess(process, &code) && code == STILL_ACTIVE) {
		DECLARE_PROC_ADDR(DWORD, ExitProcess, LPVOID);
		PVOID arg = (PVOID)(intptr_t)exit_code;
		DWORD thread_id;
		HANDLE thread;

		if (!INIT_PROC_ADDR(kernel32, ExitProcess) ||
				!process_architecture_matches_current(process)) {
			SetLastError(ERROR_ACCESS_DENIED);
			ret = -1;
			goto finish;
		}

		if ((thread = CreateRemoteThread(process, NULL, 0,
					    ExitProcess, arg, 0, &thread_id))) {
			CloseHandle(thread);
		}
	}

 finish:
	CloseHandle(process);
	return ret;
}

static int kill_SIGTERM(pid_t pid, int exit_code)
{
	HANDLE process;

	if (!(process = OpenProcess(SYNCHRONIZE | PROCESS_CREATE_THREAD |
			PROCESS_QUERY_INFORMATION |
			PROCESS_VM_OPERATION | PROCESS_VM_WRITE |
			PROCESS_VM_READ, FALSE, pid))) {
		return -1;
	}

	return kill_SIGTERM_by_handle(process, exit_code);
}

/*
 * This way of terminating processes is not gentle: they get no chance to
 * clean up after themselves (closing file handles, removing .lock files,
 * terminating spawned processes (if any), etc).
 */
static int kill_SIGKILL(pid_t pid, int exit_code)
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

static int kill_SIGTEST(pid_t pid, int exit_code UNUSED_PARAM)
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
		return kill_pids(pid, 128+sig, kill_SIGTERM);
	}
	else if (sig == SIGKILL) {
		return kill_pids(pid, 128+sig, kill_SIGKILL);
	}
	else if (sig == 0) {
		return kill_pids(pid, 128+sig, kill_SIGTEST);
	}

	errno = EINVAL;
	return -1;
}
