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

typedef struct {
	I64 x, y;
} Vec;

Vec sub(Vec a, Vec b)
{
	return (Vec){a.x - b.x, a.y - b.y};
}

typedef struct {
	int hit;
	Vec point;
} Isect;

Isect intersect(Vec o1, Vec d1, Vec o2, Vec d2)
{
	I64 det = d2.x*d1.y - d1.x*d2.y;
	Isect i = {};
	if (det == 0)
		return i;
	I64 dox = o2.x - o1.x;
	I64 doy = o2.y - o1.y;
	I64 t1 = d2.x*doy - d2.y*dox;
	I64 t2 = d1.x*doy - d1.y*dox;
	if (det < 0)
		t1 *= -1, t2 *= -1, det *= -1;
	if (t1 < 0 || t2 < 0 || t1 > det || t2 > det)
		return i;
	i.hit = 1;
	i.point.x = (o1.x*det + t1*d1.x)/det;
	i.point.y = (o1.y*det + t1*d1.y)/det;
	return i;
}

Isect segintersect(Vec a1, Vec b1, Vec a2, Vec b2)
{
	return intersect(a1, sub(b1, a1), a2, sub(b2, a2));
}

void drawseg(Image *i, Vec a, Vec b, Color c)
{
	drawthickline(i, a.x, a.y, b.x, b.y, 5, c);
	drawsmoothcircle(i, a.x, a.y, 5, WHITE);
	drawsmoothcircle(i, b.x, b.y, 5, WHITE);
}

int main(int, char **argv)
{
	winopen(600, 600, argv[0], 60);
	Vec s, a = {100, 100}, b = {500, 500};
	int dragging = 0;
	while (!keyisdown('q')) {
		Image *f = framebegin();
		int mx = mousex(), my = mousey();
		drawclear(f, BLACK);
		if (btnisdown(3)) {
			Vec e = {mx, my};
			if (!dragging) {
				s = e;
				dragging = 1;
			}
			drawseg(f, s, e, RGBA(100, 100, 100, 255));
			Isect i = segintersect(s, e, a, b);
			if (i.hit)
				drawseg(f, a, b, RGBA(150, 100, 100, 255));
			else
				drawseg(f, a, b, RGBA(100, 150, 100, 255));
		} else {
			dragging = 0;
			drawseg(f, a, b, RGBA(100, 150, 100, 255));
		}
		frameend();
	}
	winclose();
	return 0;
}
