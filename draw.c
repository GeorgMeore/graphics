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

/* TODO: can I draw smooth triangles with that (maybe do the dot-product stuff for border points)? */
/* TODO: make a textured version of this function and benchmark against the barycentric one */
static void drawhalftriangle(Image *i, I16 x1, I16 y1, I16 x2, I16 y2, I16 x3, I16 y3, Color c)
{
	I64 skip = y1 >= y3; /* NOTE: skip the middle line for the bottom half */
	if (y1 == y2) {
		if (!skip && CHECKY(i, y1)) {
			for (I64 x = CLIPX(i, MIN(x1, x2)); x <= CLIPX(i, MAX(x1, x2)); x++)
				PIXEL(i, x, y1) = BLEND(PIXEL(i, x, y1), c);
		}
	} else {
		for (I64 y = CLIPY(i, MIN(y1, y2)) + skip; y <= CLIPY(i, MAX(y1, y2)); y++) {
			I64 x12 = x1 + DIVROUND((y-y1)*(x2-x1), (y2-y1));
			I64 x13 = x1 + DIVROUND((y-y1)*(x3-x1), (y3-y1));
			for (I64 x = CLIPX(i, MIN(x12, x13)); x <= CLIPX(i, MAX(x12, x13)); x++)
				PIXEL(i, x, y) = BLEND(PIXEL(i, x, y), c);
		}
	}
}

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
	drawhalftriangle(i, x1, y1, x2, y2, x3, y3, c);
	drawhalftriangle(i, x3, y3, x2, y2, x1, y1, c);
}

/* NOTE: SIGN((yp - y1)*(x2 - x1) - (xp - x1)*(y2 - y1)) gets us the the orientation
 * of the point (xp, yp) relative to the line (x1, y1) -> (x2, y2):
 * -1 (to the left), 0 (on the line) or 1 (to the right) */
void drawsmoothtriangle(Image *i, I16 x1, I16 y1, I16 x2, I16 y2, I16 x3, I16 y3, Color c)
{
	const I64 n = 3;
	I64 xmin = CLIPX(i, MIN3(x1, x2, x3)), xmax = CLIPX(i, MAX3(x1, x2, x3));
	I64 ymin = CLIPY(i, MIN3(y1, y2, y3)), ymax = CLIPY(i, MAX3(y1, y2, y3));
	if ((y3 - y1)*(x2 - x1) - (x3 - x1)*(y2 - y1) < 0) {
		SWAP(x2, x3);
		SWAP(y2, y3);
	}
	I64 o1 = n*((ymin - y1)*(x2 - x1) - (xmin - x1)*(y2 - y1));
	I64 o2 = n*((ymin - y2)*(x3 - x2) - (xmin - x2)*(y3 - y2));
	I64 o3 = n*((ymin - y3)*(x1 - x3) - (xmin - x3)*(y1 - y3));
	for (I64 x = xmin; x <= xmax; x++) {
		for (I64 y = ymin; y <= ymax; y++) {
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
				PIXEL(i, x, y) = BLEND(PIXEL(i, x, y), RGBA(R(c), G(c), B(c), A(c)*hits/SQUARE(n)));
		}
		o1 -= n*((x2 - x1)*(ymax - ymin + 1) + (y2 - y1));
		o2 -= n*((x3 - x2)*(ymax - ymin + 1) + (y3 - y2));
		o3 -= n*((x1 - x3)*(ymax - ymin + 1) + (y1 - y3));
	}
}

void drawtriangletexture(Image *i, I16 x1, I16 y1, I16 x2, I16 y2, I16 x3, I16 y3, Image *t, U16 tx1, U16 ty1, U16 tx2, U16 ty2, U16 tx3, U16 ty3)
{
	I64 xmin = CLIPX(i, MIN3(x1, x2, x3)), xmax = CLIPX(i, MAX3(x1, x2, x3));
	I64 ymin = CLIPY(i, MIN3(y1, y2, y3)), ymax = CLIPY(i, MAX3(y1, y2, y3));
	if ((y3 - y1)*(x2 - x1) - (x3 - x1)*(y2 - y1) < 0) {
		SWAP(x2, x3);
		SWAP(y2, y3);
		SWAP(tx2, tx3);
		SWAP(ty2, ty3);
	}
	I64 os = (x1 - x3)*(y2 - y3) - (x2 - x3)*(y1 - y3);
	if (!os)
		return;
	I64 o1 = (ymin - y1)*(x2 - x1) - (xmin - x1)*(y2 - y1);
	I64 o2 = (ymin - y2)*(x3 - x2) - (xmin - x2)*(y3 - y2);
	I64 o3 = (ymin - y3)*(x1 - x3) - (xmin - x3)*(y1 - y3);
	for (I64 y = ymin; y <= ymax; y++) {
		for (I64 x = xmin; x <= xmax; x++) {
			if (o1 >= 0 && o2 >= 0 && o3 >= 0) {
				I64 tx = (tx1*o2 + tx2*o3 + tx3*o1)/os;
				I64 ty = (ty1*o2 + ty2*o3 + ty3*o1)/os;
				PIXEL(i, x, y) = PIXEL(t, tx, ty);
			}
			o1 -= y2 - y1; o2 -= y3 - y2; o3 -= y1 - y3;
		}
		o1 += (y2 - y1)*(xmax - xmin + 1) + (x2 - x1);
		o2 += (y3 - y2)*(xmax - xmin + 1) + (x3 - x2);
		o3 += (y1 - y3)*(xmax - xmin + 1) + (x1 - x3);
	}
}

void drawcircle(Image *i, I16 xc, I16 yc, I16 r, Color c)
{
	for (I64 x = CLIPX(i, xc-r); x <= CLIPX(i, xc+r); x++)
	for (I64 y = CLIPY(i, yc-r); y <= CLIPY(i, yc+r); y++)
		if (SQUARE(x-xc) + SQUARE(y-yc) < SQUARE(r))
			PIXEL(i, x, y) = BLEND(PIXEL(i, x, y), c);
}

void drawsmoothcircle(Image *i, I16 xc, I16 yc, I16 r, Color c)
{
	const I64 n = 3; /* NOTE: looks ok */
	for (I64 x = CLIPX(i, xc-r); x <= CLIPX(i, xc+r); x++)
	for (I64 y = CLIPY(i, yc-r); y <= CLIPY(i, yc+r); y++) {
		/* NOTE: check if the point is definitely inside the circle */
		I64 xo = ABS(x-xc), yo = ABS(y-yc);
		if (xo <= r*43/64 && yo <= r*43/64) {
			PIXEL(i, x, y) = BLEND(PIXEL(i, x, y), c);
		} else {
			I64 hits = 0;
			for (I64 dx = 0; dx < n; dx++)
			for (I64 dy = 0; dy < n; dy++)
				hits += SQUARE(xo*n + dx) + SQUARE(yo*n + dy) < SQUARE(r*n);
			if (hits)
				PIXEL(i, x, y) = BLEND(PIXEL(i, x, y), RGBA(R(c), G(c), B(c), A(c)*hits/SQUARE(n)));
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
		PIXEL(i, x, y) = BLEND(PIXEL(i, x, y), c);
}

void drawline(Image *i, I16 x1, I16 y1, I16 x2, I16 y2, Color c)
{
	I64 dx = ABS(x2-x1), dy = ABS(y2-y1);
	if (dx + dy == 0)
		return;
	if (dx >= dy) {
		if (x1 > x2) {
			SWAP(x1, x2);
			SWAP(y1, y2);
		}
		I64 xmin = CLIPX(i, x1), xmax = CLIPX(i, x2);
		I64 sy = SIGN(y2-y1), d = (xmin-x1)*dy, y = y1 + sy*(d/dx), e = d%dx;
		for (I64 x = xmin; x <= xmax; x++, e += dy) {
			if (e*2 >= dx)
				y += sy, e -= dx;
			if (CHECKY(i, y))
				PIXEL(i, x, y) = BLEND(PIXEL(i, x, y), c);
		}
	} else {
		if (y1 > y2) {
			SWAP(y1, y2);
			SWAP(x1, x2);
		}
		I64 ymin = CLIPY(i, y1), ymax = CLIPY(i, y2);
		I64 sx = SIGN(x2-x1), d = (ymin-y1)*dx, x = x1 + sx*(d/dy), e = d%dy;
		for (I64 y = ymin; y <= ymax; y++, e += dx) {
			if (e*2 >= dy)
				x += sx, e -= dy;
			if (CHECKX(i, x))
				PIXEL(i, x, y) = BLEND(PIXEL(i, x, y), c);
		}
	}
}

void drawbezier(Image *i, I16 x1, I16 y1, I16 x2, I16 y2, I16 x3, I16 y3, Color c)
{
	const I64 n = 100; /* NOTE: looks ok */
	for (I64 t = 1, xp = x1, yp = y1; t <= n; t++) {
		I64 x = (SQUARE(n-t)*x1 + 2*(n-t)*t*x2 + SQUARE(t)*x3)/SQUARE(n);
		I64 y = (SQUARE(n-t)*y1 + 2*(n-t)*t*y2 + SQUARE(t)*y3)/SQUARE(n);
		drawline(i, xp, yp, x, y, c);
		xp = x, yp = y;
	}
}

void drawpixel(Image *i, I16 x, I16 y, Color c)
{
	if (CHECKX(i, x) && CHECKY(i, y))
		PIXEL(i, x, y) = BLEND(PIXEL(i, x, y), c);
}

static void drawthicknonsteep(Image *i, U8 flip, I16 x1, I16 y1, I16 x2, I16 y2, U8 w, Color c)
{
	const I64 n = 3;
	U64 l2 = SQUARE(x2-x1) + SQUARE(y2-y1);
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
					hits += o1 >= 0 && o2 <= 0 && SQUARE(d) < SQUARE(w)*SQUARE(n)*l2;
					o1 += y2 - y1, o2 += y2 - y1, d += x2 - x1;
				}
				o1 -= n*(y2 - y1) - (x2 - x1);
				o2 -= n*(y2 - y1) - (x2 - x1);
				d  -= n*(x2 - x1) - (y1 - y2);
			}
			o1 += n*(y2-y1 - x2+x1), o2 += n*(y2-y1 - x2+x1), d += n*(x2-x1 + y2-y1);
			if (hits) {
				if (flip)
					PIXEL(i, y, x) = BLEND(PIXEL(i, y, x), RGBA(R(c), G(c), B(c), A(c)*hits/SQUARE(n)));
				else
					PIXEL(i, x, y) = BLEND(PIXEL(i, x, y), RGBA(R(c), G(c), B(c), A(c)*hits/SQUARE(n)));
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
