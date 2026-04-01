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
	I16 xmin = g.xmin*scale - 1, xmax = g.xmax*scale + 1;
	I16 ymin = g.ymin*scale - 1, ymax = g.ymax*scale + 1;
	for (I16 y = CLIPY(f, y0 - ymax); y < CLIPY(f, y0 - ymin + 1); y++)
	for (I16 x = CLIPX(f, x0 + xmin); x < CLIPX(f, x0 + xmax + 1); x++)
		PIXEL(f, x, y) = 0;
	for (U16 i = 0; i < g.nseg; i++) {
		Segment s = g.segs[i];
		I16 x1 = s.x[0]*scale, y1 = s.y[0]*scale;
		I16 x2 = s.x[1]*scale, y2 = s.y[1]*scale;
		I16 x3 = s.x[2]*scale, y3 = s.y[2]*scale;
		I16 ylo, yhi;
		if (s.type == SegQuad)
			ylo = MIN3(y1, y2, y3), yhi = MAX3(y1, y2, y3);
		else
			ylo = MIN(y1, y2), yhi = MAX(y1, y2);
		for (I16 y = CLIPY(f, y0 - yhi); y < CLIPY(f, y0 - ylo + 1); y++) {
			Hit h;
			if (s.type == SegQuad)
				h = isectcurve(y0 - y, x1, y1, x2, y2, x3, y3);
			else
				h = isectline(y0 - y, x1, y1, x2, y2);
			for (I16 j = 0; j < h.n; j++) {
				I16 x = CLIPX(f, x0 + h.x[j] + 1) - 1;
				if (x >= 0)
					PIXEL(f, x, y) += h.wn[j];
			}
		}
	}
	for (I16 y = CLIPY(f, y0 - ymax); y < CLIPY(f, y0 - ymin + 1); y++) {
		I32 wn = 0;
		for (I16 x = CLIPX(f, x0 + xmin); x < CLIPX(f, x0 + xmax + 1); x++)
			wn += PIXEL(f, x, y);
		for (I16 x = CLIPX(f, x0 + xmin); x < CLIPX(f, x0 + xmax + 1); x++) {
			I32 dwn = PIXEL(f, x, y);
			PIXEL(f, x, y) = BOOL(wn);
			wn -= dwn;
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
	I16 xmin = g.xmin*scale - 1, xmax = g.xmax*scale + 1;
	I16 ymin = g.ymin*scale - 1, ymax = g.ymax*scale + 1;
	for (I16 x = CLIPX(f, x0 + xmin); x < CLIPX(f, x0 + xmax + 1); x++)
	for (I16 y = CLIPY(f, y0 - ymax); y < CLIPY(f, y0 - ymin + 1); y++) {
		F64 d = INF;
		for (I i = 0; i < g.nseg; i++) {
			Segment s = g.segs[i];
			I16 x1 = s.x[0]*scale, y1 = s.y[0]*scale;
			I16 x2 = s.x[1]*scale, y2 = s.y[1]*scale;
			I16 x3 = s.x[2]*scale, y3 = s.y[2]*scale;
			if (s.type == SegQuad)
				d = MIN(d, distcurve(x - x0, y0 - y, x1, y1, x2, y2, x3, y3, d));
			else
				d = MIN(d, distline(x - x0, y0 - y, x1, y1, x2, y2));
		}
		*(F32 *)&PIXEL(f, x, y) = fsetsign(d, PIXEL(f, x, y));
	}
	/* NOTE: this code is for debugging */
	/*
	 * for (I16 x = CLIPX(f, x0 + xmin); x < CLIPX(f, x0 + xmax + 1); x++)
	 * for (I16 y = CLIPY(f, y0 - ymax); y < CLIPY(f, y0 - ymin + 1); y++) {
	 * 	F32 d = *(F32 *)&PIXEL(f, x, y);
	 * 	F32 a = smoothstep(5, 0, ABS(d)) * 255;
	 * 	PIXEL(f, x, y) = RGBA(a, a, a, 255);
	 * }
	 */
}

void drawoutline(Image *f, I16 x0, I16 y0, Glyph g, Color c, F64 scale)
{
	for (I i = 0; i < g.nseg; i++) {
		Segment s = g.segs[i];
		I16 x1 = s.x[0]*scale, y1 = s.y[0]*scale;
		I16 x2 = s.x[1]*scale, y2 = s.y[1]*scale;
		I16 x3 = s.x[2]*scale, y3 = s.y[2]*scale;
		if (s.type == SegQuad)
			drawbezier(f, x0+x1, y0-y1, x0+x2, y0-y2, x0+x3, y0-y3, c);
		else
			drawline(f, x0+x1, y0-y1, x0+x2, y0-y2, c);
	}
}
