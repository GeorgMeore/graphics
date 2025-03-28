#include "types.h"
#include "alloc.h"
#include "color.h"
#include "image.h"
#include "win.h"
#include "draw.h"

#define LINECOLOR RGBA(240, 240, 240, 255)
#define BGCOLOR RGBA(18, 18, 18, 255)

typedef struct Point Point;

struct Point {
	int x, y;
	Point *next;
};

Point *point(int x, int y, Point *next)
{
	Point *p = memalloc(sizeof(*p));
	p->x = x;
	p->y = y;
	p->next = next;
	return p;
}

typedef struct Curve Curve;

struct Curve {
	Point *last;
	Curve *next;
};

Curve *curve(Curve *next)
{
	Curve *c = memalloc(sizeof(*c));
	c->last = 0;
	c->next = next;
	return c;
}

typedef Curve *Picture;

void addpoint(Picture *p, int x, int y)
{
	if (!*p)
		*p = curve(*p);
	Point *pt = (*p)->last;
	if (pt && pt->x == x && pt->y == y)
		return;
	(*p)->last = point(x, y, pt);
}

void undocurve(Picture *p)
{
	Curve *c = *p;
	while (c && !c->last) {
		*p = c->next;
		memfree(c);
		c = *p;
	}
	if (!c)
		return;
	*p = c->next;
	while (c->last) {
		Point *pt = c->last;
		c->last = pt->next;
		memfree(pt);
	}
	memfree(c);
}

void undopoint(Picture *p)
{
	Curve *c = *p;
	while (c && !c->last) {
		*p = c->next;
		memfree(c);
		c = *p;
	}
	if (!c)
		return;
	Point *pt = c->last;
	c->last = pt->next;
	memfree(pt);
}

void drawcurves(Image *fb, Picture p)
{
	for (Curve *c = p; c; c = c->next) {
		for (Point *p1 = c->last; p1 && p1->next; p1 = p1->next) {
			Point *p2 = p1->next;
			drawline(fb, p1->x, p1->y, p2->x, p2->y, LINECOLOR);
		}
	}
}

void endcurve(Picture *p)
{
	if (!*p || !(*p)->last)
		return;
	*p = curve(*p);
}

int main(void)
{
	Picture p = {};
	winopen(640, 480, "paint", 60);
	for (;;) {
		Image *fb = framebegin();
		if (keywaspressed('q'))
			break;
		if (keyisdown('u'))
			undopoint(&p);
		if (keywaspressed('y'))
			undocurve(&p);
		if (btnisdown(1))
			addpoint(&p, mousex(), mousey());
		else
			endcurve(&p);
		drawclear(fb, BGCOLOR);
		drawcurves(fb, p);
		if (keywaspressed('s'))
			saveppm(fb, "out.ppm");
		frameend();
	}
	winclose();
	return 0;
}
