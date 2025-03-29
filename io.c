#include <fcntl.h>
#include <unistd.h>
#include <stdarg.h>

#include "types.h"
#include "io.h"
#include "mlib.h"

static IOBuffer _bin  = {.fd = 0};
static IOBuffer _bout = {.fd = 1};
static IOBuffer _berr = {.fd = 2};

IOBuffer *bin  = &_bin;
IOBuffer *bout = &_bout;
IOBuffer *berr = &_berr;

int bopen(IOBuffer *b, const char *path, U8 mode)
{
	b->mode = mode;
	if (mode == 'r') {
		b->fd = open(path, O_RDONLY);
	} else if (mode == 'w') {
		b->fd = open(path, O_WRONLY|O_CREAT|O_TRUNC, 0666);
	} else {
		b->fd = -1;
		b->error = 1;
	}
	return b->fd != -1;
}

int bclose(IOBuffer *b)
{
	return (b->mode == 'r' || bflush(b)) && !close(b->fd);
}

int bpeek(IOBuffer *b)
{
	if (b->i == b->count) {
		if (b->error)
			return -1;
		/* TODO: maybe I do need to distinguish eof from errors */
		int n = read(b->fd, b->bytes, IOBUFSIZE);
		if (n <= 0) {
			b->error = 1;
			return -1;
		}
		b->count = n;
		b->i = 0;
	}
	return b->bytes[b->i];
}

int bread(IOBuffer *b)
{
	int c = bpeek(b);
	if (c != -1)
		b->i += 1;
	return c;
}

int bflush(IOBuffer *b)
{
	if (b->error)
		return 0;
	for (U8 *p = b->bytes; b->i && !b->error;) {
		int n = write(b->fd, p, b->i);
		if (n < 0) {
			b->error = 1;
			return 0;
		} else {
			p += n;
			b->i -= n;
		}
	}
	return 1;
}

int bwrite(IOBuffer *b, U8 v)
{
	if (b->i == IOBUFSIZE)
		bflush(b);
	if (b->error)
		return 0;
	b->bytes[b->i] = v;
	b->i += 1;
	return 1;
}

static void bprintu(U64 x, IOBuffer *b, U8 base, U8 bytes)
{
	char digits[64] = {};
	if (!x) {
		bwrite(b, '0');
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
		bwrite(b, digits[c-1-i]);
}

static void bprinti(U64 x, IOBuffer *b, U8 bytes)
{
	if ((I64)x < 0) {
		bwrite(b, '-');
		x = -x;
	}
	bprintu(x, b, 10, bytes);
}

#define FMTUNSIGNED(fmt) ((fmt) & 0xFF00)
#define FMTSIZE(fmt)     ((fmt) & 0x00FF)

int _bprint(IOBuffer *b, ...)
{
	va_list args;
	va_start(args, b);
	for (;;) {
		char *s = va_arg(args, char *);
		if (s) {
			for (; *s; s++)
				bwrite(b, *s);
		} else {
			int fmt = va_arg(args, int);
			if (fmt == 0) {
				va_end(args);
				return bflush(b);
			}
			int base = va_arg(args, int);
			if (base == 2 || base == 16 || FMTUNSIGNED(fmt))
				bprintu(va_arg(args, U64), b, base, FMTSIZE(fmt));
			else if (base == 10)
				bprinti(va_arg(args, U64), b, FMTSIZE(fmt));
		}
	}
}

static int isdecimal(I c)
{
	return c >= '0' && c <= '9';
}

static int isws(I c)
{
	return c == ' ' || c == '\t' || c == '\r' || c == '\n';
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

static int binputi(IOBuffer *b, int fmt, void *p)
{
	I neg = 0;
	if (bpeek(b) == '-') {
		if (FMTUNSIGNED(fmt))
			return 0;
		neg = 1;
		bread(b);
	}
	U64 max = fmtmaxabs(fmt, neg);
	if (!isdecimal(bpeek(b)))
		return 0;
	U64 v = 0;
	while (isdecimal(bpeek(b))) {
		I c = bread(b);
		if (v > (max - (c - '0'))/10)
			return 0;
		v = v*10 + c - '0';
	}
	if (neg)
		v = -v;
	fmtstore(fmt, p, v);
	return 1;
}

int _binput(IOBuffer *b, ...)
{
	va_list args;
	va_start(args, b);
	int ok = 1;
	while (ok) {
		char *s = va_arg(args, char *);
		if (s) {
			if (*s) {
				for (; *s && *s == bpeek(b); s++)
					bread(b);
				ok = !*s;
			} else {
				U c = va_arg(args, U);
				ok = isws(bpeek(b));
				for (; c && isws(bpeek(b)); c--)
					bread(b);
			}
		} else {
			int fmt = va_arg(args, int);
			if (!fmt)
				break;
			void *p = va_arg(args, void *);
			if (FMTSIZE(fmt) == 1) {
				I c = bread(b);
				fmtstore(fmt, p, c);
				ok = c != -1;
			} else {
				ok = binputi(b, fmt, p);
			}
		}
	}
	va_end(args);
	return ok;
}
