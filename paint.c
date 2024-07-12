#include <stdlib.h>

#include "types.h"
#include "color.h"
#include "image.h"
#include "win.h"
#include "draw.h"

typedef struct Point Point;

struct Point {
	int x, y;
	Point *next;
};

Point *point(int x, int y, Point *next)
{
	Point *p = malloc(sizeof(*p));
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
	Curve *c = malloc(sizeof(*c));
	c->last = NULL;
	c->next = next;
	return c;
}

typedef Curve *Picture;

void addpoint(Picture *p, int x, int y)
{
	if (!*p)
		*p = curve(*p);
	(*p)->last = point(x, y, (*p)->last);
}

void undocurve(Picture *p)
{
	Curve *c = *p;
	while (c && !c->last) {
		*p = c->next;
		free(c);
		c = *p;
	}
	if (!c)
		return;
	*p = c->next;
	while (c->last) {
		Point *pt = c->last;
		c->last = pt->next;
		free(pt);
	}
	free(c);
}

void undopoint(Picture *p)
{
	Curve *c = *p;
	while (c && !c->last) {
		*p = c->next;
		free(c);
		c = *p;
	}
	if (!c)
		return;
	Point *pt = c->last;
	c->last = pt->next;
	free(pt);
}

void drawcurves(Image *fb, Picture p)
{
	for (Curve *c = p; c; c = c->next) {
		for (Point *p1 = c->last; p1 && p1->next; p1 = p1->next) {
			Point *p2 = p1->next;
			drawline(fb, p1->x, p1->y, p2->x, p2->y, BLACK);
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
		drawclear(fb, WHITE);
		drawcurves(fb, p);
		frameend();
	}
	winclose();
	return 0;
}
