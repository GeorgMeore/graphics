#include <unistd.h>
#include <stdarg.h>

#include "types.h"
#include "print.h"

#define PBUFSIZE 256 /* We don't want TOO much stuff on the stack */

typedef struct {
	char data[PBUFSIZE];
	int i;
	int fd;
} Pbuffer;

static void pbufdrain(Pbuffer *b)
{
	for (char *d = b->data; b->i;) {
		int rc = write(b->fd, d, b->i);
		if (rc < 0) {
			/* NOTE: here we just drop everything in case of an error.
			 * Maybe that's not very correct, but who actually checks printf return values? */
			b->i = 0;
		} else {
			d += rc;
			b->i -= rc;
		}
	}
}

static void pbufpush(Pbuffer *b, char c)
{
	if (b->i + 1 >= PBUFSIZE)
		pbufdrain(b);
	b->data[b->i] = c;
	b->i += 1;
}

static void printu(U x, Pbuffer *b, U8 base)
{
	char digits[64] = {};
	if (base != 10) {
		pbufpush(b, '0');
		switch (base) {
			case 2:  pbufpush(b, 'b'); break;
			case 16: pbufpush(b, 'x'); break;
			default: pbufpush(b, '?'); /* TODO: maybe panic here? */
		}
	}
	if (!x) {
		pbufpush(b, '0');
		return;
	}
	int c;
	for (c = 0; x; c++) {
		/* a little bit of hacky hex digit calculation */
		digits[c] = '0' + x%base + x%base/10*('a' - '0');
		x /= base;
	}
	for (int i = 0; i < c; i++)
		pbufpush(b, digits[c-1-i]);
}

static void prints(I x, Pbuffer *b)
{
	if (x < 0) {
		pbufpush(b, '-');
		x = -x;
	}
	printu(x, b, 10);
}

void _fdprint(int fd, ...)
{
	va_list args;
	va_start(args, fd);
	Pbuffer b = {.fd = fd};
	for (;;) {
		char *s = va_arg(args, char *);
		if (s) {
			for (; *s; s++)
				pbufpush(&b, *s);
		} else {
			switch (va_arg(args, int)) {
			case 0:
				va_end(args);
				pbufdrain(&b);
				return;
			case 'b':
				printu(va_arg(args, U), &b, 2);
				break;
			case 'h':
				printu(va_arg(args, U), &b, 16);
				break;
			case 'u':
				printu(va_arg(args, U), &b, 10);
				break;
			case 'i':
				prints(va_arg(args, I), &b);
				break;
			}
		}
	}
}
