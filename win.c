#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xlibint.h>

#include "types.h"
#include "ntime.h"
#include "panic.h"
#include "color.h"
#include "image.h"
#include "win.h"

#define KEYCOUNT (MAXVAL(U8) + 1)
#define BTNCOUNT 6

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
	U64 targetns;
	U64 startns;
	U64 framens;
	int needswap;
	Cursor invis;
	int mouselocked;
} X11;

static X11 defxwin;

void winclose(void)
{
	if (defxwin.d)
		XCloseDisplay(defxwin.d);
	defxwin.d = 0;
	if (defxwin.i)
		XDestroyImage(defxwin.i);
	defxwin.i = 0;
}

static void onresize(U16 w, U16 h)
{
	if (!w || !h)
		return;
	if (defxwin.i) {
		XDestroyImage(defxwin.i);
		XFreePixmap(defxwin.d, defxwin.bb);
	}
	defxwin.fb.p = Xmalloc(w*h*sizeof(defxwin.fb.p[0]));
	defxwin.fb.w = w;
	defxwin.fb.h = h;
	defxwin.fb.s = w;
	defxwin.i = XCreateImage(defxwin.d, defxwin.vis, defxwin.depth, ZPixmap, 0, (char*)defxwin.fb.p, w, h, 32, 0);
	defxwin.bb = XCreatePixmap(defxwin.d, defxwin.win, w, h, defxwin.depth);
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
void winopen(U16 w, U16 h, const char *title, U16 fps)
{
	if (defxwin.d)
		return;
	defxwin.d = XOpenDisplay(0);
	if (!defxwin.d)
		panic("Failed to connect to the X server");
	int s = DefaultScreen(defxwin.d);
	defxwin.depth = DefaultDepth(defxwin.d, s);
	defxwin.vis = DefaultVisual(defxwin.d, s);
	/* NOTE: Check if the screen supports 32-bit RGB, we could also
	 * try to search for an appropriate visual, but I don't think it matters */
	if (!isrgb32(defxwin.d, defxwin.vis, defxwin.depth))
		panic("The default display visual doesn't support 32-bit RGB");
	defxwin.win = XCreateSimpleWindow(defxwin.d, RootWindow(defxwin.d, s), 0, 0, w, h, 0, 0, 0);
	defxwin.gc = DefaultGC(defxwin.d, s);
	XSelectInput(defxwin.d, defxwin.win, KeyPressMask|ButtonPressMask|ButtonReleaseMask|KeyReleaseMask|StructureNotifyMask);
	XStoreName(defxwin.d, defxwin.win, title);
	XMapWindow(defxwin.d, defxwin.win);
	defxwin.needswap = byteorder() != defxwin.d->byte_order;
	defxwin.targetns = 1000000000 / fps;
	/* NOTE: hacky hacks to get an invisible cursor */
	XColor c = {};
	Pixmap p = XCreatePixmap(defxwin.d, defxwin.win, 1, 1, 1);
	defxwin.invis = XCreatePixmapCursor(defxwin.d, p, p, &c, &c, 0, 0);
	/* NOTE: hacky hack to avoid having a 0x0 window on the first frame */
	onresize(w, h);
}

void mouselock(int on)
{
	defxwin.mouselocked = on;
	if (on)
		XDefineCursor(defxwin.d, defxwin.win, defxwin.invis);
	else
		XUndefineCursor(defxwin.d, defxwin.win);
}

/* TODO: a more proper input handling */
static void onkey(U8 k, int isdown)
{
	defxwin.keydown[k] = isdown;
}

int keyisdown(U8 k)
{
	return defxwin.keydown[k];
}

int keywaspressed(U8 k)
{
	return !defxwin.keydown[k] && defxwin.prevkeydown[k];
}

static void onbtn(U8 b, int isdown)
{
	defxwin.btndown[b] = isdown;
}

int btnisdown(U8 b)
{
	return defxwin.btndown[b];
}

int btnwaspressed(U8 b)
{
	return !defxwin.btndown[b] && defxwin.prevbtndown[b];
}

Image *framebegin(void)
{
	if (!defxwin.d)
		return 0;
	Window r, c;
	int rx, ry;
	unsigned int mask;
	XQueryPointer(defxwin.d, defxwin.win, &r, &c, &rx, &ry, &defxwin.mousex, &defxwin.mousey, &mask);
	if (defxwin.mouselocked) {
		defxwin.mousex -= defxwin.fb.w/2;
		defxwin.mousey -= defxwin.fb.h/2;
		XWarpPointer(defxwin.d, None, defxwin.win, 0, 0, 0, 0, defxwin.fb.w/2, defxwin.fb.h/2);
		/* NOTE: XSync must me used here to make sure that the cursor is actually warped before
		 * the user moves the mouse in during a new frame, or the movent can be lost. */
		XSync(defxwin.d, 0);
	}
	defxwin.startns = timens();
	return &defxwin.fb;
}

int mousex(void)
{
	return defxwin.mousex;
}

int mousey(void)
{
	return defxwin.mousey;
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

U64 lastframetime(void)
{
	return defxwin.framens;
}

void flush(void)
{
	if (!defxwin.i)
		return;
	if (defxwin.needswap)
		/* NOTE: Xlib can actually do the swapping for us, if the image's
		 * byte_order field doesn't match server's, but... */
		swaprgb32(&defxwin.fb);
	XPutImage(defxwin.d, defxwin.bb, defxwin.gc, defxwin.i, 0, 0, 0, 0, defxwin.fb.w, defxwin.fb.h);
	XCopyArea(defxwin.d, defxwin.bb, defxwin.win, defxwin.gc, 0, 0, defxwin.fb.w, defxwin.fb.h, 0, 0);
	XSync(defxwin.d, 0);
}

void frameend(void)
{
	if (!defxwin.d)
		return;
	flush();
	for (int i = 0; i < BTNCOUNT; i++)
		defxwin.prevbtndown[i] = defxwin.btndown[i];
	for (int i = 0; i < KEYCOUNT; i++)
		defxwin.prevkeydown[i] = defxwin.keydown[i];
	while (XPending(defxwin.d)) {
		XEvent e;
		XNextEvent(defxwin.d, &e);
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
	defxwin.framens = timens() - defxwin.startns;
	if (defxwin.framens < defxwin.targetns)
		sleepns(defxwin.targetns - defxwin.framens);
	defxwin.framens = timens() - defxwin.startns;
}
