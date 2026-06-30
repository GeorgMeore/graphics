#include "types.h"
#include "color.h"
#include "image.h"
#include "draw.h"
#include "poly.h"
#include "math.h"
#include "font.h"

U16 findglyph(Font f, U32 code)
{
	if (!f.ctable[0])
		return 0;
	U16 l = 0, r = f.npoints;
	while (l < r) {
		U16 m = l + (r - l)/2;
		if (f.ctable[0][m] == code)
			return f.ctable[1][m];
		if (f.ctable[0][m] < code)
			l = m + 1;
		else
			r = m;
	}
	return 0;
}

static Segment segtransform(Segment s, I16 x0, I16 y0, F64 scale)
{
	for (U8 i = 0; i < 3; i++) {
		s.x[i] = x0 + s.x[i]*scale;
		s.y[i] = y0 - s.y[i]*scale;
	}
	return s;
}

/*
 * To get a correct winding count with join points, I used a trick where if a ray hits
 * exactly a segment`s start or end point, we add a "half" of it`s y-direction at that point.
 *
 * For lines the rules are:
 *
 * y            (dy > 0)            (dy < 0)
 * ^          ^                   v
 * |  ray     |                   |
 * | ----->   |  +2     .  +1     |  -2     .  -1   (and horizontals give +0)
 * |          |         |         |         |
 * |          ^         ^         v         v
 * '----------------------------------------------->x
 *
 * This guarantees correct handling of segment joins (no false intersections
 * on peaks and no double-counting), e.g.
 *
 *                             _   _/  /
 *                            / \ /    |
 *
 * For parabolas it's a bit more tricky, but the idea is the same.
 */

typedef struct {
	F64 x[2];
	I8  wn[2];
	U8  n;
} Hit;

static Hit isectline(I16 ry, I16 x1, I16 y1, I16 x2, I16 y2)
{
	Hit h = {0};
	if (y1 == y2)
		return h;
	h.n = 1;
	if (ry == y1) {
		h.x[0] = x1, h.wn[0] = SIGN(y2 - y1);
	} else if (ry == y2) {
		h.x[0] = x2, h.wn[0] = SIGN(y2 - y1);
	} else {
		h.x[0] = (x1*(y2 - y1) + (ry - y1)*(x2 - x1))/(F64)(y2 - y1);
		h.wn[0] = 2*SIGN(y2 - y1);
	}
	return h;
}

static Hit isectcurve(I16 ry, I16 x1, I16 y1, I16 x2, I16 y2, I16 x3, I16 y3)
{
	F64 a = y1 - 2*y2 + y3;
	F64 b = 2*(y2 - y1);
	F64 c = y1 - ry;
	Poly x = {{x1, 2*(x2 - x1), x1 - 2*x2 + x3}, 2};
	Hit h = {0};
	if (a == 0) {
		F64 t = -c/b;
		if (t >= 0 && t <= 1) {
			h.n = 1;
			h.x[0] = eval(x, t);
			if (t == 0 || t == 1)
				h.wn[0] = SIGN(y2 - y1);
			else
				h.wn[0] = 2*SIGN(y2 - y1);
		}
		return h;
	}
	I64 d = b*b - 4*a*c;
	if (d < 0)
		return h;
	if (d == 0) {
		F64 t = -b/(2*a);
		if (t == 0 || t == 1) {
			h.n = 1;
			h.x[0] = eval(x, t);
			h.wn[0] = SIGN(y3 - y1);
		}
		return h;
	}
	F64 t[2] = {(-b - fsqrt(d))/(2*a), (-b + fsqrt(d))/(2*a)};
	for (I j = 0; j < 2; j++) {
		if (t[j] >= 0 && t[j] <= 1) {
			h.x[h.n] = eval(x, t[j]);
			if (t[j] == 0)
				h.wn[h.n] = SIGN(y2 - y1);
			else if (t[j] == 1)
				h.wn[h.n] = SIGN(y3 - y2);
			else if (t[j] < -b/(2*a))
				h.wn[h.n] = 2*SIGN(y2 - y1);
			else
				h.wn[h.n] = 2*SIGN(y3 - y2);
			h.n += 1;
		}
	}
	return h;
}

void drawbmp(Image *f, I16 x0, I16 y0, Glyph g, F64 scale)
{
	if (!g.nseg)
		return;
	drawclear(f, 0);
	for (U16 i = 0; i < g.nseg; i++) {
		Segment s = segtransform(g.segs[i], x0, y0, scale);
		I16 ylo, yhi;
		if (s.type == SegQuad)
			ylo = MIN3(s.y[0], s.y[1], s.y[2]), yhi = MAX3(s.y[0], s.y[1], s.y[2]);
		else
			ylo = MIN(s.y[0], s.y[1]), yhi = MAX(s.y[0], s.y[1]);
		for (I16 y = CLIPY(f, ylo); y < CLIPY(f, yhi + 1); y++) {
			Hit h;
			if (s.type == SegQuad)
				h = isectcurve(y, s.x[0], s.y[0], s.x[1], s.y[1], s.x[2], s.y[2]);
			else
				h = isectline(y, s.x[0], s.y[0], s.x[1], s.y[1]);
			for (I16 j = 0; j < h.n; j++) {
				if (h.x[j] < f->w)
					PIXEL(f, (I16)MAX(0, h.x[j]), y) += h.wn[j];
			}
		}
	}
	for (I16 y = 0; y < f->h; y++) {
		I32 wn = 0;
		for (I16 x = 0; x < f->w; x++) {
			wn += PIXEL(f, x, y);
			PIXEL(f, x, y) = BOOL(wn);
		}
	}
}

static F64 step(F64 v, F64 lim, F64 dir)
{
	return dir*MIN(ffloor(dir*v + 1), dir*lim);
}

static F64 adv(F64 v, F64 a, F64 lim, F64 dir)
{
	return dir*MIN(dir*(v + a), dir*lim);
}

/*
 * The idea of this approach is to use pixel coverage as opacity.
 * To find the coverage values we incrementally build the signed area
 * difference array.
 *
 *          i                  i+1
 * .------------------.------------------.
 * |        (x, y)    |                  |
 * |__________,_______|__________________|  Each line segment contained within one pixel
 * |         /        |                  |  adds Sr to the containing pixel (i) and (yn - y)
 * |        /         |                  |  to all the pixels to the right of it.
 * |  Sl   /    Sr    |                  |  Since (yn - y) = Sr + Sl, we should simply
 * |      /           |                  |  add Sr and Sl to i-th and i+1-st pixel's array
 * |_____/____________|__________________|  values respectively.
 * |  (xn, yn)        |                  |
 * '------------------'------------------'
 */
static void putline(Image *i, F64 x1, F64 y1, F64 x2, F64 y2)
{
	if (y1 == y2)
		return;
	F64 dx = x2 - x1, sx = SIGN(dx);
	F64 dy = y2 - y1, sy = SIGN(dy);
	for (F64 x = x1, y = y1; y != y2;) {
		F64 xn = step(x, x2, sx), xd = xn - x;
		F64 yn = step(y, y2, sy), yd = yn - y;
		if (dx) {
			if (xd && fabs(dy*xd) < fabs(yd*dx))
				yn = adv(y, dy/dx*xd, yn, sy);
			else
				xn = adv(x, dx/dy*yd, xn, sx);
		} else {
			xn = x;
		}
		F64 xm = (x + xn)/2, ym = (y + yn)/2;
		I16 px = ffloor(xm), py = ffloor(ym);
		F64 d = yn - y, f = xm - px;
		if (py >= 0 && py < i->h) {
			if (px < i->w)
				*(F32 *)&PIXEL(i, MAX(px, 0),   py) += d*(1 - f);
			if (px+1 < i->w)
				*(F32 *)&PIXEL(i, MAX(px+1, 0), py) += d*f;
		}
		x = xn, y = yn;
	}
}

static void putquad(Image *f, F64 x1, F64 y1, F64 x2, F64 y2, F64 x3, F64 y3)
{
	F64 l2  = (x3 - x1)*(x3 - x1) + (y3 - y1)*(y3 - y1);
	F64 dot = (x2 - x1)*(y3 - y1) - (y2 - y1)*(x3 - x1);
	if (dot*dot <= .25*l2) {
		/* control is less than half pixel away */
		putline(f, x1, y1, x3, y3);
		return;
	}
	F64 lx = (x1 + x2)/2, ly = (y1 + y2)/2;
	F64 rx = (x2 + x3)/2, ry = (y2 + y3)/2;
	F64 mx = (lx + rx)/2, my = (ly + ry)/2;
	putquad(f, x1, y1, lx, ly, mx, my);
	putquad(f, mx, my, rx, ry, x3, y3);
}

void drawbmpaa(Image *f, I16 x0, I16 y0, Glyph g, F64 scale)
{
	if (!g.nseg)
		return;
	drawclear(f, 0);
	for (U16 i = 0; i < g.nseg; i++) {
		Segment s = segtransform(g.segs[i], x0, y0, scale);
		if (s.type == SegLine)
			putline(f, s.x[0], s.y[0], s.x[1], s.y[1]);
		else
			putquad(f, s.x[0], s.y[0], s.x[1], s.y[1], s.x[2], s.y[2]);
	}
	for (I16 y = 0; y < f->h; y++) {
		F64 a = 0;
		for (I16 x = 0; x < f->w; x++) {
			a += *(F32 *)&PIXEL(f, x, y);
			*(F32 *)&PIXEL(f, x, y) = MIN(fabs(a), 1);
		}
	}
}

static F64 distline(F64 x, F64 y, F64 x1, F64 y1, F64 x2, F64 y2)
{
	F64 dx = x2 - x1, dy = y2 - y1;
	F64 wx = x - x1,  wy = y - y1;
	F64 ux = x - x2,  uy = y - y2;
	F64 l2 = dx*dx + dy*dy, t = wx*dx + wy*dy;
	if (t < 0)
		return wx*wx + wy*wy;
	if (t >= l2)
		return ux*ux + uy*uy;
	F64 n = wx*dy - wy*dx;
	return n*n / l2;
}

static F64 disttriangle(F64 x, F64 y, F64 x1, F64 y1, F64 x2, F64 y2, F64 x3, F64 y3)
{
	F64 o1 = (x - x1)*(y2 - y1) - (y - y1)*(x2 - x1);
	F64 o2 = (x - x2)*(y3 - y2) - (y - y2)*(x3 - x2);
	F64 o3 = (x - x3)*(y1 - y3) - (y - y3)*(x1 - x3);
	if (fsignbit(o1) == fsignbit(o2) && fsignbit(o2) == fsignbit(o3))
		return 0;
	return MIN3(
		distline(x, y, x1, y1, x2, y2),
		distline(x, y, x2, y2, x3, y3),
		distline(x, y, x3, y3, x1, y1)
	);
}

static F64 distcurve(F64 x, F64 y, F64 x1, F64 y1, F64 x2, F64 y2, F64 x3, F64 y3, F64 dmin)
{
	/* NOTE: if the current minimal distance is less than the distance
	 * to the bouding triangle, we can safely skip this curve */
	if (dmin < disttriangle(x, y, x1, y1, x2, y2, x3, y3))
		return dmin;
	/* NOTE: search for extremums of the square distance function */
	Poly dx = {{(x1 - x), 2*(x2 - x1), x3 + x1 - 2*x2}, 2};
	Poly dy = {{(y1 - y), 2*(y2 - y1), y3 + y1 - 2*y2}, 2};
	Poly d2 = padd(pmul(dx, dx), pmul(dy, dy));
	Roots r = roots(ddx(d2));
	F64 d = MIN(eval(d2, 0), eval(d2, 1));
	for (U8 i = 0; i < r.n; i++)
		if (r.v[i] > 0 && r.v[i] < 1)
			d = MIN(d, eval(d2, r.v[i]));
	return d;
}

void drawsdf(Image *f, I16 x0, I16 y0, Glyph g, F64 scale)
{
	if (!g.nseg)
		return;
	drawbmp(f, x0, y0, g, scale); /* calculate signs */
	for (I16 y = 0; y < f->h; y++)
	for (I16 x = 0; x < f->w; x++) {
		F64 d = INF;
		for (I i = 0; i < g.nseg; i++) {
			Segment s = segtransform(g.segs[i], x0, y0, scale);
			if (s.type == SegQuad)
				d = MIN(d, distcurve(x, y, s.x[0], s.y[0], s.x[1], s.y[1], s.x[2], s.y[2], d));
			else
				d = MIN(d, distline(x, y, s.x[0], s.y[0], s.x[1], s.y[1]));
		}
		*(F32 *)&PIXEL(f, x, y) = fsetsign(d, PIXEL(f, x, y));
	}
	/* NOTE: this code is for debugging */
	/*
	 * for (I16 y = 0; y < f->h; y++)
	 * for (I16 x = 0; x < f->w; x++) {
	 * 	F32 d = *(F32 *)&PIXEL(f, x, y);
	 * 	F32 a = smoothstep(5, 0, fabs(d)) * 255;
	 * 	PIXEL(f, x, y) = RGBA(a, a, a, 255);
	 * }
	 */
}

void drawoutline(Image *f, I16 x0, I16 y0, Glyph g, Color c, F64 scale)
{
	for (I i = 0; i < g.nseg; i++) {
		Segment s = segtransform(g.segs[i], x0, y0, scale);
		if (s.type == SegQuad)
			drawbezier(f, s.x[0], s.y[0], s.x[1], s.y[1], s.x[2], s.y[2], c);
		else
			drawline(f, s.x[0], s.y[0], s.x[1], s.y[1], c);
	}
}
