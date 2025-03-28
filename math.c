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
