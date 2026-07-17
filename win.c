#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xlibint.h>

#include "types.h"
#include "ntime.h"
#include "panic.h"
#include "color.h"
#include "image.h"
#include "win.h"

#define RMASK RGBA(0xFF, 0, 0, 0)
#define GMASK RGBA(0, 0xFF, 0, 0)
#define BMASK RGBA(0, 0, 0xFF, 0)

#define COUNT 256 /* keys/buttons */

typedef struct {
	Image fb;
	Display *d;
	Visual *vis;
	XImage *i;
	Pixmap bb;
	Window win;
	int depth;
	GC gc;
	OK keydown[COUNT];
	OK prevkeydown[COUNT];
	OK btndown[COUNT];
	OK prevbtndown[COUNT];
	OK gotpress;
	int mousex;
	int mousey;
	U64 targetns;
	U64 startns;
	U64 framens;
	OK needswap;
	Cursor invis;
	OK mouselocked;
	Atom delete;
	OK dead;
} X11Backend;

static X11Backend B;

static void onresize(U16 w, U16 h)
{
	if (!w || !h)
		return;
	if (B.i) {
		XDestroyImage(B.i);
		XFreePixmap(B.d, B.bb);
	}
	B.fb.p = Xmalloc(w*h*sizeof(B.fb.p[0]));
	B.fb.w = w;
	B.fb.h = h;
	B.fb.s = w;
	B.i = XCreateImage(B.d, B.vis, B.depth, ZPixmap, 0, (char*)B.fb.p, w, h, 32, 0);
	B.bb = XCreatePixmap(B.d, B.win, w, h, B.depth);
}

static OK isrgb32(Display *d, Visual *v, int depth)
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
	U32 x = 1;
	return *(U8 *)&x == 1 ? LSBFirst : MSBFirst;
}

/* TODO: more proper error handling */

/* TODO: look into the shared memory extension */
void winopen(U16 w, U16 h, const char *title, U16 fps)
{
	if (B.d)
		panic("Connection was already established");
	B.d = XOpenDisplay(0);
	if (!B.d)
		panic("Failed to connect to the X server");
	int s = DefaultScreen(B.d);
	B.depth = DefaultDepth(B.d, s);
	B.vis = DefaultVisual(B.d, s);
	/* NOTE: Check if the screen supports 32-bit RGB, we could also
	 * try to search for an appropriate visual, but I don't think it matters */
	if (!isrgb32(B.d, B.vis, B.depth))
		panic("The default display visual doesn't support 32-bit RGB");
	B.win = XCreateSimpleWindow(B.d, RootWindow(B.d, s), 0, 0, w, h, 0, 0, 0);
	B.gc = DefaultGC(B.d, s);
	XSelectInput(B.d, B.win,
		KeyPressMask|ButtonPressMask|ButtonReleaseMask|KeyReleaseMask|
		StructureNotifyMask|PointerMotionMask|ExposureMask);
	B.delete = XInternAtom(B.d, "WM_DELETE_WINDOW", False);
	XSetWMProtocols(B.d, B.win, &B.delete, 1);
	XSetGraphicsExposures(B.d, B.gc, False); /* X11 is very stupid */
	XStoreName(B.d, B.win, title);
	XMapWindow(B.d, B.win);
	B.needswap = byteorder() != B.d->byte_order;
	if (fps)
		B.targetns = 1e9 / fps;
	else
		B.targetns = 0;
	/* NOTE: hacky hacks to get an invisible cursor */
	XColor c = {0};
	Pixmap p = XCreatePixmap(B.d, B.win, 1, 1, 1);
	B.invis = XCreatePixmapCursor(B.d, p, p, &c, &c, 0, 0);
	/* NOTE: hacky hack to avoid having a 0x0 window on the first frame */
	onresize(w, h);
}

void mouselock(OK on)
{
	if (B.dead)
		return;
	B.mouselocked = on;
	if (on)
		XDefineCursor(B.d, B.win, B.invis);
	else
		XUndefineCursor(B.d, B.win);
}

/* TODO: Support text input via Xutf8LookupString. Probably a small
 * per-frame accumulative buffer will do, but I'll need to figure out
 * how to handle deletes/modifiers/other special stuff. */

/* NOTE: all currently supported keys are listed here explicitly,
 * that's probably not optimal, but it's predictable and simple */
static KeySym keymap[COUNT] = {
	[' '] = XK_space,
	['0'] = XK_0, XK_1, XK_2, XK_3, XK_4, XK_5, XK_6, XK_7, XK_8, XK_9,
	['a'] = XK_a, XK_b, XK_c, XK_d, XK_e, XK_f, XK_g, XK_h, XK_i, XK_j,
		XK_k, XK_l, XK_m, XK_n, XK_o, XK_p, XK_q, XK_r, XK_s, XK_t, XK_u,
		XK_v, XK_w, XK_x, XK_y, XK_z,
};

/* NOTE: When you hold down a keyboard key the X server
 * sends you repeated "Release/Press" pairs, when you
 * scroll with mouse or touchpad you get repeated "Press/Release".
 * That's why we need to handle button and key states a bit differently. */

static void onkey(KeySym k, OK isdown)
{
	for (I i = 0; i < COUNT; i++) {
		if (keymap[i] == k) {
			B.gotpress |= !isdown;
			B.keydown[i] = isdown;
			return;
		}
	}
}

static void onbtn(U8 b, OK isdown)
{
	B.gotpress |= !isdown;
	B.prevbtndown[b] = B.btndown[b];
	B.btndown[b] = isdown;
}

OK keyisdown(U8 k)
{
	return B.keydown[k];
}

OK keywaspressed(U8 k)
{
	return !B.keydown[k] && B.prevkeydown[k];
}

OK btnisdown(U8 b)
{
	return B.btndown[b];
}

OK btnwaspressed(U8 b)
{
	return !B.btndown[b] && B.prevbtndown[b];
}

I mousex(void)
{
	return B.mousex;
}

I mousey(void)
{
	return B.mousey;
}

#define REVERSE4(x) (\
	((x)&(0xFF<<(8*0)))<<(8*3)|\
	((x)&(0xFF<<(8*1)))<<(8*1)|\
	((x)&(0xFF<<(8*2)))>>(8*1)|\
	((x)&(0xFF<<(8*3)))>>(8*3))

static void swaprgb32(Image *i)
{
	for (I x = 0; x < i->w; x++)
	for (I y = 0; y < i->h; y++)
		PIXEL(i, x, y) = REVERSE4(PIXEL(i, x, y));
}

U64 lastframetime(void)
{
	return B.framens;
}

void flush(void)
{
	if (!B.i || B.dead)
		return;
	if (B.needswap)
		/* NOTE: Xlib can actually do the swapping for us, if the image's
		 * byte_order field doesn't match server's, but... */
		swaprgb32(&B.fb);
	XPutImage(B.d, B.bb, B.gc, B.i, 0, 0, 0, 0, B.fb.w, B.fb.h);
	XCopyArea(B.d, B.bb, B.win, B.gc, 0, 0, B.fb.w, B.fb.h, 0, 0);
	XSync(B.d, 0);
}

static void evpoll(void)
{
	for (I i = 0; i < COUNT; i++) {
		B.prevbtndown[i] = B.btndown[i];
		B.prevkeydown[i] = B.keydown[i];
	}
	/* NOTE: in the "event based" mode we want draw the next
	 * frame when a key or a button was pressed */
	if (!B.targetns && !B.gotpress) {
		while (!XPending(B.d))
			sleepns(1.5e6); /* NOTE: often enough, but not too often */
	}
	B.gotpress = 0;
	while (XPending(B.d)) {
		XEvent e;
		XNextEvent(B.d, &e);
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
		else if (e.type == ClientMessage && (Atom)e.xclient.data.l[0] == B.delete)
			B.dead = 1;
	}
	B.framens = timens() - B.startns;
	if (B.framens < B.targetns)
		sleepns(B.targetns - B.framens);
	B.framens = timens() - B.startns;
}

Image *frame(void)
{
	if (!B.d || B.dead)
		return 0;
	flush();
	evpoll();
	if (B.dead)
		return 0;
	Window r, c;
	int rx, ry;
	unsigned int mask;
	XQueryPointer(B.d, B.win, &r, &c, &rx, &ry, &B.mousex, &B.mousey, &mask);
	if (B.mouselocked) {
		B.mousex -= B.fb.w/2;
		B.mousey -= B.fb.h/2;
		XWarpPointer(B.d, None, B.win, 0, 0, 0, 0, B.fb.w/2, B.fb.h/2);
		/* NOTE: XSync must me used here to make sure that the cursor is actually warped before
		 * the user moves the mouse in during a new frame, or the movent can be lost. */
		XSync(B.d, 0);
	}
	B.startns = timens();
	return &B.fb;
}
