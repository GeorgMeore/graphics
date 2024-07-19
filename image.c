#include "types.h"
#include "mlib.h"
#include "color.h"
#include "image.h"

Image subimage(Image i, u16 x, u16 y, u16 w, u16 h)
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
