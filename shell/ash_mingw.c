extern void trace_printf(const char *format, ...);
extern void trace_argv_printf(const char **argv, const char *format, ...);

#define SER_TBLENTRY  -100
#define SER_VARTAB    -102
#define SER_LOCALVARS -103
#define SER_OPTLIST   -104
#define SER_FORK      -105
#define SER_DONE      -106
#define SER_MISC      -107

/* * * * * serialization routines * * * * */
static void node_send(int fd, union node *n);
static union node *node_recv(int fd);

#define SERDES_DEBUG 0

static inline void
ser_read(int fd, void *buf, size_t len)
{
	if (xread(fd, buf, len) != len)
		die("Corrupted");
}

static void
str_send(int fd, const char *s)
{
	int size = s ? strlen(s)+1 : -1;
	if (SERDES_DEBUG) fprintf(stderr," --> String(%d:%d %s)", size, s ? (int)(unsigned char)*s : 0,s ? s+1 : 0);
	write(fd, &size, sizeof(int));
	if (s)
		write(fd, s, size);
}

static char*
str_recv(int fd)
{
	int size;
	char *s;
	ser_read(fd, &size, sizeof(int));
	if (size == -1)
		s = NULL;
	else {
		s = ckmalloc(size);
		ser_read(fd, s, size);
	}
	if (SERDES_DEBUG) fprintf(stderr," <-- String(%d:%s)", size, s);
	return s;
}

static void
int_send(int fd, int n)
{
	write(fd, &n, sizeof(int));
	if (SERDES_DEBUG) fprintf(stderr," --> Int(%d)", n);
}

static int
int_recv(int fd)
{
	int n;
	ser_read(fd, &n, sizeof(int));
	if (SERDES_DEBUG) fprintf(stderr," <-- Int(%d)", n);
	return n;
}

static void
guard_send(int fd, int n)
{
	write(fd, &n, sizeof(int));
	if (SERDES_DEBUG) fprintf(stderr,"\n--> Guard(%d)\n", n);
}

static void
guard_recv1(int fd, int n1)
{
	int n;
	ser_read(fd, &n, sizeof(int));
	if (SERDES_DEBUG) fprintf(stderr,"\n<-- Guard1(%d,%d)\n", n, n1);
	if (n != n1)
		die("Corrupted");
}

static int
guard_recv2(int fd, int n1, int n2)
{
	int n;
	ser_read(fd, &n, sizeof(int));
	if (SERDES_DEBUG) fprintf(stderr,"\n<-- Guard2(%d,%d,%d)\n", n, n1,n2);
	if (n != n1 && n != n2)
		die("Corrupted");
	return n;
}

static void
buf_send(int fd, void *buf, int size)
{
	write(fd, buf, size);
	if (SERDES_DEBUG) fprintf(stderr," --> Buf(%d) ", size);
}

static void
buf_recv(int fd, void *buf, int size)
{
	ser_read(fd, buf, size);
	if (SERDES_DEBUG) fprintf(stderr," <-- Buf(%d) ", size);
}

static void
nodelist_send(int fd, struct nodelist *nl)
{
	int id;
	for (id = 0;nl;nl = nl->next) {
		guard_send(fd, id++);
		node_send(fd, nl->n);
	}
	guard_send(fd, -1);
}

static struct nodelist *
nodelist_recv(int fd)
{
	int id = 0;
	struct nodelist *head = NULL, *tail = NULL;
	union node *n;
	do {
		if (guard_recv2(fd, id++, -1) == -1)
			break;
		n = node_recv(fd);
		if (tail) {
			tail->next = ckmalloc(sizeof(struct nodelist));
			tail->next->next = NULL;
			tail->next->n = n;
			tail = tail->next;
		} else {
			head = tail = ckmalloc(sizeof(struct nodelist));
			tail->next = NULL;
			tail->n = n;
		}
	} while (1);
	return head;
}

static void
node_send(int fd, union node *n)
{
	int type = n ? n->type : -2;
	int size;

	int_send(fd, type);
	if (!n)
		return;

	switch (type) {
		case NCMD:
			size = sizeof(struct ncmd);
			break;
		case NPIPE:
			size = sizeof(struct npipe);
			break;
		case NREDIR:
		case NBACKGND:
		case NSUBSHELL:
			size = sizeof(struct nredir);
			break;
		case NAND:
		case NOR:
		case NSEMI:
		case NWHILE:
		case NUNTIL:
			size = sizeof(struct nbinary);
			break;
		case NIF:
			size = sizeof(struct nif);
			break;
		case NFOR:
			size = sizeof(struct nfor);
			break;
		case NCASE:
			size = sizeof(struct ncase);
			break;
		case NCLIST:
			size = sizeof(struct nclist);
			break;
		case NDEFUN:
		case NARG:
			size = sizeof(struct narg);
			break;
		case NTO:
		case NCLOBBER:
		case NFROM:
		case NFROMTO:
		case NAPPEND:
			size = sizeof(struct nfile);
			break;
		case NTOFD:
		case NFROMFD:
			size = sizeof(struct ndup);
			break;
		case NXHERE:
		case NHERE:
			size = sizeof(struct nhere);
			break;
		case NNOT:
			size = sizeof(struct nnot);
			break;
		default:
			die("Unknown construct type");
	}
	int_send(fd, size);

	switch (type) {
		case NCMD:
			node_send(fd, n->ncmd.assign);
			node_send(fd, n->ncmd.args);
			node_send(fd, n->ncmd.redirect);
			break;

		case NPIPE:
			int_send(fd, n->npipe.backgnd);
			nodelist_send(fd, n->npipe.cmdlist);
			break;

		case NREDIR:
		case NBACKGND:
		case NSUBSHELL:
			node_send(fd, n->nredir.n);
			node_send(fd, n->nredir.redirect);
			break;

		case NAND:
		case NOR:
		case NSEMI:
		case NWHILE:
		case NUNTIL:
			node_send(fd, n->nbinary.ch1);
			node_send(fd, n->nbinary.ch2);
			break;

		case NIF:
			node_send(fd, n->nif.test);
			node_send(fd, n->nif.ifpart);
			node_send(fd, n->nif.elsepart);
			break;

		case NFOR:
			node_send(fd, n->nfor.args);
			node_send(fd, n->nfor.body);
			str_send(fd, n->nfor.var);
			break;

		case NCASE:
			node_send(fd, n->ncase.expr);
			node_send(fd, n->ncase.cases);
			break;

		case NCLIST:
			node_send(fd, n->nclist.next);
			node_send(fd, n->nclist.pattern);
			node_send(fd, n->nclist.body);
			break;

		case NDEFUN:
		case NARG:
			node_send(fd, n->narg.next);
			str_send(fd, n->narg.text);
			nodelist_send(fd, n->narg.backquote);
			break;

		case NTO:
		case NCLOBBER:
		case NFROM:
		case NFROMTO:
		case NAPPEND:
			node_send(fd, n->nfile.next);
			int_send(fd, n->nfile.fd);
			node_send(fd, n->nfile.fname);
			//str_send(fd, n->nfile.expfname);
			break;

		case NTOFD:
		case NFROMFD:
			node_send(fd, n->ndup.next);
			int_send(fd, n->ndup.fd);
			int_send(fd, n->ndup.dupfd);
			node_send(fd, n->ndup.vname);
			break;

		case NXHERE:
		case NHERE:
			node_send(fd, n->nhere.next);
			int_send(fd, n->nhere.fd);
			node_send(fd, n->nhere.doc);
			break;

		case NNOT:
			node_send(fd, n->nnot.com);
			break;

		default:
			die("Unknown construct type");
	}
	guard_send(fd, -1);
}

static union node*
node_recv(int fd)
{
	int type;
	int size;
	union node *n;

	type = int_recv(fd);
	if (type == -2)
		return NULL;
	size = int_recv(fd);
	n = ckmalloc(size);
	n->type = type;

	switch (type) {
		case NCMD:
			n->ncmd.assign = node_recv(fd);
			n->ncmd.args = node_recv(fd);
			n->ncmd.redirect = node_recv(fd);
			break;

		case NPIPE:
			n->npipe.backgnd = int_recv(fd);
			n->npipe.cmdlist = nodelist_recv(fd);
			break;

		case NREDIR:
		case NBACKGND:
		case NSUBSHELL:
			n->nredir.n = node_recv(fd);
			n->nredir.redirect = node_recv(fd);
			break;

		case NAND:
		case NOR:
		case NSEMI:
		case NWHILE:
		case NUNTIL:
			n->nbinary.ch1 = node_recv(fd);
			n->nbinary.ch2 = node_recv(fd);
			break;

		case NIF:
			n->nif.test = node_recv(fd);
			n->nif.ifpart = node_recv(fd);
			n->nif.elsepart = node_recv(fd);
			break;

		case NFOR:
			n->nfor.args = node_recv(fd);
			n->nfor.body = node_recv(fd);
			n->nfor.var = str_recv(fd);
			break;

		case NCASE:
			n->ncase.expr = node_recv(fd);
			n->ncase.cases = node_recv(fd);
			break;

		case NCLIST:
			n->nclist.next = node_recv(fd);
			n->nclist.pattern = node_recv(fd);
			n->nclist.body = node_recv(fd);
			break;

		case NDEFUN:
		case NARG:
			n->narg.next = node_recv(fd);
			n->narg.text = str_recv(fd);
			n->narg.backquote = nodelist_recv(fd);
			break;

		case NTO:
		case NCLOBBER:
		case NFROM:
		case NFROMTO:
		case NAPPEND:
			n->nfile.next = node_recv(fd);
			n->nfile.fd = int_recv(fd);
			n->nfile.fname = node_recv(fd);
			//n->nfile.expfname = str_recv(fd);
			break;

		case NTOFD:
		case NFROMFD:
			n->ndup.next = node_recv(fd);
			n->ndup.fd = int_recv(fd);
			n->ndup.dupfd = int_recv(fd);
			n->ndup.vname = node_recv(fd);
			break;

		case NXHERE:
		case NHERE:
			n->nhere.next = node_recv(fd);
			n->nhere.fd = int_recv(fd);
			n->nhere.doc = node_recv(fd);
			break;

		case NNOT:
			n->nnot.com = node_recv(fd);
			break;

		default:
			die("Unknown construct type");
	}

	guard_recv1(fd, -1);
	return n;
}

static void
funcnode_send(int fd, struct funcnode *n)
{
	int_send(fd, n->count);
	node_send(fd, &n->n);
	guard_send(fd, -1);
}

static struct funcnode*
funcnode_recv(int fd)
{
	struct funcnode *fn;
	union node *n;

	fn = ckmalloc(sizeof(struct funcnode));
	fn->count = int_recv(fd);
	n = node_recv(fd);
	memcpy(&fn->n, n, sizeof(union node));
	free(n);
	guard_recv1(fd, -1);
	return fn;
}

static void
tblentry_send(int fd)
{
	int i, id = 0;
	struct tblentry *e;
	int size;

	guard_send(fd, SER_TBLENTRY);
	for (i = 0;i < CMDTABLESIZE;i ++) {
		for (e = cmdtable[i];e;e = e->next) {
			guard_send(fd, id++);
			int_send(fd, i);
			size = sizeof(struct tblentry) - ARB + strlen(e->cmdname) + 1;
			int_send(fd, size);
			buf_send(fd, e, size);
			switch (e->cmdtype) {
				case CMDUNKNOWN:
				case CMDNORMAL:
					break;

				case CMDBUILTIN:
					int_send(fd, e->param.cmd - builtintab);
					break;

				case CMDFUNCTION:
					funcnode_send(fd, e->param.func);
					break;
			}
		}
	}
	guard_send(fd, -1);
}

static void
tblentry_recv(int fd)
{
	int i = 0;
	int size;
	struct tblentry *e;
	int id = 0;

	guard_recv1(fd, SER_TBLENTRY);

	do {
		if (guard_recv2(fd, id++, -1) == -1)
			break;
		i = int_recv(fd);
		size = int_recv(fd);
		e = ckmalloc(size);
		buf_recv(fd, e, size);
		switch (e->cmdtype) {
			case CMDUNKNOWN:
			case CMDNORMAL:
				break;

			case CMDBUILTIN:
				e->param.cmd = &builtintab[int_recv(fd)];
				break;

			case CMDFUNCTION:
				e->param.func = funcnode_recv(fd);
				break;
		}
		e->next = cmdtable[i];
		cmdtable[i] = e;
	} while (1);
}

static void
var_send(int fd, struct var *v)
{
	int_send(fd, v->flags);
	str_send(fd, v->text);
}

static struct var*
var_recv(int fd)
{
	struct var *v = ckmalloc(sizeof(struct var));
	memset(v, 0, sizeof(struct var));
	v->flags = int_recv(fd);
	v->text = str_recv(fd);
	return v;
}

static void
vartab_send(int fd)
{
	int i;
	struct var *v;
	int id = 0;

	guard_send(fd, SER_VARTAB);
	for (i = 0;i < VTABSIZE;i++)
		for (v = vartab[i];v;v = v->next) {
			guard_send(fd, id++);
			int_send(fd, i);
			var_send(fd, v);
		}
	guard_send(fd, -1);
}

static void
vartab_recv(int fd)
{
	int i;
	struct var *v;
	int id = 0;

	guard_recv1(fd, SER_VARTAB);
	do {
		if (guard_recv2(fd, id++, -1) == -1)
			break;
		i = int_recv(fd);
		v = var_recv(fd);
		v->next = vartab[i];
		vartab[i] = v;
	} while (1);
}

static void
localvar_send(int fd, struct localvar *lv)
{
	var_send(fd,lv->vp);
	int_send(fd, lv->flags);
	str_send(fd, lv->text);
}

static struct localvar*
localvar_recv(int fd)
{
	int i;
	struct var *v;
	struct localvar *lv;
	lv = ckmalloc(sizeof(struct localvar));
	memset(lv, 0, sizeof(struct localvar));
	lv->vp = var_recv(fd);
	for (i = 0, v = vartab[0];i < VTABSIZE;v = v && v->next ? v->next : vartab[i++])
		if (v && !strcmp(v->text, lv->vp->text)) {
			free((char*)lv->vp->text);
			free(lv->vp);
			lv->vp = v;
			break;
		}

	if (i == VTABSIZE) {
		struct var **vp;
		vp = hashvar(lv->vp->text);
		*vp = lv->vp;
		lv->vp->next = *vp;
	}

	lv->flags = int_recv(fd);
	lv->text = str_recv(fd);
	return lv;
}

static void
localvars_send(int fd)
{
	struct localvar *lv;
	int id = 0;

	guard_send(fd, SER_LOCALVARS);
	for (lv = localvars;lv;lv = lv->next) {
		guard_send(fd, id++);
		localvar_send(fd, lv);
	}
	guard_send(fd, -1);
}

static void
localvars_recv(int fd)
{
	struct localvar *lv;
	int id = 0;

	guard_recv1(fd, SER_LOCALVARS);
	do {
		if (guard_recv2(fd, id++, -1) == -1)
			break;
		lv = localvar_recv(fd);
		lv->next = localvars;
		localvars = lv;
	} while (1);
}

static void
optlist_send(int fd)
{
	guard_send(fd, SER_OPTLIST);
	buf_send(fd, optlist, sizeof(optlist));
	guard_send(fd, -1);
}

static void
optlist_recv(int fd)
{
	guard_recv1(fd, SER_OPTLIST);
	buf_recv(fd, optlist, sizeof(optlist));
	guard_recv1(fd, -1);
}

static void
sig_send(int fd)
{
	int i;
	for (i = 0;i < NSIG;i++) {
		guard_send(fd, i);
		/* str_send(fd, trap[i]); */
		int_send(fd, sigmode[i]);
		int_send(fd, gotsig[i]);
	}
	guard_send(fd, -1);
}

static void
sig_recv(int fd)
{
	int i;
	for (i = 0;i < NSIG;i++) {
		guard_recv1(fd, i);
		/* trap[i] = str_recv(fd); */
		sigmode[i] = int_recv(fd);
		gotsig[i] = int_recv(fd);
	}
	guard_recv1(fd, -1);
}

static void
redir_send(int fd)
{
	int i;
	struct redirtab *rd;
	for (rd = redirlist, i = 0;rd;rd = rd->next) {
		guard_send(fd, i++);
		buf_send(fd, rd->renamed, 10*sizeof(int));
		int_send(fd, rd->nullredirs);
	}
	guard_send(fd, -1);
	int_send(fd, nullredirs);
	int_send(fd, preverrout_fd);
	guard_send(fd, -1);
}

static void
redir_recv(int fd)
{
	int i = 0;
	struct redirtab *rd = NULL;
	while (1) {
		if (guard_recv2(fd, i++, -1) == -1)
			break;
		if (rd) {
			rd->next = ckmalloc(sizeof(struct redirtab));
			rd = rd->next;
		} else
			redirlist = rd = ckmalloc(sizeof(struct redirtab));
		rd->next = NULL;
		buf_recv(fd, rd->renamed, 10*sizeof(int));
		rd->nullredirs = int_recv(fd);
	}
	nullredirs = int_recv(fd);
	preverrout_fd = int_recv(fd);
	guard_recv1(fd, -1);
}

static void
shparam_send(int fd)
{
	int i;
	int_send(fd, shellparam.nparam);
#if ENABLE_ASH_GETOPTS
	int_send(fd, shellparam.optind);
	int_send(fd, shellparam.optoff);
#endif
	for (i = 0;i < shellparam.nparam;i ++) {
		guard_send(fd, i);
		str_send(fd, shellparam.p[i]);
	}
	guard_send(fd, -1);
}

static void
shparam_recv(int fd)
{
	int i;
	shellparam.nparam = int_recv(fd);
	shellparam.malloc = 1;
#if ENABLE_ASH_GETOPTS
	shellparam.optind = int_recv(fd);
	shellparam.optoff = int_recv(fd);
#endif
	if (shellparam.nparam) {
		shellparam.p = ckmalloc((shellparam.nparam+1)*sizeof(char *));
		for (i = 0;i < shellparam.nparam;i ++) {
			guard_recv1(fd, i);
			shellparam.p[i] = str_recv(fd);
		}
		shellparam.p[i] = NULL;
	} else
		shellparam.p = NULL;
	guard_recv1(fd, -1);
}

static void
misc_send(int fd)
{
	guard_send(fd, SER_MISC);
	int_send(fd, rootpid); /* used in varvalue */
	/* backgndpid ($!) is unsupported */
	int_send(fd, shlvl + 1);
	int_send(fd, exitstatus);
	int_send(fd, startlinno);
	str_send(fd, commandname);
	str_send(fd, arg0);
	str_send(fd, curdir == nullstr ? NULL : curdir);
	str_send(fd, physdir == nullstr ? NULL : physdir);
	sig_send(fd);
	redir_send(fd);
	shparam_send(fd);
	/* stackbase... */
	guard_send(fd, -1);
}

static void
misc_recv(int fd)
{
	guard_recv1(fd, SER_MISC);
	rootpid = int_recv(fd); /* used in varvalue */
	shlvl = int_recv(fd);
	exitstatus = int_recv(fd);
	startlinno = int_recv(fd);
	commandname = str_recv(fd);
	arg0 = str_recv(fd);
	curdir = str_recv(fd);
	if (!curdir) curdir = nullstr;
	physdir = str_recv(fd);
	if (!physdir) physdir = nullstr;
	sig_recv(fd);
	redir_recv(fd);
	shparam_recv(fd);
	/* stackbase... */
	guard_recv1(fd, -1);
}

/* * * * * fork entries * * * * */
struct forkpoint {
	const char *name;
	void (*func)(union node *,int);
};

static void
evalbackcmd_fp(union node *n, int flags)
{
	trace_printf("ash: subshell: %s\n",__PRETTY_FUNCTION__);
	FORCE_INT_ON;
	/*
	close(pip[0]);
	if (pip[1] != 1) {
		close(1);
		copyfd(pip[1], 1);
		close(pip[1]);
	}
	*/
	eflag = 0;
	evaltree(n, EV_EXIT); /* actually evaltreenr... */
}

static void
evalsubshell_fp(union node *n, int flags)
{
	trace_printf("ash: subshell: %s\n",__PRETTY_FUNCTION__);
	INT_ON;
	expredir(n->nredir.redirect);
	redirect(n->nredir.redirect, 0);
	evaltreenr(n->nredir.n, flags);
}

static void
evalpipe_fp(union node *n,int flags)
{
	trace_printf("ash: subshell: %s\n",__PRETTY_FUNCTION__);
	INT_ON;
	evaltreenr(n, flags);
}

static void
openhere_fp(union node *redir, int flags)
{
	trace_printf("ash: subshell: %s\n",__PRETTY_FUNCTION__);
	if (redir->type == NHERE) {
		size_t len = strlen(redir->nhere.doc->narg.text);
		full_write(1, redir->nhere.doc->narg.text, len);
	} else
		expandhere(redir->nhere.doc, 1);
	_exit(0);
}

/* entry names should not be too long */
struct forkpoint forkpoints[] = {
	{ "evalbackcmd", evalbackcmd_fp },
	{ "evalsubshell", evalsubshell_fp },
	{ "evalpipe", evalpipe_fp },
	{ "openhere", openhere_fp },
	{ NULL, NULL },
};

/* * * * * fork emulation * * * * */
static int
forkshell_init(struct forkshell *fs)
{
	static char argv2[32];
	static const char *argv[4] = { NULL, "sh", argv2, NULL };
	int p[2];

	if (_pipe(p, 0, 0) < 0)
		ash_msg_and_raise_error("Unable to create pipe");

	/*
	 * Do not use C file handle here because MinGW port
	 * uses a custom execvp version, which will not reconstruct
	 * filehandle table correctly (for msvcrt to read)
	 */
	sprintf(argv2, "subash%lx:%s", _get_osfhandle(p[0]), fs->fp);

	argv[0] = CONFIG_BUSYBOX_EXEC_PATH;
	fs->cmd.argv = argv;
	fs->fd = p[1];
	return 0;
}

static void
forkshell_transfer(struct forkshell *fs)
{
	tblentry_send(fs->fd);
	vartab_send(fs->fd);
	localvars_send(fs->fd);
	optlist_send(fs->fd);
	misc_send(fs->fd);

	guard_send(fs->fd, SER_FORK);
	node_send(fs->fd, fs->n);
	int_send(fs->fd, fs->flags);
}

static void
forkshell_transfer_done(struct forkshell *fs)
{
	guard_send(fs->fd, SER_DONE);
	close(fs->fd);
}

static void
forkshell_cleanup(struct forkshell *fs)
{
}

static int
forkshell(const char *fp, union node *n, int flags)
{
	struct forkshell fs;
	int status;
	memset(&fs, 0, sizeof(fs));
	fs.fp = fp;
	fs.n = n;
	fs.flags = flags;
	forkshell_init(&fs);
	if (start_command(&fs.cmd))
		ash_msg_and_raise_error("unable to spawn shell");
	forkshell_transfer(&fs);
	forkshell_transfer_done(&fs);
	forkshell_cleanup(&fs);
	set_exitstatus(finish_command(&fs.cmd), fs.cmd.argv, &status);
	return status;
}

static void
subshell_run()
{
	struct forkpoint *fp;
	int fd = subash_fd;
	union node *n;
	int flags;

	if (fd == -1)
		return;

	for (fp = forkpoints;fp->name && strcmp(fp->name, subash_entry);fp ++);

	if (!fp->name)
		die("ASH_SHELL_ENTRY invalid");

	tblentry_recv(fd);
	vartab_recv(fd);
	localvars_recv(fd);
	optlist_recv(fd);
	misc_recv(fd);

	guard_recv1(fd, SER_FORK);
	n = node_recv(fd);
	flags = int_recv(fd);
	guard_recv1(fd, SER_DONE);

	fp->func(n, flags);

	die("subshell ended unexpectedly");
}

static int
tryspawn(const char *cmd, const char **argv, const char * const*envp)
{
	struct child_process cp;

	memset(&cp, 0, sizeof(cp));

	cp.env = envp;
#if ENABLE_FEATURE_SH_STANDALONE
	if (strchr(cmd, '/') == NULL) {
		const struct bb_applet *a;

		a = find_applet_by_name(cmd);
		if (a) {
			const char **new_argv;
			const char **argp;
			int retval;

			for (argp = argv;*argp;argp++);
			new_argv = xmalloc(sizeof(const char *)*(argp - argv + 2));
			new_argv[0] = CONFIG_BUSYBOX_EXEC_PATH;
			memcpy(&new_argv[1], &argv[0], (argp - argv + 1)*sizeof(const char*));
			cp.argv = new_argv;
			trace_argv_printf(new_argv, "git-box: applet:");
			retval = set_exitstatus(run_command(&cp), new_argv, NULL);
			free(new_argv);
			return retval;
		}
	}
#endif

	/* FIXME
	 * Need to copy vartab atab cmdtable localvars to the subshell
	 */
	cp.argv = argv;
	trace_argv_printf(argv, "git-box: spawn:");
	return set_exitstatus(run_command(&cp), argv, NULL);
}

static const char * const*
shellspawn_getenv(const struct strlist *newvars)
{
	struct var **vpp;
	struct var *vp;
	const struct strlist *vlp;
	char **ep;
	int mask;
	int on = VEXPORT;
	int off = VUNSET;

	STARTSTACKSTR(ep);
	vpp = vartab;
	mask = on | off;
	do {
		for (vp = *vpp; vp; vp = vp->next) {
			if ((vp->flags & mask) == on) {
				if (ep == stackstrend())
					ep = growstackstr();
				for (vlp = newvars;vlp;vlp = vlp->next)
					if (varequal(vlp->text, vp->text))
						break;
				if (!vlp)
					*ep++ = (char *) vp->text;
			}
		}
	} while (++vpp < vartab + VTABSIZE);
	for (vlp = newvars;vlp;vlp = vlp->next) {
		if (ep == stackstrend())
			ep = growstackstr();
		*ep++ = vlp->text;
	}
	if (ep == stackstrend())
		ep = growstackstr();
	*ep++ = NULL;
	return grabstackstr(ep);
}

static int
shellspawn(const char **argv, const char *path, int idx, struct strlist *varlist)
{
	char *cmdname;
	int e;
	const char* const* envp;

	/*clearredir(1);*/
	/*listsetvar(varlist.list, VEXPORT|VSTACK);*/
	/*envp = listvars(VEXPORT, VUNSET, 0);*/
	envp = shellspawn_getenv(varlist);
	if (strchr(argv[0], '/')
#if ENABLE_FEATURE_SH_STANDALONE
	 || find_applet_by_name(argv[0])
#endif
	) {
		e = tryspawn(argv[0], argv, envp);
	} else {
		e = ENOENT;
		while ((cmdname = padvance(&path, argv[0])) != NULL) {
			if (--idx < 0 && pathopt == NULL) {
				e = tryspawn(cmdname, argv, envp);
				if (e != -1) break;
			}
			stunalloc(cmdname);
		}
	}

	if (e == -1) {
		e = errno;
		switch (e) {
		case EACCES:
			exitstatus = 126;
			break;
		case ENOENT:
			exitstatus = 127;
			break;
		default:
			exitstatus = 2;
			break;
		}
		ash_msg_and_raise(EXEXEC, "%s: %s", argv[0], errmsg(e, "not found"));
	}
	return e;
}

static int set_exitstatus(int val, const char **argv,int *out)
{
	if (!out)
		out = &exitstatus;
	switch (val) {
		case -ERR_RUN_COMMAND_WAITPID_WRONG_PID:
			if (argv)
				ash_msg_and_raise_error("%s: Waitpid on wrong PID",argv[0]);
			else
				ash_msg_and_raise_error("Waitpid on wrong PID");
			break;

		case -ERR_RUN_COMMAND_WAITPID_SIGNAL:
		case -ERR_RUN_COMMAND_WAITPID_NOEXIT:
			*out = -val + 128;
			return 0;

		default:
			*out = -val;
			return 0;
	}
	return -1;
}

static const char *
updatepwd(const char *dir)
{
	char *new;
	char *p;
	char *cdcomppath;
	const char *lim;
	/*
	 * There are four cases
	 *  absdrive +  abspath: c:/path
	 *  absdrive + !abspath: c:path
	 * !absdrive +  abspath: /path
	 * !absdrive + !abspath: path
	 *
	 * Damn DOS!
	 * c:path behaviour is "undefined"
	 * To properly handle this case, I have to keep track of cwd
	 * of every drive, which is too painful to do.
	 * So when c:path is given, I assume it's c:${curdir}path
	 * with ${curdir} comes from the current drive
	 */
	int absdrive = *dir && dir[1] == ':';
	int abspath = absdrive ? dir[2] == '/' : *dir == '/';
	char *drive;

	cdcomppath = ststrdup(dir);
	STARTSTACKSTR(new);
	if (!absdrive && curdir == nullstr)
		return 0;
	if (!abspath) {
		if (curdir == nullstr)
			return 0;
		new = stack_putstr(curdir, new);
	}
	new = makestrspace(strlen(dir) + 2, new);

	drive = stackblock();
	if (absdrive) {
		*drive = *dir;
		cdcomppath += 2;
		dir += 2;
	} else {
		*drive = *curdir;
	}
	drive[1] = ':'; /* in case of absolute drive+path */

	if (abspath)
		new = drive + 2;
	lim = drive + 3;
	if (!abspath) {
		if (new[-1] != '/')
			USTPUTC('/', new);
		if (new > lim && *lim == '/')
			lim++;
	} else {
		USTPUTC('/', new);
		cdcomppath ++;
		if (dir[1] == '/' && dir[2] != '/') {
			USTPUTC('/', new);
			cdcomppath++;
			lim++;
		}
	}
	p = strtok(cdcomppath, "/");
	while (p) {
		switch (*p) {
		case '.':
			if (p[1] == '.' && p[2] == '\0') {
				while (new > lim) {
					STUNPUTC(new);
					if (new[-1] == '/')
						break;
				}
				break;
			}
			if (p[1] == '\0')
				break;
			/* fall through */
		default:
			new = stack_putstr(p, new);
			USTPUTC('/', new);
		}
		p = strtok(0, "/");
	}
	if (new > lim)
		STUNPUTC(new);
	*new = 0;
	return stackblock();
}

static void
evalpipe(union node *n, int flags)
{
	struct forkshell fs;
	struct nodelist *lp;
	int pipelen;
	int prevfd;
	int pip[2];

	TRACE(("evalpipe(0x%lx) called\n", (long)n));
	pipelen = 0;
	for (lp = n->npipe.cmdlist; lp; lp = lp->next)
		pipelen++;
	flags |= EV_EXIT;
	INT_OFF;
	prevfd = -1;
	for (lp = n->npipe.cmdlist; lp; lp = lp->next) {
		prehash(lp->n);
		pip[1] = -1;
		if (lp->next) {
			if (_pipe(pip, 0, O_NOINHERIT) < 0) {
				close(prevfd);
				ash_msg_and_raise_error("pipe call failed");
			}
		}
		if (prevfd != -1)
			forkshell_cleanup(&fs);
		memset(&fs, 0, sizeof(fs));
		fs.fp = "evalpipe";
		fs.flags = flags;
		fs.n = lp->n;
		if (prevfd > 0)
			fs.cmd.in = prevfd;
		if (pip[1] > 1)
			fs.cmd.out = pip[1];
		forkshell_init(&fs);
		if (start_command(&fs.cmd))
			ash_msg_and_raise_error("unable to spawn shell");
		forkshell_transfer(&fs);
		forkshell_transfer_done(&fs);
		if (prevfd >= 0)
			close(prevfd);
		prevfd = pip[0];
		if (pip[1] >= 0)
			close(pip[1]);
	}
	if (n->npipe.backgnd == 0) {
		set_exitstatus(finish_command(&fs.cmd), fs.cmd.argv, NULL); /* the last command in pipe */
	}
	INT_ON;
}

static int
openhere(union node *redir)
{
	struct forkshell fs;

	if (redir->type == NHERE) {
		size_t len = strlen(redir->nhere.doc->narg.text);
		if (len <= PIPESIZE) {
			int pip[2];
			if (_pipe(pip, 0, 0) < 0)
				ash_msg_and_raise_error("pipe call failed");
			full_write(pip[1], redir->nhere.doc->narg.text, len);
			close(pip[1]);
			return pip[0];
		}
	}
	memset(&fs, 0, sizeof(fs));
	fs.fp = "openhere";
	fs.flags = 0;
	fs.n = redir;
	fs.cmd.no_stdin = 1;
	fs.cmd.out = -1;
	forkshell_init(&fs);
	if (start_command(&fs.cmd))
		ash_msg_and_raise_error("unable to spawn shell");
	forkshell_transfer(&fs);
	forkshell_transfer_done(&fs);
	return fs.cmd.out;
}
