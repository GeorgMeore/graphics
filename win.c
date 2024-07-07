#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xlibint.h>

#include "types.h"
#include "util.h"
#include "color.h"
#include "image.h"
#include "win.h"

static Image fb;
static Display *d;
static Visual *v;
static XImage *i;
static Pixmap bb;
static Window win;
static GC gc;
static int down[CARD(u8)];
static u64 targetns;
static u64 startns;
static u64 framens;

void winclose(void)
{
	if (d)
		XCloseDisplay(d);
	d = NULL;
	if (i)
		XDestroyImage(i);
	i = NULL;
}

static void onresize(u16 w, u16 h)
{
	if (i) {
		XDestroyImage(i);
		XFreePixmap(d, bb);
	}
	fb.p = Xcalloc(w*h, sizeof(fb.p[0]));
	fb.w = w;
	fb.h = h;
	/* NOTE: You might wander: what the fuck is 24? Why the fuck 32?
	 * And the answer, my friend, is X11 is fucking garbage and
	 * it's documentation is written by retarded motherfuckers.
	 * There is no sane way whatsoever to make sure that the server will
	 * understand 32-bit BGRA data that we want to feed it, so we must
	 * use these magic numbers and hope for the best (for us, not for X11 devs, fuck them) */
	i = XCreateImage(d, v, 24, ZPixmap, 0, (char*)fb.p, w, h, 32, 0);
	bb = XCreatePixmap(d, win, w, h, 24);
}

void winopen(u16 w, u16 h, const char *title, u16 fps)
{
	d = XOpenDisplay(NULL);
	if (!d)
		return;
	int s = DefaultScreen(d);
	v = DefaultVisual(d, s);
	gc = DefaultGC(d, s);
	win = XCreateSimpleWindow(d, RootWindow(d, s), 0, 0, w, h, 0, 0, 0);
	XSelectInput(d, win, KeyPressMask|KeyReleaseMask|StructureNotifyMask);
	XStoreName(d, win, title);
	XMapWindow(d, win);
	onresize(w, h);
	targetns = 1000000000 / fps;
}

/* TODO: a more proper input handling */
static void onkey(u8 k, int isdown)
{
	down[k] = isdown;
}

int keyisdown(u8 k)
{
	return down[k];
}

Image *framebegin(void)
{
	startns = timens();
	return &fb;
}

/* TODO: error handling */
void frameend(void)
{
	XPutImage(d, bb, gc, i, 0, 0, 0, 0, fb.w, fb.h);
	XCopyArea(d, bb, win, gc, 0, 0, fb.w, fb.h, 0, 0);
	while (XPending(d)) {
		XEvent e;
		XNextEvent(d, &e);
		if (e.type == KeyPress)
			onkey(XLookupKeysym(&e.xkey, 0), 1);
		else if (e.type == KeyRelease)
			onkey(XLookupKeysym(&e.xkey, 0), 0);
		else if (e.type == ConfigureNotify)
			onresize(e.xconfigure.width, e.xconfigure.height);
	}
	XFlush(d);
	framens = timens() - startns;
	if (framens < targetns)
		sleepns(targetns - framens);
}
