#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>
#include <time.h>

#include "types.h"
#include "color.h"
#include "image.h"
#include "win.h"
#include "draw.h"

//#define PROFENABLED
#include "prof.h"

void drawscene(Image *i, int dx, int dy)
{
	drawclear(i, RGBA(18, 18, 18, 255));

	/* A bunch of lines */
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

	drawcircle(i, 1000+dx, 900+dy, 10, GREEN);
	drawcircle(i, 1500+dx, 900+dy, 10, GREEN);
	drawcircle(i, 1300+dx, 1000+dy, 10, GREEN);
	drawtriangle(i, 1000+dx, 900+dy, 1500+dx, 900+dy, 1300+dx, 1000+dy, GREEN);

	drawcircle(i, 1000+dx, 600+dy, 10, GREEN);
	drawcircle(i, 1500+dx, 600+dy, 10, GREEN);
	drawcircle(i, 1300+dx, 500+dy, 10, GREEN);
	drawtriangle(i, 1000+dx, 600+dy, 1500+dx, 600+dy, 1300+dx, 500+dy, GREEN);

	drawtriangle(i, 0+dx, 0+dy, 100+dx, 0+dy, 100+dx, 100+dy, BLUE);
}

int main()
{
	int dx = 0, dy = 0;
	winopen(600, 600, "Example");
	for (;;) {
		PROFBEGIN("drawing");
		drawscene(&fb, dx, dy);
		PROFEND();

		PROFBEGIN("input handling");
		if (keyisdown('q')) break;
		if (keyisdown('d')) dx -= 10;
		if (keyisdown('a')) dx += 10;
		if (keyisdown('s')) dy -= 10;
		if (keyisdown('w')) dy += 10;
		PROFEND();

		PROFBEGIN("polling");
		winpoll();
		PROFEND();
	}
	winclose();
	return 0;
}
