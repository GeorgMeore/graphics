#include "types.h"
#include "color.h"
#include "image.h"
#include "draw.h"
#include "win.h"
#include "poly.h"

#define MAXDEG 128

I16 tickstep(F64 scale)
{
	F64 ts = 1;
	if (scale < 1) {
		while (ts > scale)
			ts /= 10;
	} else {
		while (ts*10 <= scale)
			ts *= 10;
	}
	return ts*500/scale;
}

#define BGCOLOR RGBA(18, 18, 18, 255)
#define LINECOLOR RGBA(120, 120, 120, 255)

int main(int, char **argv)
{
	Poly p = {{-0.2, 5, -10, -3, 2}, 4};
	Roots r = roots(p);
	F64 xscale = .0125, yscale = 20;
	I16 xstep = tickstep(xscale), ystep = tickstep(yscale);
	winopen(1920, 1080, argv[0], 60);
	while (!keyisdown('q')) {
		Image *f = framebegin();
		if (keyisdown('x') && keyisdown('o'))
			xscale *= 1.025, xstep = tickstep(xscale);
		if (keyisdown('x') && keyisdown('i'))
			xscale /= 1.025, xstep = tickstep(xscale);
		if (keyisdown('y') && keyisdown('i'))
			yscale *= 1.025, ystep = tickstep(yscale);
		if (keyisdown('y') && keyisdown('o'))
			yscale /= 1.025, ystep = tickstep(yscale);
		drawclear(f, BGCOLOR);
		drawline(f, 0, f->h/2, f->w, f->h/2, LINECOLOR);
		drawsmoothtriangle(f, f->w, f->h/2, f->w-10, f->h/2-5, f->w-10, f->h/2+5, LINECOLOR);
		for (I16 i = f->w/2 + xstep; i < f->w; i += xstep) {
			drawline(f, i, f->h/2-5, i, f->h/2 + 5, LINECOLOR);
			drawline(f, f->w-i, f->h/2-5, f->w-i, f->h/2 + 5, LINECOLOR);
		}
		drawline(f, f->w/2, 0, f->w/2, f->h, LINECOLOR);
		drawsmoothtriangle(f, f->w/2, 0, f->w/2-5, 10, f->w/2+5, 10, LINECOLOR);
		for (I16 i = f->h/2 + ystep; i < f->h; i += ystep) {
			drawline(f, f->w/2-5, i, f->w/2 + 5, i, LINECOLOR);
			drawline(f, f->w/2-5, f->h-i, f->w/2 + 5, f->h-i, LINECOLOR);
		}
		drawsmoothcircle(f, f->w/2, f->h/2, 5, LINECOLOR);
		for (U16 i = 0; i < f->w; i += 5) {
			F64 x = (i - f->w/2)*xscale;
			F64 y = eval(p, x);
			F64 j = f->h/2 - y*yscale;
			if (j >= 0 && j <= f->h)
				drawsmoothcircle(f, i, j, 5, RGBA(100, 100, 150, 100));
		}
		for (U8 i = 0; i < r.n; i++)
			drawsmoothcircle(f, r.v[i]/xscale + f->w/2, f->h/2, 5, RED);
		frameend();
	}
}
