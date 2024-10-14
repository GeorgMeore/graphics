#include <unistd.h>
#include <stdarg.h>

#include "types.h"
#include "fmt.h"
#include "mlib.h"

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

static void printu(U64 x, Obuffer *b, U8 base, U8 bytes)
{
	char digits[64] = {};
	if (!x) {
		obufpush(b, '0');
		return;
	}
	/* mask out bits that came from sign extension */
	x &= MASK(0, bytes*8);
	int c;
	for (c = 0; x; c++) {
		/* a little bit of hacky hex digit calculation */
		digits[c] = '0' + x%base%10 + x%base/10*('a' - '0');
		x /= base;
	}
	for (int i = 0; i < c; i++)
		obufpush(b, digits[c-1-i]);
}

static void printi(U64 x, Obuffer *b, U8 bytes)
{
	if ((I64)x < 0) {
		obufpush(b, '-');
		x = -x;
	}
	printu(x, b, 10, bytes);
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
			int base = va_arg(args, int);
			if (base == 2 || base == 16 || FMTUNSIGNED(fmt))
				printu(va_arg(args, U64), &b, base, FMTSIZE(fmt));
			else if (base == 10)
				printi(va_arg(args, U64), &b, FMTSIZE(fmt));
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

static int isdigit10(I c)
{
	return c >= '0' && c <= '9';
}

static U64 fmtmaxabs(int fmt, int neg)
{
	switch (fmt) {
	case _INTFMT(I8):  return neg ? -(U64)MINVAL(I8) : MAXVAL(I8);
	case _INTFMT(U8):  return neg ? -(U64)MINVAL(U8) : MAXVAL(U8);
	case _INTFMT(I16): return neg ? -(U64)MINVAL(I16) : MAXVAL(I16);
	case _INTFMT(U16): return neg ? -(U64)MINVAL(U16) : MAXVAL(U16);
	case _INTFMT(I32): return neg ? -(U64)MINVAL(I32) : MAXVAL(I32);
	case _INTFMT(U32): return neg ? -(U64)MINVAL(U32) : MAXVAL(U32);
	case _INTFMT(I64): return neg ? -(U64)MINVAL(I64) : MAXVAL(I64);
	case _INTFMT(U64): return neg ? -(U64)MINVAL(U64) : MAXVAL(U64);
	}
	return 0;
}

static void fmtstore(int fmt, void *p, U64 val)
{
	switch (fmt) {
	case _INTFMT(U8):  *(U8 *)p = val; break;
	case _INTFMT(I8):  *(I8 *)p = val; break;
	case _INTFMT(U16): *(U16 *)p = val; break;
	case _INTFMT(I16): *(I16 *)p = val; break;
	case _INTFMT(U32): *(U32 *)p = val; break;
	case _INTFMT(I32): *(I32 *)p = val; break;
	case _INTFMT(U64): *(U64 *)p = val; break;
	case _INTFMT(I64): *(I64 *)p = val; break;
	}
}

static int inputi(Ibuffer *b, int fmt, void *p)
{
	I neg = 0;
	if (ibufpeek(b) == '-') {
		if (FMTUNSIGNED(fmt))
			return 0;
		neg = 1;
		ibufpop(b);
	}
	U64 max = fmtmaxabs(fmt, neg);
	if (!isdigit10(ibufpeek(b)))
		return 0;
	U64 v = 0;
	while (isdigit10(ibufpeek(b))) {
		I c = ibufpop(b);
		if (v > (max - (c - '0'))/10)
			return 0;
		v = v*10 + c - '0';
	}
	if (neg)
		v = -v;
	fmtstore(fmt, p, v);
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
				fmtstore(fmt, p, c);
			} else {
				if (!inputi(&b, fmt, p))
					return 0;
			}
		}
	}
}
