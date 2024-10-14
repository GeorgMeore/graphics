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

#define _FMTSWITCH(fmt, op, ...) ({\
	switch (fmt) {\
	case _INTFMT(U8):  op(U8, __VA_ARGS__); break;\
	case _INTFMT(I8):  op(I8, __VA_ARGS__); break;\
	case _INTFMT(U16): op(U16, __VA_ARGS__); break;\
	case _INTFMT(I16): op(I16, __VA_ARGS__); break;\
	case _INTFMT(U32): op(U32, __VA_ARGS__); break;\
	case _INTFMT(I32): op(I32, __VA_ARGS__); break;\
	case _INTFMT(U64): op(U64, __VA_ARGS__); break;\
	case _INTFMT(I64): op(I64, __VA_ARGS__); break;\
	}\
})

#define _FMTSTORE(t, p, v) (*(t *)(p) = (v))
#define FMTSTORE(fmt, p, v) _FMTSWITCH(fmt, _FMTSTORE, p, v)


#define _FMTGET(t, op, p) (*(p) = op(t))
#define FMTGET(fmt, op, p) _FMTSWITCH(fmt, _FMTGET, op, p)

static int isdigit10(I c)
{
	return c >= '0' && c <= '9';
}

static U64 fmtmaxabs(int fmt, int neg)
{
	U64 max;
	if (neg) {
		FMTGET(fmt, MINVAL, &max);
		max = -max;
	} else {
		FMTGET(fmt, MAXVAL, &max);
	}
	return max;
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
				if (!inputi(&b, fmt, p))
					return 0;
			}
		}
	}
}
