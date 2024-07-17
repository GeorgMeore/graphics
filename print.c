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

#define MAXNUMLEN (20 + 1) /* 2^64 - 1 has 20 digits + 1 for the sign */

static void printu(u64 x, Pbuffer *b)
{
	char digits[MAXNUMLEN] = {};
	if (!x) {
		pbufpush(b, '0');
		return;
	}
	int c;
	for (c = 0; x; c++) {
		digits[c] = x%10 + '0';
		x /= 10;
	}
	for (int i = 0; i < c; i++)
		pbufpush(b, digits[c-1-i]);
}

static void prints(s64 x, Pbuffer *b)
{
	if (x < 0) {
		pbufpush(b, '-');
		x = -x;
	}
	printu(x, b);
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
			switch (va_arg(args, Format)) {
			case FEND:
				va_end(args);
				pbufdrain(&b);
				return;
			case FU:
				printu(va_arg(args, u64), &b);
				break;
			case FS:
				prints(va_arg(args, s64), &b);
				break;
			}
		}
	}
}
