#include "mlib.h"
#include "types.h"
#include "time.h"
#include "color.h"
#include "image.h"
#include "draw.h"

void drawclear(Image *i, Color c)
{
	for (I32 x = 0; x < i->w; x++)
	for (I32 y = 0; y < i->h; y++)
		PIXEL(i, x, y) = c; /* NOTE: no blending here */
}

static void drawhalftriangle(Image *i, I32 x1, I32 y1, I32 x2, I32 y2, I32 x3, I32 y3, Color c)
{
	if (y1 == y2) {
		if (CHECKY(i, y1)) {
			for (I64 x = CLIPX(i, MIN(x1, x2)); x < CLIPX(i, 1+MAX(x1, x2)); x++)
				PIXEL(i, x, y1) = BLEND(PIXEL(i, x, y1), c);
		}
	} else {
		for (I64 y = CLIPY(i, MIN(y1, y2)); y < CLIPY(i, 1+MAX(y1, y2)); y++) {
			I64 x12 = x1 + DIVROUND((y-y1)*(x2-x1), (y2-y1));
			I64 x13 = x1 + DIVROUND((y-y1)*(x3-x1), (y3-y1));
			for (I64 x = CLIPX(i, MIN(x12, x13)); x < CLIPX(i, 1+MAX(x12, x13)); x++)
				PIXEL(i, x, y) = BLEND(PIXEL(i, x, y), c);
		}
	}
}

void drawtriangle(Image *i, I32 x1, I32 y1, I32 x2, I32 y2, I32 x3, I32 y3, Color c)
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

/* TODO: this is very slow, need to experiment with optimizations */
void drawsmoothtriangle(Image *i, I32 x1, I32 y1, I32 x2, I32 y2, I32 x3, I32 y3, Color c)
{
	const I64 n = 3;
	for (I64 x = CLIPX(i, MIN3(x1, x2, x3)); x < CLIPX(i, 1+MAX3(x1, x2, x3)); x++)
	for (I64 y = CLIPY(i, MIN3(y1, y2, y3)); y < CLIPY(i, 1+MAX3(y1, y2, y3)); y++) {
		I64 hits = 0;
		for (I64 dx = 0; dx < n; dx++)
		for (I64 dy = 0; dy < n; dy++) {
			/* TODO: I think an overflow can happen here, buut... */
			/* NOTE: SIGN((yp - y1)*(x2 - x1) - (xp - x1)*(y2 - y1)) gets us the the orientation
			 * of the point (xp, yp) relative to the line (x1, y1) -> (x2, y2):
			 * -1 (to the left), 0 (on the line) or 1 (to the right) */
			I64 o1 = SIGN(((y - y1)*n + dy)*(x2 - x1) - ((x - x1)*n + dx)*(y2 - y1));
			I64 o2 = SIGN(((y - y2)*n + dy)*(x3 - x2) - ((x - x2)*n + dx)*(y3 - y2));
			I64 o3 = SIGN(((y - y3)*n + dy)*(x1 - x3) - ((x - x3)*n + dx)*(y1 - y3));
			hits += o1*o2 >= 0 && o2*o3 >= 0 && o1*o3 >= 0;
		}
		if (hits)
			PIXEL(i, x, y) = BLEND(PIXEL(i, x, y), RGBA(R(c), G(c), B(c), A(c)*hits/SQUARE(n)));
	}
}

void drawcircle(Image *i, I32 xc, I32 yc, I32 r, Color c)
{
	for (I64 x = CLIPX(i, xc-r); x < CLIPX(i, 1+xc+r); x++)
	for (I64 y = CLIPY(i, yc-r); y < CLIPY(i, 1+yc+r); y++)
		if (SQUARE(x-xc) + SQUARE(y-yc) < SQUARE(r))
			PIXEL(i, x, y) = BLEND(PIXEL(i, x, y), c);
}

void drawsmoothcircle(Image *i, I32 xc, I32 yc, I32 r, Color c)
{
	const I64 n = 3; /* NOTE: looks ok */
	for (I64 x = CLIPX(i, xc-r); x < CLIPX(i, 1+xc+r); x++)
	for (I64 y = CLIPY(i, yc-r); y < CLIPY(i, 1+yc+r); y++) {
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

void drawbresenham(Image *i, I32 x1, I32 y1, I32 x2, I32 y2, Color c)
{
	I64 dx = ABS(x2 - x1), dy = -ABS(y2 - y1);
	I64 sx = SIGN(x2 - x1), sy = SIGN(y2 - y1);
	I64 x = x1, y = y1, e = dx+dy;
	for (;;) {
		if (CHECKX(i, x) && CHECKY(i, y))
			PIXEL(i, x, y) = BLEND(PIXEL(i, x, y), c);
		if (e*2 >= dy) {
			if (x == x2)
				break;
			x += sx;
			e += dy;
		}
		if (e*2 <= dx) {
			if (y == y2)
				break;
			y += sy;
			e += dx;
		}
	}
}

void drawbezier(Image *i, I32 x1, I32 y1, I32 x2, I32 y2, I32 x3, I32 y3, Color c)
{
	const I64 n = 100; /* NOTE: looks ok */
	for (I64 t = 1, xp = x1, yp = y1; t <= n; t++) {
		I64 x = (SQUARE(n-t)*x1 + 2*(n-t)*t*x2 + SQUARE(t)*x3)/SQUARE(n);
		I64 y = (SQUARE(n-t)*y1 + 2*(n-t)*t*y2 + SQUARE(t)*y3)/SQUARE(n);
		drawbresenham(i, xp, yp, x, y, c);
		xp = x, yp = y;
	}
}

void drawrect(Image *i, I32 xtl, I32 ytl, I32 w, I32 h, Color c)
{
	if (w < 0) {
		xtl += w;
		w = -w;
	}
	if (h < 0) {
		ytl += h;
		h = -h;
	}
	for (I32 x = CLIPX(i, xtl); x < CLIPX(i, xtl+w); x++)
	for (I32 y = CLIPY(i, ytl); y < CLIPY(i, ytl+h); y++)
		PIXEL(i, x, y) = BLEND(PIXEL(i, x, y), c);
}

void drawline(Image *i, I32 x1, I32 y1, I32 x2, I32 y2, Color c)
{
	I64 dx = ABS(x2-x1), dy = ABS(y2-y1);
	if (dx + dy == 0)
		return;
	if (dx >= dy) {
		if (x1 > x2) {
			SWAP(x1, x2);
			SWAP(y1, y2);
		}
		for (I64 x = CLIPX(i, x1); x < CLIPX(i, 1+x2); x++) {
			I32 y = y1 + DIVROUND((x-x1)*(y2-y1), (x2-x1));
			if (CHECKY(i, y))
				PIXEL(i, x, y) = BLEND(PIXEL(i, x, y), c);
		}
	} else {
		if (y1 > y2) {
			SWAP(y1, y2);
			SWAP(x1, x2);
		}
		for (I64 y = CLIPY(i, y1); y < CLIPY(i, 1+y2); y++) {
			I32 x = x1 + DIVROUND((y-y1)*(x2-x1), (y2-y1));
			if (CHECKX(i, x))
				PIXEL(i, x, y) = BLEND(PIXEL(i, x, y), c);
		}
	}
}

void drawpixel(Image *i, I32 x, I32 y, Color c)
{
	if (CHECKX(i, x) && CHECKY(i, y))
		PIXEL(i, x, y) = BLEND(PIXEL(i, x, y), c);
}

void drawthickline(Image *i, I32 x1, I32 y1, I32 x2, I32 y2, U8 w, Color c)
{
	const I64 n = 3;
	U64 dx = ABS(x2-x1), dy = ABS(y2-y1);
	if (dx + dy == 0)
		return;
	U64 l2 = dx*dx + dy*dy;
	if (dx >= dy) {
		if (x1 > x2) {
			SWAP(x1, x2);
			SWAP(y1, y2);
		}
		for (I64 x = CLIPX(i, x1-2*w); x < CLIPX(i, x2+2*w+1); x++) {
			I32 yc = y1 + DIVROUND((x-x1)*(y2-y1), (x2-x1));
			for (I64 y = CLIPY(i, yc-2*w); y < CLIPY(i, yc+2*w+1); y++) {
				I64 hits = 0;
				for (I64 dx = 0; dx < n; dx++)
				for (I64 dy = 0; dy < n; dy++) {
					U64 d = ABS((n*(x - x1) + dx)*(y1 - y2) + (n*(y - y1) + dy)*(x2 - x1));
					I64 o1 = (n*(x - x1) + dx)*(x2 - x1) + (n*(y - y1) + dy)*(y2 - y1);
					I64 o2 = (n*(x - x2) + dx)*(x2 - x1) + (n*(y - y2) + dy)*(y2 - y1);
					hits += o1 >= 0 && o2 <= 0 && SQUARE(d) < SQUARE(w)*SQUARE(n)*l2;
				}
				if (hits)
					PIXEL(i, x, y) = BLEND(PIXEL(i, x, y), RGBA(R(c), G(c), B(c), A(c)*hits/SQUARE(n)));
			}
		}
	} else {
		if (y1 > y2) {
			SWAP(y1, y2);
			SWAP(x1, x2);
		}
		for (I64 y = CLIPY(i, y1-2*w); y < CLIPY(i, y2+2*w+1); y++) {
			I32 xc = x1 + DIVROUND((y-y1)*(x2-x1), (y2-y1));
			for (I64 x = CLIPX(i, xc-2*w); x < CLIPX(i, xc+2*w+1); x++) {
				I64 hits = 0;
				for (I64 dy = 0; dy < n; dy++)
				for (I64 dx = 0; dx < n; dx++) {
					U64 d = ABS((n*(y - y1) + dy)*(x1 - x2) + (n*(x - x1) + dx)*(y2 - y1));
					I64 o1 = (n*(y - y1) + dy)*(y2 - y1) + (n*(x - x1) + dx)*(x2 - x1);
					I64 o2 = (n*(y - y2) + dy)*(y2 - y1) + (n*(x - x2) + dx)*(x2 - x1);
					hits += o1 >= 0 && o2 <= 0 && SQUARE(d) < SQUARE(w)*SQUARE(n)*l2;
				}
				if (hits)
					PIXEL(i, x, y) = BLEND(PIXEL(i, x, y), RGBA(R(c), G(c), B(c), A(c)*hits/SQUARE(n)));
			}
		}
	}
}
