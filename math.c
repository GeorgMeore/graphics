#include "types.h"
#include "math.h"

U64 lsb(U64 x)
{
	return x ^ (x & (x - 1));
}

U64 msb(U64 x)
{
	/* NOTE: the trick is to set all bits to the right of msb to 1,
	 * the proper way would be to use intrinsics, but I'm experimenting */
	x |= x >> 1;
	x |= x >> 2;
	x |= x >> 4;
	x |= x >> 8;
	x |= x >> 16;
	x |= x >> 32;
	return x - (x >> 1);
}

U8 nlz(U64 x)
{
	U8 n = 64;
	U8 c;
	/* NOTE: this is unrolled binary search */
	c = !!(x >> 32) << 5;
	x >>= c, n -= c;
	c = !!(x >> 16) << 4;
	x >>= c, n -= c;
	c = !!(x >> 8) << 3;
	x >>= c, n -= c;
	c = !!(x >> 4) << 2;
	x >>= c, n -= c;
	c = !!(x >> 2) << 1;
	x >>= c, n -= c;
	c = x >> 1;
	x >>= c, n -= c;
	return (n - x);
}

U64 isqrt(U64 x)
{
	U64 l = 1, r = x;
	while (l < r) {
		U64 m = l + (r - l)/2;
		if (m*m > x)
			r = m;
		else
			l = m + 1;
	}
	return l - 1;
}

/* iexp(x, 2*k + 1) = x * iexp(x, 2*k)
 * iexp(x, 2*k) = iexp(x*x, k) */
U64 iexp(U64 x, U8 p)
{
	U64 v = 1, m = x;
	while (p) {
		U64 c = p & 1;
		v *= 1 ^ ((m ^ 1) & -c); /* conditional multiply */
		p >>= 1;
		m *= m;
	}
	return v;
}

U64 iabs(I64 x)
{
	I64 s = x >> 63;
	return (x + s) ^ s; /* == ~(x - 1) for 2's complement negatives */
}

I64 cpsign(I64 x, I64 y)
{
	I64 xs = x >> 63;
	I64 ys = y >> 63;
	I64 s = xs ^ ys;
	return (x + s) ^ s;
}

U64 divceil(U64 x, U64 y)
{
	return (x + y - 1) / y;
}

I64 divround(I64 x, I64 y)
{
	U64 ax = iabs(x), ay = iabs(y);
	U64 c = (ax + ay/2) / ay;
	I64 sx = x >> 63, sy = y >> 63;
	U64 s = sx ^ sy;
	return (c ^ s) - s;
}

I64 imod(I64 x, U32 y)
{
	return (x%y + y) % y;
}

typedef union {
	F64 v;
	struct {
		U64 frac: 52;
		U64 exp: 11;
		U64 sign: 1;
	};
} _F64;

OK fsignbit(F64 x)
{
	_F64 _x = {x};
	return _x.sign;
}

F64 fsetsign(F64 x, OK v)
{
	_F64 _x = {x};
	_x.sign = BOOL(v);
	return _x.v;
}

F64 fabs(F64 x)
{
	_F64 _x = {x};
	_x.sign = 0;
	return _x.v;
}

OK fisnan(F64 x)
{
	_F64 _x = {x};
	return _x.exp == 0b11111111111 && _x.frac;
}

OK fisinf(F64 x)
{
	_F64 _x = {x};
	return _x.exp == 0b11111111111 && !_x.frac;
}

F64 ftrunc(F64 x)
{
	if (x == 0)
		return x;
	_F64 _x = {x};
	I64 e = _x.exp - 1023;
	if (e >= 52) /* NOTE: NaN and +-inf go here */
		return x;
	if (e <= -1)
		return fsetsign(0, _x.sign);
	U64 mask = ((U64)1 << (52 - e)) - 1;
	_x.frac &= ~mask;
	return _x.v;
}

F64 ffloor(F64 x)
{
	F64 xt = ftrunc(x);
	if (x != xt)
		xt -= fsignbit(x);
	return xt;
}

F64 fceil(F64 x)
{
	F64 xt = ftrunc(x);
	if (x != xt)
		xt += 1 - fsignbit(x);
	return xt;
}

F64 fround(F64 x)
{
	F64 xt = ftrunc(x);
	if (fabs(x - xt) >= 0.5)
		xt += fsetsign(1, fsignbit(xt));
	return xt;
}

F64 fsqrt(F64 x)
{
	if (x == 0)
		return x;
	_F64 _x = {x};
	if (_x.sign)
		_x.exp = 0b11111111111;
	if (_x.exp == 0b11111111111)
		return _x.v;
	_x.exp = (_x.exp + 1023)/2;
	F64 s = _x.v;
	for (F64 i = 0, p = s-1; i < 15 && p != s; i++)
		p = s, s = (s + x/s)/2;
	return s;
}

/* TODO: binary search is computationally more predictable but
 * is slower than Newton's method, I should research that */
F64 fcbrt(F64 x)
{
	if (x == 0)
		return x;
	_F64 _x = {x};
	if (_x.exp == 0b11111111111)
		return _x.v;
	if (_x.sign)
		x = -x;
	F64 l = 0, r = x > 1 ? x : 1;
	for (F64 lp = l-1, rp = r+1; l > lp || r < rp;) {
		F64 m = l + (r - l)/2, y = m*m*m - x;
		lp = l, rp = r;
		if (y <= 0)
			l = m;
		if (y >= 0)
			r = m;
	}
	if (_x.sign)
		l = -l;
	return l;
}

/* TODO: tailor series are not good for sin/cos, but it'll do for now */
F64 fsin(F64 x)
{
	F64 nx = x - (2*PI)*ffloor(x / (2*PI));
	if (nx > PI)
		nx = PI - nx;
	F64 a = nx, t = nx;
	for (F64 i = 3; i <= 11; i += 2) {
		t = -t*nx*nx/(i*i - i);
		a += t;
	}
	return a;
}

F64 fcos(F64 x)
{
	return fsin(x + PI/2);
}

F64 smoothstep(F64 e0, F64 e1, F64 x)
{
	x = CLAMP((x - e0)/(e1 - e0), 0, 1);
	return x*x*(3 - 2*x);
}
