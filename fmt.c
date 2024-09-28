#include <unistd.h>
#include <stdarg.h>

#include "types.h"
#include "fmt.h"

#define OBUFSIZE 256 /* We don't want TOO much stuff on the stack */

typedef struct {
	int fd;
	char data[OBUFSIZE];
	int i;
} Obuffer;

static void obufdrain(Obuffer *b)
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

static void obufpush(Obuffer *b, char c)
{
	if (b->i + 1 >= OBUFSIZE)
		obufdrain(b);
	b->data[b->i] = c;
	b->i += 1;
}

static void printu(U64 x, Obuffer *b, U8 base)
{
	char digits[64] = {};
	if (!x) {
		obufpush(b, '0');
		return;
	}
	int c;
	for (c = 0; x; c++) {
		/* a little bit of hacky hex digit calculation */
		digits[c] = '0' + x%base + x%base/10*('a' - '0');
		x /= base;
	}
	for (int i = 0; i < c; i++)
		obufpush(b, digits[c-1-i]);
}

static void prints(U64 x, Obuffer *b)
{
	if ((I64)x < 0) {
		obufpush(b, '-');
		x = -x;
	}
	printu(x, b, 10);
}

#define FMTUNSIGNED(fmt) ((fmt) & 0xFF00)
#define FMTSIZE(fmt)     ((fmt) & 0x00FF)

void _fdprint(int fd, ...)
{
	va_list args;
	va_start(args, fd);
	Obuffer b = {.fd = fd};
	for (;;) {
		char *s = va_arg(args, char *);
		if (s) {
			for (; *s; s++)
				obufpush(&b, *s);
		} else {
			int fmt = va_arg(args, int);
			if (fmt == 0) {
				va_end(args);
				obufdrain(&b);
				break;
			}
			switch (va_arg(args, int)) {
			case 2:
				printu(va_arg(args, U64), &b, 2);
				break;
			case 16:
				printu(va_arg(args, U64), &b, 16);
				break;
			case 10:
				if (FMTUNSIGNED(fmt))
					printu(va_arg(args, U64), &b, 10);
				else
					prints(va_arg(args, U64), &b);
			}
		}
	}
}

#define IEOF   -1
#define IERROR -2
#define IEMPTY -3

typedef struct {
	int fd;
	I next;
} Ibuffer;

I ibufpeek(Ibuffer *i)
{
	if (i->next == IEMPTY) {
		U8 c;
		I rc = read(i->fd, &c, 1);
		if (rc < 0)
			i->next = IERROR;
		else if (rc == 0)
			i->next = IEOF;
		else
			i->next = c;
	}
	return i->next;
}

I ibufpop(Ibuffer *i)
{
	if (i->next == IEMPTY)
		ibufpeek(i);
	I c = i->next;
	if (i->next >= 0)
		i->next = IEMPTY;
	return c;
}

#define _FMTSTORE(sign, size, p, v) ({\
	switch (size) {\
	case 1: *(sign##8  *)p = v; break;\
	case 2: *(sign##16 *)p = v; break;\
	case 4: *(sign##32 *)p = v; break;\
	case 8: *(sign##64 *)p = v; break;\
	}\
})

#define FMTSTORE(fmt, p, v) ({\
	if (FMTUNSIGNED(fmt))\
		_FMTSTORE(U, FMTSIZE(fmt), p, v);\
	else\
		_FMTSTORE(I, FMTSIZE(fmt), p, v);\
})

static int inputint(Ibuffer *b, int fmt, void *p)
{
	U64 v = 1;
	I c = ibufpop(b);
	if (c == '-') {
		v = -1;
		c = ibufpop(b);
	}
	if (c < '0' || c > '9')
		return 0;
	v *= c - '0';
	for (;;) {
		c = ibufpeek(b);
		if (c < '0' || c > '9')
			break;
		ibufpop(b);
		v = v*10 + c - '0';
	}
	FMTSTORE(fmt, p, v);
	return 1;
}

int _fdinputln(int fd, ...)
{
	va_list args;
	va_start(args, fd);
	Ibuffer b = {.fd = fd, .next = IEMPTY};
	for (;;) {
		char *s = va_arg(args, char *);
		if (s) {
			for (; *s; s++) {
				if (*s != ibufpop(&b)) {
					va_end(args);
					return 0;
				}
			}
		} else {
			int fmt = va_arg(args, int);
			if (!fmt) {
				if (ibufpeek(&b) == '\n')
					return 1;
				return 0;
			}
			void *p = va_arg(args, void *);
			if (FMTSIZE(fmt) == 1) {
				I c = ibufpop(&b);
				FMTSTORE(fmt, p, c);
			} else {
				if (!inputint(&b, fmt, p))
					return 0;
			}
		}
	}
}
