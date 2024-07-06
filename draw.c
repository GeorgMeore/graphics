#include "imath.h"
#include "types.h"
#include "util.h"
#include "color.h"
#include "image.h"
#include "draw.h"

void drawclear(Image *i, Color c)
{
	for (int x = 0; x < i->w; x++)
	for (int y = 0; y < i->h; y++)
		PIXEL(i, x, y) = c;
}

static void drawhalftriangle(Image *i, int x1, int y1, int x2, int y2, int x3, int y3, Color c)
{
	if (y1 == y2) {
		if (CHECKY(i, y1)) {
			for (int x = CLIPX(i, MIN(x1, x2)); x < CLIPX(i, 1+MAX(x1, x2)); x++)
				PIXEL(i, x, y1) = c;
		}
	} else {
		for (int y = CLIPY(i, MIN(y1, y2)); y < CLIPY(i, 1+MAX(y1, y2)); y++) {
			int x12 = x1 + DIVROUND((y-y1)*(x2-x1), (y2-y1));
			int x13 = x1 + DIVROUND((y-y1)*(x3-x1), (y3-y1));
			for (int x = CLIPX(i, MIN(x12, x13)); x < CLIPX(i, 1+MAX(x12, x13)); x++)
				PIXEL(i, x, y) = c;
		}
	}
}

void drawtriangle(Image *i, int x1, int y1, int x2, int y2, int x3, int y3, Color c)
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

void drawcircle(Image *i, int xc, int yc, int r, Color c)
{
	for (int x = CLIPX(i, xc-r); x < CLIPX(i, 1+xc+r); x++)
	for (int y = CLIPY(i, yc-r); y < CLIPY(i, 1+yc+r); y++)
		if (SQUARE(x-xc) + SQUARE(y-yc) < SQUARE(r))
			PIXEL(i, x, y) = c;
}

void drawrect(Image *i, int xtl, int ytl, int w, int h, Color c)
{
	for (int x = CLIPX(i, xtl); x < CLIPX(i, xtl+w); x++)
	for (int y = CLIPY(i, ytl); y < CLIPY(i, ytl+h); y++)
		PIXEL(i, x, y) = c;
}

/* TODO: check out Bresenham's Algorithm */
void drawline(Image *i, int x1, int y1, int x2, int y2, Color c)
{
	int dx2 = SQUARE(x2-x1), dy2 = SQUARE(y2-y1);
	if (dx2 == 0 && dy2 == 0)
		return;
	if (dx2 >= dy2) {
		if (x1 > x2) {
			SWAP(x1, x2);
			SWAP(y1, y2);
		}
		for (int x = CLIPX(i, x1); x < CLIPX(i, 1+x2); x++) {
			int y = y1 + DIVROUND((x-x1)*(y2-y1), (x2-x1));
			if (CHECKY(i, y))
				PIXEL(i, x, y) = c;
		}
	}
	else {
		if (y1 > y2) {
			SWAP(y1, y2);
			SWAP(x1, x2);
		}
		for (int y = CLIPY(i, y1); y < CLIPY(i, 1+y2); y++) {
			int x = x1 + DIVROUND((y-y1)*(x2-x1), (y2-y1));
			if (CHECKX(i, x))
				PIXEL(i, x, y) = c;
		}
	}
}
