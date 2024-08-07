#include "types.h"
#include "mlib.h"
#include "color.h"
#include "image.h"

Image subimage(Image i, U16 x, U16 y, U16 w, U16 h)
{
	Image s = {};
	if (x > i.w || y > i.h)
		return s;
	s.p = &PIXEL(&i, x, y);
	s.w = MIN(w, i.w - x);
	s.h = MIN(h, i.h - y);
	s.s = i.s;
	return s;
}
