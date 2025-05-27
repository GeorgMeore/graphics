#include "types.h"
#include "alloc.h"
#include "mlib.h"
#include "color.h"
#include "image.h"
#include "win.h"
#include "draw.h"
#include "io.h"

void updatepoint(Image *i, int p[2])
{
	static void *curr = 0;
	int mx = mousex(), my = mousey();
	if (!btnisdown(1))
		curr = 0;
	else if (!curr && SQUARE(p[0] - mx) + SQUARE(p[1] - my) < SQUARE(6))
		curr = p;
	if (curr == p) {
		p[0] = CLAMP(mx, 0, i->w);
		p[1] = CLAMP(my, 0, i->h);
	}
}

#define PROFENABLED
#include "prof.h"

int main(int argc, char **argv)
{
	if (argc != 2) {
		println("usage: ", argv[0], " IMAGE.ppm");
		return 1;
	}
	Image i = loadppm(argv[1]);
	if (!i.p) {
		println("error: failed to open the image");
		return 1;
	}
	winopen(600, 600, argv[0], 60);
	int pt[4][2] = {{100, 700}, {100, 100}, {500, 100}, {500, 700}};
	Color c[4] = {RED, GREEN, BLUE, YELLOW};
	int smooth = 1;
	while (!keyisdown('q')) {
		Image *fb = framebegin();
		for (int i = 0; i < 4; i++)
			updatepoint(fb, pt[i]);
		if (keywaspressed('s')) {
			smooth = !smooth;
			profreset("render");
		}
		drawclear(fb, BLACK);
		for (int i = 0; i < 4; i++)
			drawsmoothcircle(fb, pt[i][0], pt[i][1], 5, c[i]);
		drawtriangletexture(
			fb, pt[0][0], pt[0][1], pt[1][0], pt[1][1], pt[2][0], pt[2][1],
			&i, 0, i.h-1, 0, 0, i.w-1, 0);
		drawtriangletexture(
			fb, pt[2][0], pt[2][1], pt[3][0], pt[3][1], pt[0][0], pt[0][1],
			&i, i.w-1, 0, i.w-1, i.h-1, 0, i.h-1);
		//drawtriangletexture(
		//	fb, pt[0][0], pt[0][1], pt[1][0], pt[1][1], pt[2][0], pt[2][1],
		//	&i, 0, i.h-1, 0, 0, i.w-1, 0);
		//drawtriangletexture(
		//	fb, pt[2][0], pt[2][1], pt[3][0], pt[3][1], pt[1][0], pt[1][1],
		//	&i, 0, i.h-1, i.w-1, 0, i.w-1, i.h-1);
		frameend();
	}
	winclose();
	return 0;
}
