#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xlibint.h>

#include "types.h"
#include "util.h"
#include "color.h"
#include "image.h"
#include "win.h"

#define KEYCOUNT CARD(u8)
#define BTNCOUNT 5

#define RMASK RGBA(0xFF, 0, 0, 0)
#define GMASK RGBA(0, 0xFF, 0, 0)
#define BMASK RGBA(0, 0, 0xFF, 0)

typedef struct {
	Image fb;
	Display *d;
	Visual *vis;
	XImage *i;
	Pixmap bb;
	Window win;
	int depth;
	GC gc;
	int keydown[KEYCOUNT];
	int prevkeydown[KEYCOUNT];
	int btndown[BTNCOUNT];
	int prevbtndown[BTNCOUNT];
	int mousex;
	int mousey;
	u64 targetns;
	u64 startns;
	u64 framens;
	int needswap;
} X11;

static X11 x;

void winclose(void)
{
	if (x.d)
		XCloseDisplay(x.d);
	x.d = 0;
	if (x.i)
		XDestroyImage(x.i);
	x.i = 0;
}

static void onresize(u16 w, u16 h)
{
	if (!w || !h)
		return;
	if (x.i) {
		XDestroyImage(x.i);
		XFreePixmap(x.d, x.bb);
	}
	x.fb.p = Xmalloc(w*h*sizeof(x.fb.p[0]));
	x.fb.w = w;
	x.fb.h = h;
	x.fb.s = w;
	x.i = XCreateImage(x.d, x.vis, x.depth, ZPixmap, 0, (char*)x.fb.p, w, h, 32, 0);
	x.bb = XCreatePixmap(x.d, x.win, w, h, x.depth);
}

static int isrgb32(Display *d, Visual *v, int depth)
{
	if (v->class != TrueColor)
		return 0;
	if (v->red_mask != RMASK || v->green_mask != GMASK || v->blue_mask != BMASK)
		return 0;
	for (int i = 0; i < d->nformats; i++) {
		ScreenFormat *f = &d->pixmap_format[i];
		if (f->depth == depth)
			return f->bits_per_pixel == 32;
	}
	return 0;
}

static int byteorder(void)
{
	int x = 1;
	return *(char *)&x == 1 ? LSBFirst : MSBFirst;
}

/* TODO: more proper error handling */
/* TODO: look into the shared memory extension */
void winopen(u16 w, u16 h, const char *title, u16 fps)
{
	if (x.d)
		return;
	x.d = XOpenDisplay(0);
	if (!x.d)
		return;
	int s = DefaultScreen(x.d);
	x.depth = DefaultDepth(x.d, s);
	x.vis = DefaultVisual(x.d, s);
	/* NOTE: Check if the screen supports 32-bit RGB, we could also
	 * try to search for an appropriate visual, but I don't think it matters */
	if (!isrgb32(x.d, x.vis, x.depth)) {
		winclose();
		return;
	}
	x.win = XCreateSimpleWindow(x.d, RootWindow(x.d, s), 0, 0, w, h, 0, 0, 0);
	x.gc = DefaultGC(x.d, s);
	XSelectInput(x.d, x.win, KeyPressMask|ButtonPressMask|ButtonReleaseMask|KeyReleaseMask|StructureNotifyMask);
	XStoreName(x.d, x.win, title);
	XMapWindow(x.d, x.win);
	x.needswap = byteorder() != x.d->byte_order;
	x.targetns = 1000000000 / fps;
}

/* TODO: a more proper input handling */
static void onkey(u8 k, int isdown)
{
	x.keydown[k] = isdown;
}

int keyisdown(u8 k)
{
	return x.keydown[k];
}

int keywaspressed(u8 k)
{
	return !x.keydown[k] && x.prevkeydown[k];
}

static void onbtn(u8 b, int isdown)
{
	x.btndown[b] = isdown;
}

int btnisdown(u8 b)
{
	return x.btndown[b];
}

int btnwaspressed(u8 b)
{
	return !x.btndown[b] && x.prevbtndown[b];
}

Image *framebegin(void)
{
	if (!x.d)
		return 0;
	Window r, c;
	int rx, ry;
	unsigned int mask;
	XQueryPointer(x.d, x.win, &r, &c, &rx, &ry, &x.mousex, &x.mousey, &mask);
	x.startns = timens();
	return &x.fb;
}

int mousex(void)
{
	return x.mousex;
}

int mousey(void)
{
	return x.mousey;
}

#define REVERSE4(x) (\
	((x)&(0xFF<<(8*0)))<<(8*3)|\
	((x)&(0xFF<<(8*1)))<<(8*1)|\
	((x)&(0xFF<<(8*2)))>>(8*1)|\
	((x)&(0xFF<<(8*3)))>>(8*3))

static void swaprgb32(Image *i)
{
	for (int x = 0; x < i->w; x++)
	for (int y = 0; y < i->h; y++)
		PIXEL(i, x, y) = REVERSE4(PIXEL(i, x, y));
}

void frameend(void)
{
	if (!x.d)
		return;
	if (x.i) {
		if (x.needswap)
			/* NOTE: Xlib can actually do the swapping for us, if the image's
			 * byte_order field doesn't match server's, but... */
			swaprgb32(&x.fb);
		XPutImage(x.d, x.bb, x.gc, x.i, 0, 0, 0, 0, x.fb.w, x.fb.h);
		XCopyArea(x.d, x.bb, x.win, x.gc, 0, 0, x.fb.w, x.fb.h, 0, 0);
	}
	XSync(x.d, 0);
	for (int i = 0; i < BTNCOUNT; i++)
		x.prevbtndown[i] = x.btndown[i];
	for (int i = 0; i < KEYCOUNT; i++)
		x.prevkeydown[i] = x.keydown[i];
	while (XPending(x.d)) {
		XEvent e;
		XNextEvent(x.d, &e);
		if (e.type == KeyPress)
			onkey(XLookupKeysym(&e.xkey, 0), 1);
		else if (e.type == KeyRelease)
			onkey(XLookupKeysym(&e.xkey, 0), 0);
		else if (e.type == ConfigureNotify)
			onresize(e.xconfigure.width, e.xconfigure.height);
		else if (e.type == ButtonPress)
			onbtn(e.xbutton.button, 1);
		else if (e.type == ButtonRelease)
			onbtn(e.xbutton.button, 0);
	}
	x.framens = timens() - x.startns;
	if (x.framens < x.targetns)
		sleepns(x.targetns - x.framens);
}
