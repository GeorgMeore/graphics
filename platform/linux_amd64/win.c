#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xlibint.h>

#include "types.h"
#include "ntime.h"
#include "panic.h"
#include "color.h"
#include "image.h"
#include "win.h"
#include "math.h"

#define RMASK RGBA(0xFF, 0, 0, 0)
#define GMASK RGBA(0, 0xFF, 0, 0)
#define BMASK RGBA(0, 0, 0xFF, 0)

typedef struct {
	Image   fb;
	Display *d;
	Visual  *vis;
	XImage  *i;
	Pixmap  bb;
	Window  win;
	int     depth;
	GC      gc;
	U64     keys, prevkeys;
	U64     btns, prevbtns;
	OK      gotinput;
	int     mousex;
	int     mousey;
	U64     targetns;
	U64     startns;
	U64     framens;
	OK      needswap;
	Cursor  invis;
	OK      mouselocked;
	Atom    delete;
	OK      dead;
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

/* NOTE: When you hold down a keyboard key the X server
 * sends you repeated "Release/Press" pairs, when you
 * scroll with mouse or touchpad you get repeated "Press/Release".
 * That's why we need to handle button and key states a bit differently. */

static void onkey(KeySym sym, OK isdown)
{
	Key k;
	switch (sym) {
	case XK_grave:        k = KeyGrave;     break;
	case XK_0:            k = Key0;         break;
	case XK_1:            k = Key1;         break;
	case XK_2:            k = Key2;         break;
	case XK_3:            k = Key3;         break;
	case XK_4:            k = Key4;         break;
	case XK_5:            k = Key5;         break;
	case XK_6:            k = Key6;         break;
	case XK_7:            k = Key7;         break;
	case XK_8:            k = Key8;         break;
	case XK_9:            k = Key9;         break;
	case XK_minus:        k = KeyMinus;     break;
	case XK_equal:        k = KeyEqual;     break;
	case XK_a:            k = KeyA;         break;
	case XK_b:            k = KeyB;         break;
	case XK_c:            k = KeyC;         break;
	case XK_d:            k = KeyD;         break;
	case XK_e:            k = KeyE;         break;
	case XK_f:            k = KeyF;         break;
	case XK_g:            k = KeyG;         break;
	case XK_h:            k = KeyH;         break;
	case XK_i:            k = KeyI;         break;
	case XK_j:            k = KeyJ;         break;
	case XK_k:            k = KeyK;         break;
	case XK_l:            k = KeyL;         break;
	case XK_m:            k = KeyM;         break;
	case XK_n:            k = KeyN;         break;
	case XK_o:            k = KeyO;         break;
	case XK_p:            k = KeyP;         break;
	case XK_q:            k = KeyQ;         break;
	case XK_r:            k = KeyR;         break;
	case XK_s:            k = KeyS;         break;
	case XK_t:            k = KeyT;         break;
	case XK_u:            k = KeyU;         break;
	case XK_v:            k = KeyV;         break;
	case XK_w:            k = KeyW;         break;
	case XK_x:            k = KeyX;         break;
	case XK_y:            k = KeyY;         break;
	case XK_z:            k = KeyZ;         break;
	case XK_bracketleft:  k = KeyLBracket;  break;
	case XK_bracketright: k = KeyRBracket;  break;
	case XK_backslash:    k = KeyBackslash; break;
	case XK_semicolon:    k = KeySemicolon; break;
	case XK_apostrophe:   k = KeyQuote;     break;
	case XK_comma:        k = KeyComma;     break;
	case XK_period:       k = KeyDot;       break;
	case XK_slash:        k = KeySlash;     break;
	case XK_Tab:          k = KeyTab;       break;
	case XK_Caps_Lock:    k = KeyCaps;      break;
	case XK_Shift_L:      k = KeyLShift;    break;
	case XK_Control_L:    k = KeyLCtrl;     break;
	case XK_Super_L:      k = KeyLWin;      break;
	case XK_Alt_L:        k = KeyLAlt;      break;
	case XK_space:        k = KeySpace;     break;
	case XK_Alt_R:        k = KeyRAlt;      break;
	case XK_Super_R:      k = KeyRWin;      break;
	case XK_Control_R:    k = KeyRCtrl;     break;
	case XK_Shift_R:      k = KeyRShift;    break;
	case XK_Return:       k = KeyEnter;     break;
	case XK_BackSpace:    k = KeyBackspace; break;
	default:
		return;
	}
	if (isdown)
		B.keys |= k;
	else
		B.keys &= ~k;
	B.gotinput = 1;
}

static void onbtn(U button, OK isdown)
{
	Btn b;
	switch (button) {
	case 1: b = BtnLeft;   break;
	case 2: b = BtnMiddle; break;
	case 3: b = BtnRight;  break;
	case 4: b = BtnUp;     break;
	case 5: b = BtnDown;   break;
	default:
		return;
	}
	B.prevbtns = B.btns;
	if (isdown)
		B.btns |= b;
	else
		B.btns &= ~b;
	B.gotinput = 1;
}

OK keyisdown(Key k)
{
	return BOOL(B.keys & k);
}

OK keywaspressed(Key k)
{
	return BOOL(~B.keys & B.prevkeys & k);
}

OK btnisdown(Btn b)
{
	return BOOL(B.btns & b);
}

OK btnwaspressed(Btn b)
{
	return BOOL(~B.btns & B.prevbtns & b);
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
	B.prevbtns = B.btns;
	B.prevkeys = B.keys;
	/* NOTE: in the "event based" mode we want draw the next
	 * frame when a key or a button was pressed */
	if (!B.targetns && !B.gotinput) {
		while (!XPending(B.d))
			sleepns(1.5e6); /* NOTE: often enough, but not too often */
	}
	B.gotinput = 0;
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
}

Image *frame(void)
{
	if (!B.d || B.dead)
		return 0;
	flush();
	B.framens = timens() - B.startns;
	if (B.framens < B.targetns)
		sleepns(B.targetns - B.framens);
	B.startns = timens();
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
	return &B.fb;
}
