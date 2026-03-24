#define PI 3.14159265358979323846
#define INF (1e250 * 1e250)

#define BOOL(v) (!!(v)) /* non-zero -> 1 */
#define SIGN(a) (((a) > 0) - ((a) < 0))
#define SQUARE(n) ((n) * (n))
#define MIN(a, b) ({\
	typeof(a) _av = (a);\
	typeof(b) _bv = (b);\
	((_av) > (_bv) ? (_bv) : (_av));\
})
#define MAX(a, b) ({\
	typeof(a) _av = (a);\
	typeof(b) _bv = (b);\
	((_av) < (_bv) ? (_bv) : (_av));\
})
#define MIN3(a, b, c) MIN(a, MIN(b, c))
#define MAX3(a, b, c) MAX(a, MAX(b, c))
#define CLAMP(x, l, r) (MIN(MAX(l, x), r))

U64 lsb(U64 x);
U64 msb(U64 x);
U8  nlz(U64 x);
U64 isqrt(U64 x);
U64 iexp(U64 x, U8 p);
I64 cpsign(I64 x, I64 y);
U64 iabs(I64 v);
U64 divceil(U64 x, U64 y);
I64 divround(I64 x, I64 y);
I64 imod(I64 x, U32 y);

F64 fsqrt(F64 x);
F64 fcbrt(F64 x);
F64 fsin(F64);
F64 fcos(F64);
F64 smoothstep(F64 e0, F64 e1, F64 x);

F64 ftrunc(F64 x);
F64 ffloor(F64 x);
F64 fceil(F64 x);
F64 fround(F64 x);

OK  fsignbit(F64 x);
F64 fsetsign(F64 x, OK v);
F64 fabs(F64 x);
OK  fisnan(F64 x);
OK  fisinf(F64 x);
