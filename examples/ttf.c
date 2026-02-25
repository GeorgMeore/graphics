#include "types.h"
#include "color.h"
#include "image.h"
#include "win.h"
#include "draw.h"
#include "io.h"
#include "alloc.h"
#include "mlib.h"
#include "math.h"
#include "font.h"

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
	I8      ss;
	Image   bmp[CACHESIZE];
	Image   tmp;
	Glyph   *map[CACHESIZE];
	U16     next[CACHESIZE];
	U16     n, last;
} GCache;

#define MAXHTPX 2048
#define MAXSS      6

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
	c->ss = CLAMP((U64)MAXHTPX / h, (U64)1, (U64)MAXSS);
	c->tmp.w = w * c->ss;
	c->tmp.s = w * c->ss;
	c->tmp.h = h * c->ss;
	c->tmp.p = aralloc(&c->mem, (c->ss*c->ss)*w*h * sizeof(Color));
}

static void addglyph(GCache *c, Image *b, Glyph *g)
{
	drawclear(&c->tmp, 0);
	drawbmp(&c->tmp, c->xoff * c->ss, c->yoff * c->ss, *g, c->scale * c->ss);
	for (I y = 0; y < b->h; y++)
	for (I x = 0; x < b->w; x++) {
		I n = 0;
		for (I dy = 0; dy < c->ss; dy++)
		for (I dx = 0; dx < c->ss; dx++)
			n += PIXEL(&c->tmp, x*c->ss + dx, y*c->ss + dy);
		*(F32 *)&PIXEL(b, x, y) = n / (F64)(c->ss*c->ss);
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
		addglyph(c, b, g);
	}
	return b;
}

void drawgbitmap(Image *f, I16 x0, I16 y0, Image *b, Color c)
{
	for (I16 y = CLIPY(f, y0); y < CLIPY(f, y0 + b->h); y++)
	for (I16 x = CLIPX(f, x0); x < CLIPX(f, x0 + b->w); x++) {
		if (PIXEL(b, x - x0, y - y0)) {
			U8 a = *(F32 *)&PIXEL(b, x - x0, y - y0) * 255.0;
			PIXEL(f, x, y) = blend(PIXEL(f, x, y), RGBA(R(c), G(c), B(c), a));
		}
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
	U32 w = 0;
	for (;;) {
		U32 code;
		OK ok = utf8(&t, &code);
		if (!ok || !code)
			return w;
		U16 idx = findglyph(c->fn, code);
		Glyph *g = &c->fn.glyphs[idx];
		w += g->advance * c->scale;
	}
}

void drawtext(Image *f, I16 x, I16 y, GCache *gc, char *t, Color c)
{
	for (;;) {
		U32 code;
		OK ok = utf8(&t, &code);
		if (!ok || !code)
			return;
		U16 idx = findglyph(gc->fn, code);
		Glyph *g = &gc->fn.glyphs[idx];
		Image *b = lookup(gc, g);
		drawgbitmap(f, x - gc->xoff, y - gc->yoff, b, c);
		x += g->advance * gc->scale;
	}
}

#define MAXRUNES 4096

int main(int argc, char **argv)
{
	if (argc != 3) {
		println("usage: ", argv[0], " FONT.ttf STRING");
		return 1;
	}
	Font fn = openttf(argv[1]);
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
	arfree(&fn.mem);
	return 0;
}
