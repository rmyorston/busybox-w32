ssize_t write_in_full(int fd, const void *buf, size_t count);

#define alloc_nr(x) (((x)+16)*3/2)

#define ALLOC_GROW(x, nr, alloc) \
	do { \
		if ((nr) > alloc) { \
			if (alloc_nr(alloc) < (nr)) \
				alloc = (nr); \
			else \
				alloc = alloc_nr(alloc); \
			x = xrealloc((x), alloc * sizeof(*(x))); \
		} \
	} while(0)

static inline int is_absolute_path(const char *path)
{
	return path[0] == '/' || has_dos_drive_prefix(path);
}

#define NORETURN ATTRIBUTE_NORETURN
