#include "types.h"
#include "color.h"
#include "image.h"
#include "win.h"
#include "draw.h"
#include "io.h"
#include "math.h"
#include "font.h"
#include "alloc.h"
#include "fontfmt.h"

OK utf8(char **s, U32 *c)
{
	U8 mask = 0b10000000, n = 0;
	while (**s & mask) {
		n += 1;
		mask >>= 1;
	}
	if (n == 1 || n > 4)
		return 0;
	*c = **s & ((mask << 1) - 1);
	for (*s += 1; n > 1; n--) {
		if ((**s & 0b11000000) != 0b10000000)
			return 0;
		*c = (*c << 6) | (**s & 0b00111111);
		*s += 1;
	}
	return 1;
}

#define CACHESIZE 128

/* TODO: maybe the font cache should take a fixed size arena */
/* NOTE: this is a basic LRU cache */
typedef struct {
	Font    fn;
	Arena   mem;
	F64     px, scale;
	I64     xoff, yoff;
	Image   bmp[CACHESIZE];
	Glyph   *map[CACHESIZE];
	U16     next[CACHESIZE];
	U16     n, last;
} GCache;

void setpx(GCache *c, F64 px)
{
	c->n = 0;
	arclear(&c->mem);
	c->px = px;
	c->scale = px / c->fn.upm;
	I64 xmin = ffloor(c->fn.xmin * c->scale), xmax = fceil(c->fn.xmax * c->scale);
	I64 ymin = ffloor(c->fn.ymin * c->scale), ymax = fceil(c->fn.ymax * c->scale);
	c->xoff = -xmin;
	c->yoff = ymax;
	U64 w = xmax - xmin, h = ymax - ymin;
	for (I i = 0; i < CACHESIZE; i++) {
		c->bmp[i].w = w;
		c->bmp[i].s = w;
		c->bmp[i].h = h;
		c->bmp[i].p = aralloc(&c->mem, w*h * sizeof(Color));
	}
}

Image *lookup(GCache *c, Glyph *g)
{
	U16 *pp = &c->last;
	OK found = 0;
	for (U16 i = 0; i < c->n; i++) {
		if (c->map[*pp] == g) {
			found = 1;
			break;
		}
		pp = &c->next[*pp];
	}
	if (!found && c->n < CACHESIZE) {
		c->next[c->n] = c->last;
		c->last = c->n;
		c->n += 1;
	} else if (*pp != c->last) {
		U16 p = *pp;
		*pp = c->next[p];
		c->next[p] = c->last;
		c->last = p;
	}
	Image *b = &c->bmp[c->last];
	if (!found) {
		c->map[c->last] = g;
		drawbmpaa(b, c->xoff, c->yoff, *g, c->scale);
	}
	return b;
}

void drawgbitmap(Image *f, I16 x0, I16 y0, Image *b, Color c)
{
	for (I16 y = CLIPY(f, y0); y < CLIPY(f, y0 + b->h); y++)
	for (I16 x = CLIPX(f, x0); x < CLIPX(f, x0 + b->w); x++) {
		U8 a = *(F32 *)&PIXEL(b, x - x0, y - y0) * 255.0;
		PIXEL(f, x, y) = blend(PIXEL(f, x, y), RGBA(R(c), G(c), B(c), a));
	}
}

I16 drawchar(Image *f, I16 x, I16 y, GCache *gc, U64 code, Color c)
{
	U16 idx = findglyph(gc->fn, code);
	Glyph *g = &gc->fn.glyphs[idx];
	Image *b = lookup(gc, g);
	drawgbitmap(f, x - gc->xoff, y - gc->yoff, b, c);
	return g->advance * gc->scale;
}

U32 textwidth(char *t, GCache *c)
{
	U32 w = 0, wmax = 0;
	for (;;) {
		U32 code;
		OK ok = utf8(&t, &code);
		if (!ok || !code)
			return MAX(w, wmax);
		if (code == '\n') {
			wmax = MAX(w, wmax), w = 0;
		} else if (code == '\t') {
			w += textwidth("    ", c);
		} else {
			U16 idx = findglyph(c->fn, code);
			Glyph *g = &c->fn.glyphs[idx];
			w += g->advance * c->scale;
		}
	}
}

U32 drawtext(Image *f, I16 x, I16 y, GCache *gc, char *t, Color c)
{
	I16 xstart = x;
	I16 ylnstep = (gc->fn.ascend - gc->fn.descend + gc->fn.linegap)*gc->scale;
	for (;;) {
		U32 code;
		OK ok = utf8(&t, &code);
		if (!ok || !code)
			return x - xstart;
		if (code == '\n') {
			y += ylnstep;
			x = xstart;
		} else if (code == '\t') {
			x += drawtext(f, x, y, gc, "    ", c);
		} else {
			x += drawchar(f, x, y, gc, code, c);
		}
	}
}

#define MAXRUNES 4096

int main(int argc, char **argv)
{
	if (argc != 3) {
		println("usage: ", argv[0], " FONT.ttf STRING");
		return 1;
	}
	Arena fntmem = {0};
	Font fn = openttf(argv[1], &fntmem);
	if (!fn.nglyph) {
		eprintln("error: fornt parsing failed");
		return 1;
	}
	GCache c = {.fn = fn};
	setpx(&c, 20);
	winopen(1920, 1080, argv[0], 0);
	while (!keyisdown('q')) {
		Image *f = frame();
		if (btnwaspressed(4))
			setpx(&c, c.px * 1.1);
		if (btnwaspressed(5))
			setpx(&c, c.px / 1.1);
		drawclear(f, RGBA(18, 18, 18, 255));
		U32 l = textwidth(argv[2], &c);
		drawtext(f, mousex() - l/2, mousey(), &c, argv[2], RGBA(255, 255, 255, 200));
	}
	winclose();
	arfree(&c.mem);
	arfree(&fntmem);
	return 0;
}
