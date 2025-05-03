typedef struct {
	U16 w, h, s;
	Color *p;
} Image;

#define PIXEL(i, x, y) ((i)->p[(y)*(i)->s + (x)])
#define CLIPX(i, x) (CLAMP((x), 0, (i)->w))
#define CLIPY(i, y) (CLAMP((y), 0, (i)->h))
#define CHECKX(i, x) ((x) >= 0 && (x) < (i)->w)
#define CHECKY(i, y) ((y) >= 0 && (y) < (i)->h)

Image subimage(Image i, U16 x, U16 y, U16 w, U16 h);
Image loadppm(const char *path);
int   savec(Image *i, const char *var, const char *path);
int   saveppm(Image *i, const char *path);
