#include "libbb.h"
#include <tlhelp32.h>
#include <psapi.h>
#include "lazyload.h"
#include "NUM_APPLETS.h"

pid_t waitpid(pid_t pid, int *status, int options)
#if ENABLE_TIME
{
	return mingw_wait3(pid, status, options, NULL);
}
#endif

#if ENABLE_TIME
pid_t mingw_wait3(pid_t pid, int *status, int options, struct rusage *rusage)
#endif
{
	HANDLE proc;
	DWORD code;

	/* Windows does not understand parent-child */
	if (pid > 0 && options == 0) {
		if ( (proc=OpenProcess(SYNCHRONIZE|PROCESS_QUERY_INFORMATION,
						FALSE, pid)) != NULL ) {
			WaitForSingleObject(proc, INFINITE);
			GetExitCodeProcess(proc, &code);
#if ENABLE_TIME
			if (rusage != NULL) {
				FILETIME crTime, exTime, keTime, usTime;

				memset(rusage, 0, sizeof(*rusage));
				if (GetProcessTimes(proc, &crTime, &exTime, &keTime, &usTime)) {
					uint64_t kernel_usec =
						(((uint64_t)keTime.dwHighDateTime << 32)
							| (uint64_t)keTime.dwLowDateTime)/10;
					uint64_t user_usec =
						(((uint64_t)usTime.dwHighDateTime << 32)
							| (uint64_t)usTime.dwLowDateTime)/10;

					rusage->ru_utime.tv_sec = user_usec / 1000000U;
					rusage->ru_utime.tv_usec = user_usec % 1000000U;
					rusage->ru_stime.tv_sec = kernel_usec / 1000000U;
					rusage->ru_stime.tv_usec = kernel_usec % 1000000U;
				}
			}
#endif
			CloseHandle(proc);
			*status = exit_code_to_wait_status(code);
			return pid;
		}
	}
	errno = pid < 0 ? ENOSYS : EINVAL;
	return -1;
}

int FAST_FUNC
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
		 * relating to '#!'.  See also https://lwn.net/Articles/630727/
		 * for Linux-specific details.
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
 * See https://docs.microsoft.com/en-us/cpp/cpp/main-function-command-line-args?view=vs-2019#parsing-c-command-line-arguments
 * (Parsing C++ Command-Line Arguments)
 */
char * FAST_FUNC
quote_arg(const char *arg)
{
	char *d, *r = xmalloc(2 * strlen(arg) + 3);	// max-esc, quotes, \0
	size_t nbs = 0; // consecutive backslashes before current char
	int quoted = !*arg;

	for (d = r; *arg; *d++ = *arg++) {
		if (*arg == ' ' || *arg == '\t')
			quoted = 1;

		if (*arg == '\\' || *arg == '"')
			*d++ = '\\';
		else
			d -= nbs; // undo nbs escapes, if any (not followed by DQ)

		if (*arg == '\\')
			++nbs;
		else
			nbs = 0;
	}

	if (quoted) {
		memmove(r + 1, r, d++ - r);
		*r = *d++ = '"';
	} else {
		d -= nbs;
	}

	*d = 0;
	return r;
}

char * FAST_FUNC
find_first_executable(const char *name)
{
	const char *path = getenv("PATH");
	return find_executable(name, &path);
}

static intptr_t
spawnveq(int mode, const char *path, char *const *argv, char *const *env)
{
	char **new_argv;
	char *new_path = NULL;
	int i, argc;
	intptr_t ret;
	struct stat st;
	size_t len = 0;

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
	new_argv = xzalloc(sizeof(*argv)*(argc+1));
	for (i = 0; i < argc; i++) {
		new_argv[i] = quote_arg(argv[i]);
		len += strlen(new_argv[i]) + 1;
	}

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

	errno = 0;
	ret = spawnve(mode, new_path ? new_path : path, new_argv, env);
	if (errno == EINVAL && len > bb_arg_max())
		errno = E2BIG;

 done:
	for (i = 0;i < argc;i++)
		free(new_argv[i]);
	free(new_argv);
	free(new_path);

	return ret;
}

#if ENABLE_FEATURE_PREFER_APPLETS && NUM_APPLETS > 1
static intptr_t
mingw_spawn_applet(int mode,
		   char *const *argv,
		   char *const *envp)
{
	return spawnveq(mode, bb_busybox_exec_path, argv, envp);
}
#endif

/* Make a copy of an argv array with n extra slots at the start */
char ** FAST_FUNC
grow_argv(char **argv, int n)
{
	char **new_argv;
	int argc;

	argc = string_array_len(argv) + 1;
	new_argv = xmalloc(sizeof(*argv) * (argc + n));
	memcpy(new_argv + n, argv, sizeof(*argv) * argc);
	return new_argv;
}

#if ENABLE_FEATURE_HTTPD_CGI
static int
create_detached_process(const char *prog, char *const *argv)
{
	int argc, i;
	char *command = NULL;
	STARTUPINFO siStartInfo;
	PROCESS_INFORMATION piProcInfo;
	int success;

	argc = string_array_len((char **)argv);
	for (i = 0; i < argc; i++) {
		char *qarg = quote_arg(argv[i]);
		command = xappendword(command, qarg);
		if (ENABLE_FEATURE_CLEAN_UP)
			free(qarg);
	}

	ZeroMemory(&siStartInfo, sizeof(STARTUPINFO));
	siStartInfo.cb = sizeof(STARTUPINFO);
	siStartInfo.hStdInput = (HANDLE)_get_osfhandle(STDIN_FILENO);
	siStartInfo.hStdOutput = (HANDLE)_get_osfhandle(STDOUT_FILENO);
	siStartInfo.dwFlags = STARTF_USESTDHANDLES;

	success = CreateProcess((LPCSTR)prog,
				(LPSTR)command,    /* command line */
				NULL,              /* process security attributes */
				NULL,              /* primary thread security attributes */
				TRUE,              /* handles are inherited */
				CREATE_NO_WINDOW,  /* creation flags */
				NULL,              /* use parent's environment */
				NULL,              /* use parent's current directory */
				&siStartInfo,      /* STARTUPINFO pointer */
				&piProcInfo);      /* receives PROCESS_INFORMATION */

	if (ENABLE_FEATURE_CLEAN_UP)
		free(command);

	if (!success)
		return -1;
	exit(0);
}

# define SPAWNVEQ(m, p, a, e) \
	((m != HTTPD_DETACH) ? spawnveq(m, p, a, e) : \
		create_detached_process(p, a))
#else
# define SPAWNVEQ(m, p, a, e) spawnveq(m, p, a, e)
#endif

static intptr_t
mingw_spawn_interpreter(int mode, const char *prog, char *const *argv,
			char *const *envp, int level)
{
	intptr_t ret = -1;
	int nopts;
	interp_t interp;
	char **new_argv;
	char *path = NULL;
	int is_unix_path;

	if (!parse_interpreter(prog, &interp))
		return SPAWNVEQ(mode, prog, argv, envp);

	if (++level > 4) {
		errno = ELOOP;
		return -1;
	}

	nopts = interp.opts != NULL;
	new_argv = grow_argv((char **)(argv + 1), nopts + 2);
	new_argv[1] = interp.opts;
	new_argv[nopts+1] = (char *)prog; /* pass absolute path */

	is_unix_path = unix_path(interp.path);
#if ENABLE_FEATURE_PREFER_APPLETS && NUM_APPLETS > 1
	if (is_unix_path && find_applet_by_name(interp.name) >= 0) {
		/* the fake path indicates the index of the script */
		new_argv[0] = path = xasprintf("%d:/%s", nopts+1, interp.name);
		ret = SPAWNVEQ(mode, bb_busybox_exec_path, new_argv, envp);
		goto done;
	}
#endif

	path = file_is_win32_exe(interp.path);
	if (!path && is_unix_path)
		path = find_first_executable(interp.name);

	if (path) {
		new_argv[0] = path;
		ret = mingw_spawn_interpreter(mode, path, new_argv, envp, level);
	} else {
		errno = ENOENT;
	}
#if ENABLE_FEATURE_PREFER_APPLETS && NUM_APPLETS > 1
 done:
#endif
	free(path);
	free(new_argv);
	return ret;
}

static intptr_t
mingw_spawnvp(int mode, const char *cmd, char *const *argv)
{
	char *path;
	intptr_t ret;

#if ENABLE_FEATURE_PREFER_APPLETS && NUM_APPLETS > 1
	if ((!has_path(cmd) || unix_path(cmd)) &&
			find_applet_by_name(bb_basename(cmd)) >= 0)
		return mingw_spawn_applet(mode, argv, NULL);
#endif
	if (has_path(cmd)) {
		path = file_is_win32_exe(cmd);
		if (path) {
			ret = mingw_spawn_interpreter(mode, path, argv, NULL, 0);
			free(path);
			return ret;
		}
		if (unix_path(cmd))
			cmd = bb_basename(cmd);
	}

	if (!has_path(cmd) && (path = find_first_executable(cmd)) != NULL) {
		ret = mingw_spawn_interpreter(mode, path, argv, NULL, 0);
		free(path);
		return ret;
	}

	errno = ENOENT;
	return -1;
}

pid_t FAST_FUNC
mingw_spawn(char **argv)
{
	intptr_t ret;

	ret = mingw_spawnvp(P_NOWAIT, argv[0], (char *const *)argv);

	return ret == -1 ? (pid_t)-1 : (pid_t)GetProcessId((HANDLE)ret);
}

intptr_t FAST_FUNC
mingw_spawn_detach(char **argv)
{
	return mingw_spawnvp(P_DETACH, argv[0], argv);
}

intptr_t FAST_FUNC
mingw_spawn_proc(const char **argv)
{
	return mingw_spawnvp(P_NOWAIT, argv[0], (char *const *)argv);
}

BOOL WINAPI kill_child_ctrl_handler(DWORD dwCtrlType)
{
	static pid_t child_pid = 0;
	DWORD dummy, *procs, count, rcount, i;
	DECLARE_PROC_ADDR(DWORD, GetConsoleProcessList, LPDWORD, DWORD);

	if (child_pid == 0) {
		// First call sets child pid
		child_pid = dwCtrlType;
		return FALSE;
	}

	if (dwCtrlType == CTRL_C_EVENT || dwCtrlType == CTRL_BREAK_EVENT) {
		if (!INIT_PROC_ADDR(kernel32.dll, GetConsoleProcessList))
			return TRUE;

		count = GetConsoleProcessList(&dummy, 1) + 16;
		procs = malloc(sizeof(DWORD) * count);
		rcount = GetConsoleProcessList(procs, count);
		if (rcount != 0 && rcount <= count) {
			for (i = 0; i < rcount; i++) {
				if (procs[i] == child_pid) {
					// Child is attached to our console
					break;
				}
			}
			if (i == rcount) {
				// Kill non-console child; console children can
				// handle Ctrl-C as they see fit.
				kill(-child_pid, SIGINT);
			}
		}
		free(procs);
		return TRUE;
	}
	return FALSE;
}

static int exit_code_to_wait_status_cmd(DWORD exit_code, const char *cmd)
{
	int sig, status;
	DECLARE_PROC_ADDR(ULONG, RtlNtStatusToDosError, NTSTATUS);
	DWORD flags, code;
	char *msg = NULL;
	const char *sep = ": ";

	if (exit_code == 0xc0000005)
		return SIGSEGV;
	else if (exit_code == 0xc000013a)
		return SIGINT;

	// When a process is terminated as if by a signal the Windows
	// exit code is zero apart from the signal in its topmost byte.
	// This is a busybox-w32 convention.
	sig = exit_code >> 24;
	if (sig != 0 && exit_code == sig << 24 && is_valid_signal(sig))
		return sig;

	// The exit code may be an NTSTATUS code.  Try to obtain a
	// descriptive message for it.
	if (exit_code > 0xff) {
		flags = FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM;
		if (INIT_PROC_ADDR(ntdll.dll, RtlNtStatusToDosError)) {
			code = RtlNtStatusToDosError(exit_code);
			if (FormatMessage(flags, NULL, code, 0, (char *)&msg, 0, NULL)) {
				char *cr = strrchr(msg, '\r');
				if (cr) {		// Replace CRLF with a space
					cr[0] = ' ';
					cr[1] = '\0';
				}
			}
		}

		if (!cmd)
			cmd = sep = "";
		bb_error_msg("%s%s%sError 0x%lx", cmd, sep, msg ?: "", exit_code);
		LocalFree(msg);
	}

	// Use least significant byte as exit code, but not if it's zero
	// and the Windows exit code as a whole is non-zero.
	status = exit_code & 0xff;
	if (exit_code != 0 && status == 0)
		status = 255;
	return status << 8;
}

static NORETURN void wait_for_child(HANDLE child, const char *cmd)
{
	DWORD code;
	int status;

	if (getppid() == 1)
		exit(0);

	kill_child_ctrl_handler(GetProcessId(child));
	SetConsoleCtrlHandler(kill_child_ctrl_handler, TRUE);
	WaitForSingleObject(child, INFINITE);
	GetExitCodeProcess(child, &code);
	// We don't need the wait status, but get it anyway so the error
	// message can include the command.  In such cases we pass the
	// exit status to exit() so our caller won't repeat the message.
	status = exit_code_to_wait_status_cmd(code, cmd);
	if (!WIFSIGNALED(status) && code > 0xff)
		code = WEXITSTATUS(status);
	exit((int)code);
}

static intptr_t
shell_execute(const char *path, char *const *argv)
{
	SHELLEXECUTEINFO info;
	char *args;

	memset(&info, 0, sizeof(SHELLEXECUTEINFO));
	info.cbSize = sizeof(SHELLEXECUTEINFO);
	info.fMask = SEE_MASK_NOCLOSEPROCESS;
	/* info.hwnd = NULL; */
	info.lpVerb = "runas";
	info.lpFile = path;

	args = NULL;
	if (*argv++) {
		while (*argv) {
			char *q = quote_arg(*argv++);
			args = xappendword(args, q);
			free(q);
		}
	}

	info.lpParameters = args;
	/* info.lpDirectory = NULL; */
	info.nShow = SW_SHOWNORMAL;

	mingw_shell_execute(&info);

	free(args);
	return info.hProcess ? (intptr_t)info.hProcess : -1;
}

int
mingw_execvp(const char *cmd, char *const *argv)
{
	intptr_t ret = mingw_spawnvp(P_NOWAIT, cmd, argv);
	if (ret != -1)
		wait_for_child((HANDLE)ret, cmd);
	return ret;
}

int
mingw_execve(const char *cmd, char *const *argv, char *const *envp)
{
	intptr_t ret = mingw_spawn_interpreter(P_NOWAIT, cmd, argv, envp, 0);

	if (ret == -1 && GetLastError() == ERROR_ELEVATION_REQUIRED) {
		// Command exists but failed because it wants elevated privileges.
		// Try again using ShellExecuteEx().
		SetLastError(0);
		ret = shell_execute(cmd, argv);
		if (GetLastError())
			exit(1);
	}

	if (ret != -1)
		wait_for_child((HANDLE)ret, cmd);
	return ret;
}

int
mingw_execv(const char *cmd, char *const *argv)
{
	return mingw_execve(cmd, argv, NULL);
}

#if ENABLE_FEATURE_HTTPD_CGI
int httpd_execv_detach(const char *script, char *const *argv)
{
	intptr_t ret = mingw_spawn_interpreter(HTTPD_DETACH, script,
							(char *const *)argv, NULL, 0);
	if (ret != -1)
		exit(0);
	return ret;
}
#endif

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
	DECLARE_PROC_ADDR(DWORD, GetProcessImageFileNameA, HANDLE,
							LPSTR, DWORD);
	DECLARE_PROC_ADDR(BOOL, EnumProcessModules, HANDLE, HMODULE *,
							DWORD, LPDWORD);
	DECLARE_PROC_ADDR(DWORD, GetModuleFileNameExA, HANDLE, HMODULE,
							LPSTR, DWORD);

	if (!INIT_PROC_ADDR(psapi.dll, GetProcessImageFileNameA) ||
			!INIT_PROC_ADDR(psapi.dll, EnumProcessModules) ||
			!INIT_PROC_ADDR(psapi.dll, GetModuleFileNameExA))
		return NULL;

	if (!(proc=OpenProcess(PROCESS_QUERY_INFORMATION|PROCESS_VM_READ,
							FALSE, pid))) {
		return NULL;
	}

	if (exe == NULL) {
		if (GetProcessImageFileNameA(proc, exepath, PATH_MAX) != 0) {
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
		if (GetModuleFileNameExA(proc, mlist[i], modname, sizeof(modname))) {
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

pid_t getppid(void)
{
	procps_status_t *sp = NULL;
	int my_pid = getpid();

	while ((sp = procps_scan(sp, 0)) != NULL) {
		if (sp->pid == my_pid) {
			return sp->ppid;
		}
	}
	return 1;
}

#define NPIDS 128

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
		if (Process32First(sp->snapshot, &pe)) {
			int maxpids = 0;
			do {
				if (sp->npids == maxpids) {
					maxpids += NPIDS;
					sp->pids = xrealloc(sp->pids, sizeof(DWORD) * maxpids);
				}
				sp->pids[sp->npids++] = pe.th32ProcessID;
			} while (Process32Next(sp->snapshot, &pe));
		}
		ret = Process32First(sp->snapshot, &pe);
	}
	else {
		ret = Process32Next(sp->snapshot, &pe);
	}

	if (!ret) {
		CloseHandle(sp->snapshot);
		free(sp->pids);
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

	/* The parent of PID 0 is 0.  If the parent is a PID we haven't
	 * seen set PPID to 1. */
	sp->ppid = pe.th32ProcessID != 0;
	for (int i = 0; i < sp->npids; ++i) {
		if (sp->pids[i] == pe.th32ParentProcessID) {
			sp->ppid = pe.th32ParentProcessID;
			break;
		}
	}
	sp->pid = pe.th32ProcessID;

	if (flags & PSSCAN_COMM) {
		if (sp->pid == getpid()) {
			comm = applet_name;
		}
		else if ((name=get_bb_string(sp->pid, pe.szExeFile, bb_comm)) != NULL) {
			comm = name;
		}
		else {
			comm = pe.szExeFile;
		}
		safe_strncpy(sp->comm, comm, COMM_LEN);
	}

	return sp;
}

int FAST_FUNC read_cmdline(char *buf, int col, unsigned pid, const char *comm)
{
	const char *str, *cmdline;

	*buf = '\0';
	if (pid == getpid())
		cmdline = bb_command_line;
	else if ((str=get_bb_string(pid, NULL, bb_command_line)) != NULL)
		cmdline = str;
	else
		cmdline = comm;
	safe_strncpy(buf, cmdline, col);
	return 0;
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
static int kill_signal_by_handle(HANDLE process, int sig)
{
	DECLARE_PROC_ADDR(DWORD, ExitProcess, LPVOID);
	PVOID arg = (PVOID)(intptr_t)(sig << 24);
	DWORD thread_id;
	HANDLE thread;

	if (!INIT_PROC_ADDR(kernel32, ExitProcess) ||
			!process_architecture_matches_current(process)) {
		SetLastError(ERROR_ACCESS_DENIED);
		return -1;
	}

	if (sig != 0 && (thread = CreateRemoteThread(process, NULL, 0,
					   ExitProcess, arg, 0, &thread_id))) {
		CloseHandle(thread);
	}
	return 0;
}

static int kill_signal(pid_t pid, int sig)
{
	HANDLE process;
	int ret = 0;
	DWORD code, flags;

	if (sig == SIGKILL)
		flags = PROCESS_TERMINATE | PROCESS_QUERY_INFORMATION;
	else
		flags = SYNCHRONIZE | PROCESS_CREATE_THREAD |
					PROCESS_QUERY_INFORMATION |
					PROCESS_VM_OPERATION | PROCESS_VM_WRITE |
					PROCESS_VM_READ;
	process = OpenProcess(flags, FALSE, pid);

	if (!process)
		return -1;

	if (!GetExitCodeProcess(process, &code) || code != STILL_ACTIVE) {
		SetLastError(ERROR_INVALID_PARAMETER);
		ret = -1;
	} else if (sig == SIGKILL) {
		/* This way of terminating processes is not gentle: they get no
		 * chance to clean up after themselves (closing file handles,
		 * removing .lock files, terminating spawned processes (if any),
		 * etc). */
		ret = !TerminateProcess(process, SIGKILL << 24);
	} else {
		ret = kill_signal_by_handle(process, sig);
	}
	CloseHandle(process);

	return ret;
}

/**
 * If the process ID is positive signal that process only.  If negative
 * or zero signal all descendants of the indicated process.  Zero
 * indicates the current process; negative indicates the process with
 * process ID -pid.
 */
int kill(pid_t pid, int sig)
{
	DWORD *pids;
	int max_len, i, len, ret = 0;

	if (!is_valid_signal(sig)) {
		errno = EINVAL;
		return -1;
	}

	max_len = NPIDS;
	pids = xmalloc(sizeof(*pids) * max_len);

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
		PROCESSENTRY32 entry;
		int pid_added;

		if (snapshot == INVALID_HANDLE_VALUE) {
			errno = err_win_to_posix();
			free(pids);
			return -1;
		}

		entry.dwSize = sizeof(entry);
		pid_added = TRUE;
		while (pid_added && Process32First(snapshot, &entry)) {
			pid_added = FALSE;

			do {
				for (i = len - 1; i >= 0; i--) {
					if (pids[i] == entry.th32ProcessID)
						break;
					if (pids[i] == entry.th32ParentProcessID) {
						if (len == max_len) {
							max_len += NPIDS;
							pids = xrealloc(pids, sizeof(*pids) * max_len);
						}
						pids[len++] = entry.th32ProcessID;
						pid_added = TRUE;
					}
				}
			} while (Process32Next(snapshot, &entry));
		}

		CloseHandle(snapshot);
	}

	for (i = len - 1; i >= 0; i--) {
		SetLastError(0);
		if (kill_signal(pids[i], sig)) {
			errno = err_win_to_posix();
			ret = -1;
		}
	}
	free(pids);

	return ret;
}

int FAST_FUNC is_valid_signal(int number)
{
	return isalpha(*get_signame(number));
}

int exit_code_to_wait_status(DWORD exit_code)
{
	return exit_code_to_wait_status_cmd(exit_code, NULL);
}

int exit_code_to_posix(DWORD exit_code)
{
	int status = exit_code_to_wait_status(exit_code);

	if (WIFSIGNALED(status))
		return 128 + WTERMSIG(status);
	return WEXITSTATUS(status);
}
