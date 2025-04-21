#include "types.h"
#include "alloc.h"
#include "mlib.h"
#include "color.h"
#include "image.h"
#include "win.h"
#include "draw.h"
#include "math.h"

typedef struct Bezier2 Bezier2;
struct Bezier2 {
	int     pt[3][2];
	Bezier2 *next;
};

Bezier2 *bezier2(int pt[3][2], Bezier2 *next)
{
	Bezier2 *c = memalloc(sizeof(*c));
	for (int i = 0; i < 3; i++)
	for (int j = 0; j < 2; j++)
		c->pt[i][j] = pt[i][j];
	c->next = next;
	return c;
}

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

void drawbeziertop(Image *i, int pt[3][2])
{
	F64 tm = (pt[1][1] - pt[0][1])/(F64)(2*pt[1][1] - pt[0][1] - pt[2][1]);
	F64 x = (1 - tm)*(1 - tm)*pt[0][0] + 2*(1 - tm)*tm*pt[1][0] + tm*tm*pt[2][0];
	F64 y = (1 - tm)*(1 - tm)*pt[0][1] + 2*(1 - tm)*tm*pt[1][1] + tm*tm*pt[2][1];
	if (tm >= 0 && tm <= 1)
		drawsmoothcircle(i, x, y, 5, GREEN);
}

void drawbezierisect(Image *i, int pt[3][2], int xr, int yr)
{
	F64 a = pt[0][1] - 2*pt[1][1] + pt[2][1];
	F64 b = 2*(pt[1][1] - pt[0][1]);
	F64 c = pt[0][1] - yr;
	F64 d = b*b - 4*a*c;
	if (d < 0 || a == 0)
		return;
	F64 t1 = (-b + fsqrt(d))/(2*a), t2 = (-b - fsqrt(d))/(2*a);
	if (t1 >= 0 && t1 <= 1) {
		F64 x = (1 - t1)*(1 - t1)*pt[0][0] + 2*(1 - t1)*t1*pt[1][0] + t1*t1*pt[2][0];
		F64 y = (1 - t1)*(1 - t1)*pt[0][1] + 2*(1 - t1)*t1*pt[1][1] + t1*t1*pt[2][1];
		if (x >= xr)
			drawsmoothcircle(i, x, y, 5, BLUE);
	}
	if (t2 >= 0 && t2 <= 1) {
		F64 x = (1 - t2)*(1 - t2)*pt[0][0] + 2*(1 - t2)*t2*pt[1][0] + t2*t2*pt[2][0];
		F64 y = (1 - t2)*(1 - t2)*pt[0][1] + 2*(1 - t2)*t2*pt[1][1] + t2*t2*pt[2][1];
		if (x >= xr)
			drawsmoothcircle(i, x, y, 5, BLUE);
	}
}

void drawbezier2(Image *i, F64 x1, F64 y1, F64 x2, F64 y2, F64 x3, F64 y3, Color c)
{
	I16 x = ((x1 + x2) + (x2 + x3)) / 4;
	I16 y = ((y1 + y2) + (y2 + y3)) / 4;
	if (ABS(x1 - x2) + ABS(y1 - y2) < 5) {
		drawline(i, x1, y1, x, y, c);
		drawline(i, x, y, x3, y3, c);
	} else {
		drawbezier2(i, x1, y1, (x1+x2)/2, (y1+y2)/2, x, y, c);
		drawbezier2(i, x, y, (x2+x3)/2, (y2+y3)/2, x3, y3, c);
	}
}

#if 0

Trying to figure out how to find the minimum distance between a point and a quadratic curve
Looks like it would make sense to implement a general numeric root computation
(it would also allow me to compute distances to higher order curves)

#include "panic.h"

void drawbezier3(Image *i, I16 x1, I16 y1, I16 x2, I16 y2, I16 x3, I16 y3, Color c)
{
	/*
	P = (a, b)
	B(t) = (1 - t)^2 P0 + 2(1 - t)t P1 + t^2 P2
	     = (1 - 2t + t^2) P0 + 2(t - t^2) P1 + t^2 P2
	     = t^2 (P2 - 2P1 + P0) + 2t (P1 - P0) + P0

	d(t) = (t^2 (x2 - 2x1 + x0) + 2t (x1 - x0) + (x0 - a))^2 + (t^2 (y2 - 2y1 + y0) + 2t (y1 - y0) + (y0 - b))
      ] c = (x2 - 2x1 + x0), d = 2(x1 - x0), e = (x0 - a)
        f = (y2 - 2y1 + y0), g = 2(y1 - y0), h = (y0 - b)

    d/dt (c*t^2 + d*t + e)^2 = 2(c*t^2 + d*t + e)(2c*t + d) = 2(2c^2 t^3 + 3*d*c t^2 + (d^2 + 2*c*e) t + e*d)

	We need to find the root of the derivative, which is a cubic polynomial of t
	*/
	for (I16 x = CLIPX(i, MIN3(x1, x2, x3)); x <= CLIPX(i, MAX3(x1, x2, x3)); x++)
	for (I16 y = CLIPY(i, MIN3(y1, y2, y3)); y <= CLIPY(i, MAX3(y1, y2, y3)); y++) {
		F64 c1x = (x3 + x1 - 2*x2), c2x = 2*(x2 - x1), c3x = (x1 - x);
		F64 c1y = (y3 + y1 - 2*y2), c2y = 2*(y2 - y1), c3y = (y1 - y);
		F64 ax = 2*c1x*c1x, bx = 3*c1x*c2x, cx = c2x*c2x + 2*c1x*c3x, dx = c2x*c3x;
		F64 ay = 2*c1y*c1y, by = 3*c1y*c2y, cy = c2y*c2y + 2*c1y*c3y, dy = c2y*c3y;
		F64 a = ax+ay, b = bx+by, c = cx+cy, d = dx+dy;
		F64 p = (3*a*c - b*b)/(3*a*a), q = (2*b*b*b - 9*a*b*c + 27*a*a*d)/(27*a*a*a);
		F64 D = q*q/4 + p*p*p/27;
		//F64 u = cbrt(-q/2 + fsqrt(D)), v = cbrt(-q/2 - fsqrt(D));
		/*
		if we have roots t1, t2, t3, we can do

		mind = infinity;
		for (t in t1, t2, t2, 0, 1) {
			if (t >= 0 && t <= 1) {
				xb, yb = B(t);
				mind = min(d, dist(x, y, xb, yb));
			}
		}
		*/
	}
}

#endif

int main(int, char **argv)
{
	winopen(600, 600, argv[0], 60);
	int tmpcnt = 0, tmp[3][2] = {95, 372, 106, 286, 365, 202};
	Bezier2 *head = bezier2(tmp, 0);
	int sw = 0;
	while (!keyisdown('q')) {
		Image *fb = framebegin();
		int mx = mousex(), my = mousey();
		if (btnwaspressed(3)) {
			tmp[tmpcnt][0] = mx;
			tmp[tmpcnt][1] = my;
			tmpcnt += 1;
			if (tmpcnt == 3) {
				tmpcnt = 0;
				head = bezier2(tmp, head);
			}
		}
		if (btnwaspressed(2)) {
			if (tmpcnt) {
				tmpcnt -= 1;
			} else if (head) {
				Bezier2 *top = head;
				head = top->next;
				memfree(top);
			}
		}
		if (keywaspressed('s')) {
			profreset("bezier");
			sw = !sw;
		}
		drawclear(fb, BLACK);
		profbegin("bezier");
		drawline(fb, mx, my, fb->w-1, my, RGBA(255, 0, 255, 60));
		for (Bezier2 *c = head; c; c = c->next) {
			for (int i = 0; i < 3; i++)
				updatepoint(fb, c->pt[i]);
			for (int i = 0; i < 3; i++)
				drawline(fb, c->pt[i][0], c->pt[i][1], c->pt[1][0], c->pt[1][1], RGBA(40, 40, 40, 255));
			for (int i = 0; i < 3; i++)
				drawsmoothcircle(fb, c->pt[i][0], c->pt[i][1], 6, sw ? BLUE : WHITE);
			drawbezier(fb, c->pt[0][0], c->pt[0][1], c->pt[1][0], c->pt[1][1], c->pt[2][0], c->pt[2][1], WHITE);
			drawbeziertop(fb, c->pt);
			drawbezierisect(fb, c->pt, mx, my);
		}
		profend();
		profdump();
		for (int i = 0; i < tmpcnt; i++)
			drawsmoothcircle(fb, tmp[i][0], tmp[i][1], 6, RED);
		frameend();
	}
	winclose();
	return 0;
}
