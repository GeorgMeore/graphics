#include <stdlib.h>

#include "types.h"
#include "color.h"
#include "image.h"
#include "win.h"
#include "draw.h"

typedef struct {
	int x, y;
} Point;

typedef struct {
	Point *ps;
	int cnt, cap;
} Curve;

void addpoint(Curve *c, int x, int y)
{
	if (c->cnt >= c->cap) {
		c->cap = c->cap*2 + 1;
		c->ps = reallocarray(c->ps, c->cap, sizeof(c->ps[0]));
	}
	c->ps[c->cnt] = (Point){x, y};
	c->cnt += 1;
}

void clearpoints(Curve *c)
{
	c->cnt = 0;
}

void drawpoints(Image *fb, Curve c)
{
	for (int i = 0; i < c.cnt - 1; i++) {
		Point p1 = c.ps[i], p2 = c.ps[i+1];
		drawline(fb, p1.x, p1.y, p2.x, p2.y, BLACK);
	}
}

int main(void)
{
	Curve c = {};
	winopen(640, 480, "paint", 30);
	for (;;) {
		Image *fb = framebegin();
		if (keyisdown('q')) break;
		if (btnisdown(1))
			addpoint(&c, mousex(), mousey());
		else
			clearpoints(&c);
		drawclear(fb, WHITE);
		drawpoints(fb, c);
		frameend();
	}
	return 0;
}
