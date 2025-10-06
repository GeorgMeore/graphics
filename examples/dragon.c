#include "types.h"
#include "color.h"
#include "image.h"
#include "win.h"
#include "draw.h"

void dragon(Image *f, I16 x1, I16 y1, I16 x2, I16 y2, I16 x3, I16 y3, I n, OK o)
{
	if (n) {
		if (o) {
			dragon(f, x1, y1, (x1+x3)/2 + (x2-x3), (y1+y3)/2 + (y2-y3), x2, y2, n-1, 1);
			dragon(f, x2, y2, (x1+x3)/2, (y1+y3)/2, x3, y3, n-1, 0);
		} else {
			dragon(f, x1, y1, (x1+x3)/2, (y1+y3)/2, x2, y2, n-1, 1);
			dragon(f, x2, y2, (x1+x3)/2 + (x2-x1), (y1+y3)/2 + (y2-y1), x3, y3, n-1, 0);
		}
	} else {
		drawline(f, x1, y1, x2, y2, GREEN);
		drawline(f, x2, y2, x3, y3, GREEN);
		drawsmoothcircle(f, x1, y1, 4, BLUE);
		drawsmoothcircle(f, x2, y2, 4, BLUE);
		drawsmoothcircle(f, x3, y3, 4, BLUE);
	}
}

#define SIZE 512

int main(int, char **argv)
{
	winopen(1920, 1080, argv[0], 0);
	U n = 1;
	while (!keyisdown('q')) {
		Image *f = framebegin();
		drawclear(f, BLACK);
		if (btnwaspressed(4))
			n += 1;
		if (btnwaspressed(5) && n)
			n -= 1;
		I x = mousex(), y = mousey();
		dragon(f, x, y, x+SIZE, y+SIZE, x+SIZE+SIZE, y, n, 1);
		frameend();
	}
}
