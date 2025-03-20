#include <fcntl.h>
#include <unistd.h>

#include "types.h"
#include "alloc.h"
#include "color.h"
#include "image.h"
#include "io.h"
#include "ppm.h"

/* TODO: proper parsing (ppm "header" values can be separated by arbitrary amount of spaces) */
Image loadppm(const char *path)
{
	Image i = {};
	int fd = open(path, O_RDONLY);
	if (fd == -1)
		return i;
	IOBuffer b = {.fd = fd};
	U16 w, h, m;
	if (!binputln(&b, "P6", "\n", ID(&w), " ", ID(&h), "\n", ID(&m)))
		goto out;
	if (!m || m >= 256) /* TODO: 2-byte ppms */
		goto out;
	Color *p = memalloc(w*h*sizeof(*p));
	if (!p)
		goto out;
	for (U16 y = 0; y < h; y++)
	for (U16 x = 0; x < w; x++) {
		I rv = bread(&b), gv = bread(&b), bv = bread(&b);
		if (rv == -1 || gv == -1 || bv == -1) {
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
	close(fd);
	return i;
}

int saveppm(Image *i, const char *path)
{
	int fd = open(path, O_WRONLY|O_CREAT|O_TRUNC, 0600);
	if (fd == -1)
		return 0;
	IOBuffer b = {.fd = fd};
	bprintln(&b, "P6\n", OD(i->w), " ", OD(i->h), "\n", OD(255));
	for (U16 y = 0; y < i->h; y++)
	for (U16 x = 0; x < i->w; x++) {
		bwrite(&b, R(PIXEL(i, x, y)));
		bwrite(&b, G(PIXEL(i, x, y)));
		bwrite(&b, B(PIXEL(i, x, y)));
	}
	return bflush(&b) && !close(fd);
}
