#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "buf.h"

#define BUF_ERROR ((buf_t) {NULL, SIZE_MAX, 0})

#define max(x, y) (((x) > (y))? (x) : (y))

static buf_t
grow(buf_t buf, size_t sz)
{
	void *p;

	buf.cap += sz;
	buf.p = (uint8_t *) realloc(buf.p, buf.cap);
	if (buf.p == NULL)
		return BUF_ERROR;
	return buf;
}

bool
buf_error(buf_t buf)
{

	return buf.len > buf.cap;
}

void
buf_free(buf_t buf)
{

	free(buf.p);
}

buf_t
buf_readfile(buf_t buf, FILE *f)
{

	while (!feof(f)) {
		if (buf.len == buf.cap)
			buf  = grow(buf, 1024 * 1024);
		if (buf_error(buf))
			return buf;
		buf.len += fread(buf.p + buf.len, 1, buf.cap - buf.len, f);
		if (ferror(f)) {
			buf_free(buf);
			return BUF_ERROR;
		}
	}
	return buf;
}

buf_t
buf_puts(buf_t buf, const char *s)
{
	int n;

	while (*s != '\0') {
		if (buf.len == buf.cap) {
			n = strlen(s);
			buf = grow(buf, max(n, buf.cap));
		}
		for (; buf.len < buf.cap && *s != '\0'; s++)
			*(buf.p + buf.len++) = *s;
	}
	return buf;
}

buf_t
buf_putz(buf_t buf, const char *s)
{

	buf = buf_puts(buf, s);
	if (buf.len == buf.cap) {
		buf = grow(buf, max(1, buf.cap));
		if (buf_error(buf))
			return BUF_ERROR;
	}
	*(buf.p + buf.len++) = '\0';
	return buf;
}

buf_t
buf_trunc(buf_t buf, size_t n)
{
	if (buf.len <= n)
		return buf;

	buf.len = n;
	return buf;
}