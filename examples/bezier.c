#include "types.h"
#include "alloc.h"
#include "mlib.h"
#include "color.h"
#include "image.h"
#include "win.h"
#include "draw.h"

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

int main(void)
{
	winopen(600, 600, "Bezier", 60);
	int tmpcnt = 0, tmp[3][2];
	Bezier2 *head = 0;
	int sw = 0;
	while (!keyisdown('q')) {
		Image *fb = framebegin();
		if (btnwaspressed(3)) {
			tmp[tmpcnt][0] = mousex();
			tmp[tmpcnt][1] = mousey();
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
		//profbegin("bezier");
		for (Bezier2 *c = head; c; c = c->next) {
			for (int i = 0; i < 3; i++)
				updatepoint(fb, c->pt[i]);
			for (int i = 0; i < 3; i++)
				drawline(fb, c->pt[i][0], c->pt[i][1], c->pt[1][0], c->pt[1][1], RGBA(40, 40, 40, 255));
			for (int i = 0; i < 3; i++)
				drawsmoothcircle(fb, c->pt[i][0], c->pt[i][1], 6, sw ? BLUE : WHITE);
			drawbezier(fb, c->pt[0][0], c->pt[0][1], c->pt[1][0], c->pt[1][1], c->pt[2][0], c->pt[2][1], WHITE);
		}
		//profend();
		//profdump();
		for (int i = 0; i < tmpcnt; i++)
			drawsmoothcircle(fb, tmp[i][0], tmp[i][1], 6, RED);
		frameend();
	}
	winclose();
	return 0;
}
