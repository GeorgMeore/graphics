#include "mlib.h"
#include "types.h"
#include "time.h"
#include "color.h"
#include "image.h"
#include "draw.h"

/* IDEA: introduce a separate "shadering" rendering step, that would handle
 * pixel-by-pixel-rendered objects in pararallel using multiple threads */

void drawclear(Image *i, Color c)
{
	for (I64 x = 0; x < i->w; x++)
	for (I64 y = 0; y < i->h; y++)
		PIXEL(i, x, y) = c; /* NOTE: no blending here */
}

/* TODO: an aa version that uses covered pixel area as opacity */
/* TODO: make a textured version of this function and benchmark against the barycentric one */
void drawtriangle(Image *i, I16 x1, I16 y1, I16 x2, I16 y2, I16 x3, I16 y3, Color c)
{
	if (y3 < y1) {
		SWAP(x1, x3);
		SWAP(y1, y3);
	}
	if (y3 < y2) {
		SWAP(x2, x3);
		SWAP(y2, y3);
	} else if (y2 < y1) {
		SWAP(x1, x2);
		SWAP(y1, y2);
	}
	for (I64 y = CLIPY(i, y1); y < CLIPY(i, y2); y++) {
		I64 x12 = x1 + DIVROUND((2*(y - y1) + 1)*(x2 - x1), 2*(y2 - y1));
		I64 x13 = x1 + DIVROUND((2*(y - y1) + 1)*(x3 - x1), 2*(y3 - y1));
		for (I64 x = CLIPX(i, MIN(x12, x13)); x < CLIPX(i, MAX(x12, x13)+1); x++)
			PIXEL(i, x, y) = blend(PIXEL(i, x, y), c);
	}
	for (I64 y = CLIPY(i, y2); y < CLIPY(i, y3); y++) {
		I64 x23 = x2 + DIVROUND((2*(y - y2) + 1)*(x3 - x2), 2*(y3 - y2));
		I64 x13 = x1 + DIVROUND((2*(y - y1) + 1)*(x3 - x1), 2*(y3 - y1));
		for (I64 x = CLIPX(i, MIN(x13, x23)); x < CLIPX(i, MAX(x13, x23)+1); x++)
			PIXEL(i, x, y) = blend(PIXEL(i, x, y), c);
	}
}

/* NOTE: For any vector (x, y) (y, -x) will be it`s 90-degree clockwise rotation.
 * Using this insight we can easily check if a point p=(xp, yp) is to the left or
 * to the right of a line p1=(x1, y1) -> p2=(x2, y2) by calculating the dot product
 * between (p - p1) and the right normal to the (p2 - p1): it`s sign will give us
 * orientation (-1 (to the left), 0 (on the line) or 1 (to the right)) and it`s
 * magnitude will be equal to the area of the parallelogram formed by
 * (p2 - p1) and (p - p1). This gives you a way of checking if a point is inside
 * a triangle and doing barycentric interpolation. */
void drawsmoothtriangle(Image *i, I16 x1, I16 y1, I16 x2, I16 y2, I16 x3, I16 y3, Color c)
{
	const I64 n = 3;
	I64 xmin = CLIPX(i, MIN3(x1, x2, x3)), xmax = CLIPX(i, MAX3(x1, x2, x3)+1);
	I64 ymin = CLIPY(i, MIN3(y1, y2, y3)), ymax = CLIPY(i, MAX3(y1, y2, y3)+1);
	/* NOTE: fix the winding order if the third vertex is to the left of the other two*/
	if ((y3 - y1)*(x2 - x1) - (x3 - x1)*(y2 - y1) < 0) {
		SWAP(x2, x3);
		SWAP(y2, y3);
	}
	I64 o1 = n*((ymin - y1)*(x2 - x1) - (xmin - x1)*(y2 - y1));
	I64 o2 = n*((ymin - y2)*(x3 - x2) - (xmin - x2)*(y3 - y2));
	I64 o3 = n*((ymin - y3)*(x1 - x3) - (xmin - x3)*(y1 - y3));
	for (I64 x = xmin; x < xmax; x++) {
		for (I64 y = ymin; y < ymax; y++) {
			I64 hits = 0;
			for (I64 dx = 0; dx < n; dx++) {
				for (I64 dy = 0; dy < n; dy++) {
					hits += o1 >= 0 && o2 >= 0 && o3 >= 0;
					o1 += x2 - x1, o2 += x3 - x2, o3 += x1 - x3;
				}
				o1 -= (x2 - x1)*n + (y2 - y1);
				o2 -= (x3 - x2)*n + (y3 - y2);
				o3 -= (x1 - x3)*n + (y1 - y3);
			}
			o1 += n*(x2-x1 + y2-y1), o2 += n*(x3-x2 + y3-y2), o3 += n*(x1-x3 + y1-y3);
			if (hits)
				PIXEL(i, x, y) = blend(PIXEL(i, x, y), RGBA(R(c), G(c), B(c), A(c)*hits/SQUARE(n)));
		}
		o1 -= n*((x2 - x1)*(ymax - ymin) + (y2 - y1));
		o2 -= n*((x3 - x2)*(ymax - ymin) + (y3 - y2));
		o3 -= n*((x1 - x3)*(ymax - ymin) + (y1 - y3));
	}
}

void drawcircle(Image *i, I16 xc, I16 yc, I16 r, Color c)
{
	for (I64 x = CLIPX(i, xc-r); x < CLIPX(i, xc+r+1); x++)
	for (I64 y = CLIPY(i, yc-r); y < CLIPY(i, yc+r+1); y++)
		if (SQUARE(x-xc) + SQUARE(y-yc) < SQUARE(r))
			PIXEL(i, x, y) = blend(PIXEL(i, x, y), c);
}

void drawsmoothcircle(Image *i, I16 xc, I16 yc, I16 r, Color c)
{
	const I64 n = 3; /* NOTE: looks ok */
	for (I64 x = CLIPX(i, xc-r); x < CLIPX(i, xc+r+1); x++)
	for (I64 y = CLIPY(i, yc-r); y < CLIPY(i, yc+r+1); y++) {
		/* NOTE: check if the point is definitely inside the circle */
		I64 xo = ABS(x-xc), yo = ABS(y-yc);
		if (xo <= r*45/64 && yo <= r*45/64) {
			PIXEL(i, x, y) = blend(PIXEL(i, x, y), c);
		} else {
			I64 hits = 0;
			for (I64 dx = 0; dx < n; dx++)
			for (I64 dy = 0; dy < n; dy++)
				hits += SQUARE(xo*n + dx) + SQUARE(yo*n + dy) < SQUARE(r*n);
			if (hits)
				PIXEL(i, x, y) = blend(PIXEL(i, x, y), RGBA(R(c), G(c), B(c), A(c)*hits/SQUARE(n)));
		}
	}
}

void drawrect(Image *i, I16 xtl, I16 ytl, I16 w, I16 h, Color c)
{
	if (w < 0) {
		xtl += w;
		w = -w;
	}
	if (h < 0) {
		ytl += h;
		h = -h;
	}
	for (I64 x = CLIPX(i, xtl); x < CLIPX(i, xtl+w); x++)
	for (I64 y = CLIPY(i, ytl); y < CLIPY(i, ytl+h); y++)
		PIXEL(i, x, y) = blend(PIXEL(i, x, y), c);
}

/* TODO: a comment on how this works */
void drawline(Image *i, I16 x1, I16 y1, I16 x2, I16 y2, Color c)
{
	I64 dx = ABS(x2-x1), dy = ABS(y2-y1);
	if (dx + dy == 0)
		return;
	/* NOTE: we always exclude (x2, y2), because it solves the problem
	 * of double-painting pixels when we draw a contour of semi-transparent lines. */
	if (dx >= dy) {
		I64 xmin, xmax, sy, d, y;
		if (x1 > x2) {
			xmin = CLIPX(i, x2+1), xmax = CLIPX(i, x1+1);
			sy = SIGN(y1-y2), d = (xmin-x2)*dy, y = y2 + sy*(d/dx);
		} else {
			xmin = CLIPX(i, x1), xmax = CLIPX(i, x2);
			sy = SIGN(y2-y1), d = (xmin-x1)*dy, y = y1 + sy*(d/dx);
		}
		for (I64 x = xmin, e = d%dx; x < xmax; x++, e += dy) {
			if (e*2 >= dx)
				y += sy, e -= dx;
			if (CHECKY(i, y))
				PIXEL(i, x, y) = blend(PIXEL(i, x, y), c);
		}
	} else {
		I64 ymin, ymax, sx, d, x;
		if (y1 > y2) {
			ymin = CLIPY(i, y2+1), ymax = CLIPY(i, y1+1);
			sx = SIGN(x1-x2), d = (ymin-y2)*dx, x = x2 + sx*(d/dy);
		} else {
			ymin = CLIPY(i, y1), ymax = CLIPY(i, y2);
			sx = SIGN(x2-x1), d = (ymin-y1)*dx, x = x1 + sx*(d/dy);
		}
		for (I64 y = ymin, e = d%dy; y < ymax; y++, e += dx) {
			if (e*2 >= dy)
				x += sx, e -= dy;
			if (CHECKX(i, x))
				PIXEL(i, x, y) = blend(PIXEL(i, x, y), c);
		}
	}
}

void drawbezier(Image *i, I16 x1, I16 y1, I16 x2, I16 y2, I16 x3, I16 y3, Color c)
{
	const I64 n = 64; /* NOTE: looks ok */
	for (I64 t = 1, xp = x1, yp = y1; t <= n; t++) {
		I64 x = DIVROUND(SQUARE(n-t)*x1 + 2*(n-t)*t*x2 + SQUARE(t)*x3, SQUARE(n));
		I64 y = DIVROUND(SQUARE(n-t)*y1 + 2*(n-t)*t*y2 + SQUARE(t)*y3, SQUARE(n));
		drawline(i, xp, yp, x, y, c);
		xp = x, yp = y;
	}
}

void drawpixel(Image *i, I16 x, I16 y, Color c)
{
	if (CHECKX(i, x) && CHECKY(i, y))
		PIXEL(i, x, y) = blend(PIXEL(i, x, y), c);
}

static void drawthicknonsteep(Image *i, U8 flip, I16 x1, I16 y1, I16 x2, I16 y2, U8 w, Color c)
{
	const I64 n = 3;
	U64 l2 = SQUARE(x2-x1) + SQUARE(y2-y1);
	U64 wn2 = SQUARE(w) * SQUARE(n);
	if (x1 > x2) {
		SWAP(x1, x2);
		SWAP(y1, y2);
	}
	I64 xlim = flip*i->h + (1 - flip)*i->w - 1;
	I64 ylim = flip*i->w + (1 - flip)*i->h - 1;
	I64 xmin = CLAMP(x1 - 2*w, 0, xlim), xmax = CLAMP(x2 + 2*w+1, 0, xlim);
	for (I64 x = xmin; x < xmax; x++) {
		I64 yc = y1 + DIVROUND((x - x1)*(y2 - y1), (x2 - x1));
		I64 ymin = CLAMP(yc-2*w, 0, ylim), ymax = CLAMP(yc+2*w+1, 0, ylim);
		/* NOTE: a fully incremental version I tried didn't perform better */
		I64 o1 = n*((x - x1)*(x2 - x1) + (ymin - y1)*(y2 - y1));
		I64 o2 = n*((x - x2)*(x2 - x1) + (ymin - y2)*(y2 - y1));
		I64 d  = n*((x - x1)*(y1 - y2) + (ymin - y1)*(x2 - x1));
		for (I64 y = ymin; y < ymax; y++) {
			I64 hits = 0;
			for (I64 dx = 0; dx < n; dx++) {
				for (I64 dy = 0; dy < n; dy++) {
					hits += o1 >= 0 && o2 <= 0 && SQUARE(d) < wn2*l2;
					o1 += y2 - y1, o2 += y2 - y1, d += x2 - x1;
				}
				o1 -= n*(y2 - y1) - (x2 - x1);
				o2 -= n*(y2 - y1) - (x2 - x1);
				d  -= n*(x2 - x1) - (y1 - y2);
			}
			o1 += n*(y2-y1 - x2+x1), o2 += n*(y2-y1 - x2+x1), d += n*(x2-x1 + y2-y1);
			if (hits) {
				Color p = RGBA(R(c), G(c), B(c), A(c)*hits/SQUARE(n));
				if (flip)
					PIXEL(i, y, x) = blend(PIXEL(i, y, x), p);
				else
					PIXEL(i, x, y) = blend(PIXEL(i, x, y), p);
			}
		}
	}
}

void drawthickline(Image *i, I16 x1, I16 y1, I16 x2, I16 y2, U8 w, Color c)
{
	if (ABS(x2-x1) + ABS(y2-y1) == 0)
		return;
	if (ABS(x2-x1) >= ABS(y2-y1))
		drawthicknonsteep(i, 0, x1, y1, x2, y2, w, c);
	else
		drawthicknonsteep(i, 1, y1, x1, y2, x2, w, c);
}
