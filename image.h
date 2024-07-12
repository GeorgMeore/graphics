typedef struct {
	u16 w, h;
	Color *p;
} Image;

#define PIXEL(i, x, y) ((i)->p[(y)*(i)->w + (x)])
#define CLIPX(i, x) (CLAMP((x), 0, (i)->w - 1))
#define CLIPY(i, y) (CLAMP((y), 0, (i)->h - 1))
#define CHECKX(i, x) ((x) >= 0 && (x) < (i)->w)
#define CHECKY(i, y) ((y) >= 0 && (y) < (i)->h)
