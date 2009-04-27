#include "libbb.h"
#include "strbuf_file.h"

struct strbuf_file *sfdopen(int fd, const char *mode)
{
	struct strbuf_file *s;

	s = xmalloc(sizeof(*s));
	strbuf_init(&s->input, 0);
	s->has_error = 0;
	s->fd = fd;
	return s;
}

int sfclose(struct strbuf_file *sf)
{
	strbuf_release(&sf->input);
	close(sf->fd);
	free(sf);
	return 0;
}

int sfprintf(struct strbuf_file *sf, const char *fmt, ...)
{
	int len;
	va_list ap;
	struct strbuf sb = STRBUF_INIT;

	strbuf_grow(&sb, 64);
	va_start(ap, fmt);
	len = vsnprintf(sb.buf + sb.len, sb.alloc - sb.len, fmt, ap);
	va_end(ap);
	if (len < 0)
		die("your vsnprintf is broken");
	if (len > strbuf_avail(&sb)) {
		strbuf_grow(&sb, len);
		va_start(ap, fmt);
		len = vsnprintf(sb.buf + sb.len, sb.alloc - sb.len, fmt, ap);
		va_end(ap);
		if (len > strbuf_avail(&sb)) {
			die("this should not happen, your snprintf is broken");
		}
	}
	strbuf_setlen(&sb, sb.len + len);
	len = write(sf->fd, sb.buf, sb.len);
	strbuf_release(&sb);
	return len;
}

void sfclearerr(struct strbuf_file *sf)
{
	sf->has_error = 0;
}

int sfread(char *buf, int size, int n, struct strbuf_file *sf)
{
	int avail = size * n;
	int len = avail > sf->input.len ? sf->input.len : avail;
	char *p;
	struct strbuf *sb = &sf->input;

	if (len) {
		memcpy(buf, sb->buf, len);
		strbuf_setlen(sb, sb->len - len);
		avail -= len;
	}

	p = buf + len;
	while ((len = read(sf->fd, p, avail)) > 0) {
		p += len;
		avail -= len;
		if (!avail)
			break;
	}
	if  (len == -1)
		sf->has_error = 1;
	/* just in case we have some leftover */
	if ((p-buf) % size) {
		len = (p-buf) % size;
		strbuf_grow(sb, len);
		strbuf_add(sb, p-len, len);
	}
	return (p-buf) / size;
}

int sfgetc(struct strbuf_file *sf)
{
	int ret;
	char ch;
	if (sf->input.len) {
		ret = sf->input.buf[0];
		memcpy(sf->input.buf, sf->input.buf+1, sf->input.len);
		sf->input.len--;
		return ret;
	}
	ret = read(sf->fd, &ch, 1);
	if (ret > 0)
		return ch;
	if (ret == -1)
		sf->has_error = 1;
	return EOF;
}

char *sfgets(char *buf, int size, struct strbuf_file *sf)
{
	char *p = buf;
	int ch = -1;
	while (size > 1 && (ch = sfgetc(sf)) != EOF) {
		*p++ = ch;
		size--;
		if (ch == '\n')
			break;
	}
	if (p > buf && size > 0) {
		*p++ = '\0';
		return buf;
	}
	return NULL;
}
