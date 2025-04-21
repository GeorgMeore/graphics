#define MAXDEG 128

typedef struct {
	F64 c[MAXDEG+1];
	U8  d;
} Poly;

Poly padd(Poly a, Poly b);
Poly pmul(Poly a, Poly b);
Poly pdiv(Poly a, Poly b);
Poly ddx(Poly p);
F64  eval(Poly p, F64 x);

typedef struct {
	F64 v[MAXDEG];
	U8  n;
} Roots;

Roots roots(Poly p);
