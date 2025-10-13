#include "types.h"
#include "math.h"
#include "alloc.h"
#include "mlib.h"
#include "color.h"
#include "image.h"
#include "win.h"
#include "draw.h"

void updatepoint(Image *i, I p[2])
{
	static void *curr = 0;
	I mx = mousex(), my = mousey();
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
	winopen(600, 600, "Circle", 60);
	I pt[2][2] = {{100, 100}, {200, 200}};
	Color c[2] = {RED, GREEN};
	OK smooth = 1;
	while (!keyisdown('q')) {
		Image *f = frame();
		for (I i = 0; i < 2; i++)
			updatepoint(f, pt[i]);
		if (keywaspressed('s'))
			smooth = !smooth;
		drawclear(f, BLACK);
		if (smooth)
			drawsmoothcircle(f, pt[0][0], pt[0][1], isqrt(SQUARE(pt[1][0]-pt[0][0]) + SQUARE(pt[1][1]-pt[0][1])), WHITE);
		else
			drawcircle(f, pt[0][0], pt[0][1], isqrt(SQUARE(pt[1][0]-pt[0][0]) + SQUARE(pt[1][1]-pt[0][1])), WHITE);
		for (I i = 0; i < 2; i++)
			drawsmoothcircle(f, pt[i][0], pt[i][1], 5, c[i]);
	}
	winclose();
	return 0;
}
