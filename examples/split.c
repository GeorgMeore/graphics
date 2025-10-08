#include "types.h"
#include "color.h"
#include "image.h"
#include "win.h"
#include "draw.h"

#define PROFENABLED
#include "prof.h"

void drawshapes(Image *i, int dx, int dy)
{
	drawclear(i, BLACK);

	drawline(i, 400+dx, 200+dy, -200+dx, 200+dy, GREEN);
	drawline(i, 100+dx, 200+dy, -200+dx, 351+dy, GREEN);
	drawline(i, 100+dx, 200+dy, -200+dx, 49+dy, GREEN);
	drawline(i, 100+dx, 200+dy, 400+dx, 351+dy, GREEN);
	drawline(i, 100+dx, 200+dy, 400+dx, 49+dy, GREEN);
	drawline(i, 100+dx, 200+dy, 150+dx, 400+dy, GREEN);

	drawline(i, 400+dx, 200+dy, 300+dx, 300+dy, BLUE);
	drawline(i, 300+dx, 300+dy, 200+dx, 300+dy, BLUE);
	drawline(i, 200+dx, 300+dy, 100+dx, 200+dy, BLUE);
	drawline(i, 100+dx, 200+dy, 150+dx, 50+dy, BLUE);
	drawline(i, 150+dx, 50+dy, 350+dx, 50+dy, BLUE);
	drawline(i, 350+dx, 50+dy, 400+dx, 200+dy, BLUE);

	drawrect(i, 300+dx, 300+dy, 40, 40, RED);

	drawcircle(i, 300+dx, 300+dy, 10, GREEN);
	drawcircle(i, 500+dx, 200+dy, 10, GREEN);
	drawcircle(i, 600+dx, 400+dy, 10, GREEN);
	drawtriangle(i, 300+dx, 300+dy, 500+dx, 200+dy, 600+dx, 400+dy, GREEN);

	drawcircle(i, 800+dx, 600+dy, 10, GREEN);
	drawcircle(i, 900+dx, 700+dy, 10, GREEN);
	drawcircle(i, 900+dx, 900+dy, 10, GREEN);
	drawtriangle(i, 800+dx, 600+dy, 900+dx, 700+dy, 900+dx, 900+dy, GREEN);

	drawsmoothcircle(i, 1000+dx, 900+dy, 10, GREEN);
	drawsmoothcircle(i, 1500+dx, 900+dy, 10, GREEN);
	drawsmoothcircle(i, 1300+dx, 1000+dy, 10, GREEN);
	drawtriangle(i, 1000+dx, 900+dy, 1500+dx, 900+dy, 1300+dx, 1000+dy, GREEN);

	drawsmoothcircle(i, 1000+dx, 600+dy, 10, GREEN);
	drawsmoothcircle(i, 1500+dx, 600+dy, 10, GREEN);
	drawsmoothcircle(i, 1300+dx, 500+dy, 10, GREEN);
	drawtriangle(i, 1000+dx, 600+dy, 1500+dx, 600+dy, 1300+dx, 500+dy, GREEN);

	drawtriangle(i, 0+dx, 0+dy, 100+dx, 0+dy, 100+dx, 100+dy, BLUE);
}

void drawgradients(Image *i, int dx, int dy)
{
	for (int x = 0; x < i->w; x++)
	for (int y = 0; y < i->h; y++)
		PIXEL(i, x, y) = RGBA(0, (U8)(x-dx), (U8)(y-dy), 0);
}

int main()
{
	int dx = 0, dy = 0;
	winopen(600, 600, "Example", 60);
	for (;;) {
		profbegin("frame preparation");
		Image *f = frame();
		Image l = subimage(*f, 0, 0, f->w/2, f->h);
		Image r = subimage(*f, f->w/2, 0, f->w - f->w/2, f->h);
		int mx = mousex(), my = mousey();
		profend();

		profbegin("drawing");
		drawgradients(&l, dy, dx);
		drawshapes(&r, dx, dy);
		drawrect(f, mx, my, 50, 50, RGBA(0, 0, 255, 100));
		drawrect(f, mx, my, -50, -50, RGBA(200, 200, 0, 100));
		drawrect(f, mx, my, 50, -50, RGBA(255, 0, 0, 100));
		drawrect(f, mx, my, -50, 50, RGBA(255, 0, 255, 100));
		profend();

		if (keyisdown('q')) break;
		if (keyisdown('d')) dx -= 4;
		if (keyisdown('a')) dx += 4;
		if (keyisdown('s')) dy -= 4;
		if (keyisdown('w')) dy += 4;

		profdump();
	}
	winclose();
	return 0;
}
