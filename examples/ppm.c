#include "types.h"
#include "mlib.h"
#include "color.h"
#include "image.h"
#include "draw.h"
#include "win.h"
#include "io.h"

void drawimage(Image *i, I32 xp, I32 yp, Image *s)
{
	for (U16 x = CLIPX(i, xp); x < CLIPX(i, xp+s->w); x++)
	for (U16 y = CLIPY(i, yp); y < CLIPY(i, yp+s->h); y++)
		PIXEL(i, x, y) = PIXEL(s, x - xp, y - yp);
}

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
	while (!keyisdown('q')) {
		Image *f = frame();
		drawclear(f, BLACK);
		drawimage(f, mousex() - i.w/2, mousey() - i.h/2, &i);
	}
	winclose();
	return 0;
}
