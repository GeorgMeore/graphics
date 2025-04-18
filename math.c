#include "types.h"
#include "math.h"

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

typedef union {
	F64 v;
	struct {
		U64 frac: 52;
		U64 exp: 11;
		U64 sign: 1;
	};
} _F64;

F64 ffloor(F64 x)
{
	if (x == 0)
		return x;
	_F64 _x = {x};
	I64 e = _x.exp - 1023;
	if (e >= 52) /* NOTE: NaN and +-inf go here */
		return x;
	if (e <= -1)
		return -_x.sign;
	U64 mask = ((U64)1 << (52 - e)) - 1;
	if (_x.frac & mask) {
		_x.frac &= ~mask;
		_x.v -= _x.sign;
	}
	return _x.v;
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
	_x.exp = (_x.exp + 1023 + 1)/2;
	F64 a = x, b = a-1;
	while (b != a) {
		b = a;
		a = (a + x/a)/2;
	}
	return a;
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
