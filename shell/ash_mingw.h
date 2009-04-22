struct forkshell {
	const char *fp;
	union node *n;
	int flags;
	struct child_process cmd;
	int fd;
};

static int forkshell_init(struct forkshell *fs);
static void forkshell_transfer(struct forkshell *fs);
static void forkshell_transfer_done(struct forkshell *fs);
static void forkshell_cleanup(struct forkshell *fs);
static int forkshell(const char *fp, union node *n, int flags);
static void subshell_run();
struct strlist;
static int set_exitstatus(int retval, const char **argv, int *out);
static int shellspawn(const char **argv, const char *path, int idx, struct strlist *varlist);

static int subash_fd = -1;
static char subash_entry[16];
