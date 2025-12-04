#define PI 3.14159265358979323846
#define INF (1e250 * 1e250)

U64 isqrt(U64 x);

F64 fsqrt(F64 x);
F64 fcbrt(F64 x);
F64 fsin(F64);
F64 fcos(F64);
F64 smoothstep(F64 e0, F64 e1, F64 x);

F64 ftrunc(F64 x);
F64 ffloor(F64 x);
F64 fceil(F64 x);
F64 fround(F64 x);

OK  fsign(F64 x);
F64 fsetsign(F64 x, OK v);
F64 fabs(F64 x);
OK  fisnan(F64 x);
OK  fisinf(F64 x);
