typedef struct {
	U16 w, h, s;
	Color *p;
} Image;

#define PIXEL(i, x, y) ((i)->p[(y)*(i)->s + (x)])
#define CLIPX(i, x) (CLAMP((x), 0, (i)->w - 1))
#define CLIPY(i, y) (CLAMP((y), 0, (i)->h - 1))
#define CHECKX(i, x) ((x) >= 0 && (x) < (i)->w)
#define CHECKY(i, y) ((y) >= 0 && (y) < (i)->h)

Image subimage(Image i, U16 x, U16 y, U16 w, U16 h);
