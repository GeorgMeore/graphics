#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xlibint.h>

#include "types.h"
#include "color.h"
#include "image.h"
#include "win.h"

Image fb;

static Display *d;
static Visual *v;
static XImage *i;
static Window win;
static GC gc;
static int down[2<<sizeof(u8)];

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
	if (i)
		XDestroyImage(i);
	fb.p = Xcalloc(w*h, sizeof(fb.p[0]));
	fb.w = w;
	fb.h = h;
	i = XCreateImage(d, v, 24, ZPixmap, 0,
			(char*)fb.p, w, h, sizeof(fb.p[0])*8, 0);
}

/* TODO: double buffering */
void winopen(u16 w, u16 h, const char *title)
{
	d = XOpenDisplay(NULL);
	if (!d)
		return;
	int s = DefaultScreen(d);
	v = DefaultVisual(d, s);
	gc = DefaultGC(d, s);
	/* we try to be careful and check that the display
	 * will understand our RGBA data (TODO: check byte order) */
	if (DefaultDepth(d, s) != 24 ||
			v->red_mask != RGBA(0xFF, 0, 0, 0) ||
			v->green_mask != RGBA(0, 0xFF, 0, 0) ||
			v->blue_mask != RGBA(0, 0, 0xFF, 0)) {
		winclose();
		return;
	}
	win = XCreateSimpleWindow(d, RootWindow(d, s),
			0, 0, w, h, 0, BlackPixel(d, s), BlackPixel(d, s));
	XSelectInput(d, win, KeyPressMask|KeyReleaseMask|StructureNotifyMask);
	XStoreName(d, win, title);
	XMapWindow(d, win);
	onresize(w, h);
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

/* TODO: error handling */
void winpoll(void)
{
	XPutImage(d, win, gc, i, 0, 0, 0, 0, fb.w, fb.h);
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
}
