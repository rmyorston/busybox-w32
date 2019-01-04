#ifndef LAZYLOAD_H
#define LAZYLOAD_H

/* simplify loading of DLL functions */

struct proc_addr {
	FARPROC pfunction;
	unsigned initialized;
};

/* Declares a function to be loaded dynamically from a DLL. */
#define DECLARE_PROC_ADDR(rettype, function, ...) \
	static struct proc_addr proc_addr_##function = { NULL, 0 }; \
	rettype (WINAPI *function)(__VA_ARGS__)

/*
 * Loads a function from a DLL (once-only).
 * Returns non-NULL function pointer on success.
 * Returns NULL and sets errno == ENOSYS on failure.
 */
#define INIT_PROC_ADDR(dll, function) \
	(function = get_proc_addr(#dll, #function, &proc_addr_##function))

static inline void *get_proc_addr(const char *dll, const char *function, struct proc_addr *proc)
{
	/* only do this once */
	if (!proc->initialized) {
		HANDLE hnd = LoadLibraryExA(dll, NULL, LOAD_LIBRARY_SEARCH_SYSTEM32);
		if (hnd)
			proc->pfunction = GetProcAddress(hnd, function);
		proc->initialized = 1;
	}
	/* set ENOSYS if DLL or function was not found */
	if (!proc->pfunction)
		errno = ENOSYS;
	return proc->pfunction;
}

#endif
