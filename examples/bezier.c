#include "types.h"
#include "alloc.h"
#include "mlib.h"
#include "color.h"
#include "image.h"
#include "win.h"
#include "draw.h"
#include "math.h"
#include "poly.h"

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
	if (a == 0) {
		if (b == 0)
			return;
		F64 t = -c/b;
		if (t >= 0 && t <= 1) {
			F64 x = (1 - t)*(1 - t)*pt[0][0] + 2*(1 - t)*t*pt[1][0] + t*t*pt[2][0];
			F64 y = (1 - t)*(1 - t)*pt[0][1] + 2*(1 - t)*t*pt[1][1] + t*t*pt[2][1];
			if (x >= xr)
				drawsmoothcircle(i, x, y, 5, BLUE);
		}
	} else {
		F64 d = b*b - 4*a*c;
		if (d < 0)
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

F64 min(F64 x, F64 y)
{
	return x < y ? x : y;
}

F64 clamp(F64 x, F64 l, F64 r)
{
	if (x < l)
		return l;
	if (x > r)
		return r;
	return x;
}

F64 smoothstep(F64 e0, F64 e1, F64 x)
{
	x = clamp((x - e0)/(e1 - e0), 0, 1);
	return x*x*(3 - 2*x);
}

/* FIXME: this is VEEEEEEEEEERY slow, but still interesting...
 *
 * For each point we try to compute the minimum distance to the curve,
 * to do that we check distance's function infliction points and curve's extreme points
 * (t = 0 and t = 1) and choose the minimal value */
void drawbezier3(Image *i, I16 x1, I16 y1, I16 x2, I16 y2, I16 x3, I16 y3, Color c)
{
	/* TODO: precompute the ddy indices, multiplication and differentiation take some time for sure */
	for (I16 x = CLIPX(i, MIN3(x1, x2, x3) - 5); x <= CLIPX(i, MAX3(x1, x2, x3) + 5); x++)
	for (I16 y = CLIPY(i, MIN3(y1, y2, y3) - 5); y <= CLIPY(i, MAX3(y1, y2, y3) + 5); y++) {
		Poly dx = {{(x1 - x), 2*(x2 - x1), x3 + x1 - 2*x2}, 2};
		Poly dy = {{(y1 - y), 2*(y2 - y1), y3 + y1 - 2*y2}, 2};
		Poly d2 = padd(pmul(dx, dx), pmul(dy, dy));
		Poly ddy = ddx(d2);
		Roots r = roots(ddy);
		F64 d = min(eval(d2, 0), eval(d2, 1));
		for (U8 i = 0; i < r.n; i++)
			if (r.v[i] >= 0 && r.v[i] <= 1)
				d = min(d, eval(d2, r.v[i]));
		F64 a = smoothstep(25, 0, d)*A(c);
		PIXEL(i, x, y) = blend(PIXEL(i, x, y), RGBA(R(c), G(c), B(c), a));
	}
}

int main(int, char **argv)
{
	winopen(600, 600, argv[0], 60);
	int tmpcnt = 0, tmp[3][2] = {{95, 100}, {106, 200}, {365, 300}};
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
			if (sw)
				drawbezier3(fb, c->pt[0][0], c->pt[0][1], c->pt[1][0], c->pt[1][1], c->pt[2][0], c->pt[2][1], WHITE);
			else
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
