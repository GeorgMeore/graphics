#include "types.h"
#include "alloc.h"
#include "mlib.h"
#include "color.h"
#include "image.h"
#include "win.h"
#include "draw.h"

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

int main(void)
{
	winopen(600, 600, "Line", 60);
	int pt[2][2] = {{100, 100}, {200, 200}};
	Color c[2] = {RED, GREEN};
	while (!keyisdown('q')) {
		Image *fb = framebegin();
		for (int i = 0; i < 2; i++)
			updatepoint(fb, pt[i]);
		drawclear(fb, BLACK);
		for (int i = 0; i < 2; i++)
			drawsmoothcircle(fb, pt[i][0], pt[i][1], 5, c[i]);
		drawbresenham(fb, pt[0][0], pt[0][1], pt[1][0], pt[1][1], WHITE);
		frameend();
	}
	winclose();
	return 0;
}
