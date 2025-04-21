#include "types.h"
#include "math.h"
#include "poly.h"
#include "mlib.h"

Poly padd(Poly a, Poly b)
{
	Poly c = {};
	c.d = MAX(a.d, b.d);
	for (U8 i = 0; i <= c.d; i++)
		c.c[i] = a.c[i] + b.c[i];
	while (c.d && !c.c[c.d])
		c.d -= 1;
	return c;
}

Poly pmul(Poly a, Poly b)
{
	Poly c = {};
	if (a.d + b.d >= MAXDEG)
		return c;
	c.d = a.d + b.d;
	for (U8 i = 0; i <= a.d; i++)
	for (U8 j = 0; j <= b.d; j++)
		c.c[i+j] += a.c[i]*b.c[j];
	return c;
}

Poly pdiv(Poly a, Poly b)
{
	Poly c = {};
	if (a.d < b.d || !b.c[b.d])
		return c;
	c.d = a.d - b.d;
	for (I16 i = c.d; i >= 0; i--) {
		c.c[i] = a.c[b.d+i]/b.c[b.d];
		for (I16 j = 0; j <= b.d; j++)
			a.c[j+i] -= b.c[j]*c.c[i];
	}
	return c;
}

Poly ddx(Poly p)
{
	Poly q = {};
	if (p.d == 0)
		return q;
	q.d = p.d - 1;
	for (U8 i = 0; i <= q.d; i++)
		q.c[i] = p.c[i+1]*(i+1);
	return q;
}

F64 eval(Poly p, F64 x)
{
	F64 y = 0;
	for (U8 i = 0; i <= p.d; i++)
		y = x*y + p.c[p.d-i];
	return y;
}

/* NOTE: this function assumes that p has a positive leading
 * coefficient and that either p(l) < 0 (p(r) > 0) or you must
 * be able to get to a negative (positive) value by doubling l (r) */
static F64 bisectroot(Poly p, F64 l, F64 r)
{
	while (eval(p, l) > 0)
		r = l, l *= 2;
	while (eval(p, r) < 0)
		l = r, r *= 2;
	for (F64 lp = l-1, rp = r+1; l > lp || r < rp;) {
		lp = l, rp = r;
		F64 m = l + (r - l)/2;
		if (eval(p, m) > 0)
			r = m;
		else
			l = m;
	}
	return l;
}

/* TODO: definitely not the fastest or most precise implementation */
static Roots calcroots(Poly p)
{
	Roots r = {};
	if (p.d == 1) {
		r.n = 1;
		r.v[0] = -p.c[0]/p.c[1];
	} else if (p.d == 2) {
		F64 d = p.c[1]*p.c[1] - 4*p.c[2]*p.c[0];
		if (d == 0) {
			r.n = 2;
			r.v[0] = r.v[1] = -p.c[1]/(2*p.c[2]);
		} else if (d > 0) {
			F64 ds = fsqrt(d);
			r.n = 2;
			r.v[0] = (-p.c[1] + ds)/(2*p.c[2]);
			r.v[1] = (-p.c[1] - ds)/(2*p.c[2]);
		}
	} else if (p.d > 2) {
		F64 lb = 1, rb = -1;
		if (p.d % 2) {
			lb = -1, rb = 1;
		} else {
			Roots dr = calcroots(ddx(p));
			for (U8 i = 0; i < dr.n && lb > rb; i++)
				if (eval(p, dr.v[i]) <= 0)
					lb = dr.v[i], rb = MAX(dr.v[i] + 1, 1);
		}
		if (lb < rb) {
			F64 x = bisectroot(p, lb, rb);
			Poly t = {{-x, 1}, 1};
			r = calcroots(pdiv(p, t));
			r.v[r.n] = x;
			r.n += 1;
		}
	}
	return r;
}

Roots roots(Poly p)
{
	while (p.d && !p.c[p.d])
		p.d -= 1;
	if (p.c[p.d] < 0)
		for (U8 i = 0; i <= p.d; i++)
			p.c[i] = -p.c[i];
	return calcroots(p);
}
