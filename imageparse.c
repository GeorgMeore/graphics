#include "types.h"
#include "io.h"
#include "color.h"
#include "image.h"
#include "alloc.h"
#include "imageparse.h"

/* TODO: a custom compressed image format based on k-means clustering
 * with 256-color palette (1 byte per pixel) */

Image loadppm(const char *path, Arena *a)
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
	Color *p = aralloc(a, w*h*sizeof(*p));
	for (U16 y = 0; y < h; y++)
	for (U16 x = 0; x < w; x++) {
		U8 rv, gv, bv;
		if (!binput(&b, ID(&rv), ID(&gv), ID(&bv)))
			goto out;
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
