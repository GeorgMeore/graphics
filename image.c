#include "types.h"
#include "alloc.h"
#include "color.h"
#include "mlib.h"
#include "io.h"
#include "image.h"

Image subimage(Image i, U16 x, U16 y, U16 w, U16 h)
{
	Image s = {0};
	if (x > i.w || y > i.h)
		return s;
	s.p = &PIXEL(&i, x, y);
	s.w = MIN(w, i.w - x);
	s.h = MIN(h, i.h - y);
	s.s = i.s;
	return s;
}

Image loadppm(const char *path)
{
	Image i = {0};
	IOBuffer b = {0};
	if (!bopen(&b, path, 'r'))
		return i;
	U16 w, h, m;
	if (!binput(&b, "P6", IWS, ID(&w), IWS, ID(&h), IWS, ID(&m), IWS1))
		goto out;
	if (!m || m >= 256) /* TODO: 2-byte ppms */
		goto out;
	Color *p = memalloc(w*h*sizeof(*p));
	if (!p)
		goto out;
	for (U16 y = 0; y < h; y++)
	for (U16 x = 0; x < w; x++) {
		U8 rv, gv, bv;
		if (!binput(&b, ID(&rv), ID(&gv), ID(&bv))) {
			memfree(p);
			goto out;
		}
		p[y*w + x] = RGBA(rv*(U64)255/m, gv*(U64)255/m, bv*(U64)255/m, 255);
	}
	i.w = w;
	i.h = h;
	i.s = w;
	i.p = p;
out:
	bclose(&b);
	return i;
}

int savec(Image *i, const char *var, const char *path)
{
	IOBuffer b = {0};
	if (!bopen(&b, path, 'w'))
		return 0;
	bprintln(&b, "Color ", var, "[", OD(i->h), "][", OD(i->w), "] = {");
	for (U16 y = 0; y < i->h; y++) {
		bprint(&b, "\t{");
		for (U16 x = 0; x < i->w; x++)
			bprint(&b, "0x", OH(PIXEL(i, x, y)), ",");
		bprint(&b, "},\n");
	}
	bprintln(&b, "};");
	return bclose(&b);
}

int saveppm(Image *i, const char *path)
{
	IOBuffer b = {0};
	if (!bopen(&b, path, 'w'))
		return 0;
	bprintln(&b, "P6\n", OD(i->w), " ", OD(i->h), "\n", OD(255));
	for (U16 y = 0; y < i->h; y++)
	for (U16 x = 0; x < i->w; x++) {
		bwrite(&b, R(PIXEL(i, x, y)));
		bwrite(&b, G(PIXEL(i, x, y)));
		bwrite(&b, B(PIXEL(i, x, y)));
	}
	return bclose(&b);
}
