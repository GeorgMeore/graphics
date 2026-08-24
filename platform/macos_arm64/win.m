#import <AppKit/AppKit.h>
#import <QuartzCore/CATransaction.h>
#import <Carbon/Carbon.h>

#include "../../types.h"
#include "../../color.h"
#include "../../image.h"
#include "../../win.h"
#include "../../ntime.h"
#include "../../alloc.h"

#define KEYCOUNT 256
#define BTNCOUNT 8

/* NOTE: this module is compiled without ARC, so no problems
 * with storing object pointers in C structs */
typedef struct {
	Image fb;
	NSApplication *app;
	NSWindow *win;
	U64 targetns;
	U64 startns;
	U64 framens;
	OK mouselocked;
	I  mousex;
	I  mousey;
	U8 flags;
	OK keydown[KEYCOUNT];
	OK prevkeydown[KEYCOUNT];
	OK btndown[BTNCOUNT];
	OK prevbtndown[BTNCOUNT];
	CGFloat scale;
} MacBackend;

/* NOTE: every external function that interacts with AppKit objects
 * should have its body wrapped in an autoreleasepool, because
 * the accessed methods may autorelease objects which would leak
 * without a pool up the stack somewhere */

static MacBackend B;

static void onresize(void)
{
	NSSize s = B.win.contentView.bounds.size;
	B.scale = B.win.backingScaleFactor;
	U16 w = s.width * B.scale;
	U16 h = s.height * B.scale;
	pfree(B.fb.p);
	B.fb.p = palloc(w*h*sizeof(Color));
	B.fb.w = w;
	B.fb.h = h;
	B.fb.s = w;
}

typedef enum {
	WindowResized = 1,
	WindowClosed  = 2,
} WindowFlag;

static void evwatch(NSNotificationName name, WindowFlag flag)
{
	[[NSNotificationCenter defaultCenter] addObserverForName:name
		object:B.win
		queue:nil
		usingBlock:^(NSNotification *){
			B.flags |= flag;
		}];
}

void winopen(U16 w, U16 h, const char *title, U16 fps)
{
	@autoreleasepool {
	if (B.app)
		panic("Connection already established");
	B.app = [NSApplication sharedApplication];
	if (!B.app)
		panic("Failed to start an NS application");
	[B.app setActivationPolicy:NSApplicationActivationPolicyRegular];
	B.win = [[NSWindow alloc]
		initWithContentRect:NSMakeRect(0, 0, w, h)
		styleMask:(NSWindowStyleMaskTitled|NSWindowStyleMaskClosable|NSWindowStyleMaskResizable)
		backing:NSBackingStoreBuffered
		defer:NO];
	if (!B.win)
		panic("Failed to create a window");
	evwatch(NSWindowDidResizeNotification, WindowResized);
	evwatch(NSWindowDidChangeBackingPropertiesNotification, WindowResized);
	evwatch(NSWindowWillCloseNotification, WindowClosed);
	[B.win setTitle:[NSString stringWithUTF8String:title]];
	[B.win center];
	[B.win makeKeyAndOrderFront:nil];
	/* NOTE: Force move the window to the forcus. This method
	 * is "deprecated", but it does exactly the thing
	 * I need when running executables from the terminal */
	[B.app activateIgnoringOtherApps:YES];
	if (fps)
		B.targetns = 1e9 / fps;
	else
		B.targetns = 0;
	onresize();
	B.win.contentView.wantsLayer = YES;
	[B.app finishLaunching];
	}
}

void flush(void)
{
	@autoreleasepool {
	CGColorSpaceRef cs = CGColorSpaceCreateDeviceRGB();
	CGDataProviderRef dp = CGDataProviderCreateWithData(0, B.fb.p, B.fb.s*B.fb.h*4, 0);
	CGImageRef i = CGImageCreate(B.fb.w, B.fb.h, 8, 32, B.fb.s * 4, cs,
		kCGImageAlphaPremultipliedFirst | kCGBitmapByteOrder32Little,
		dp, 0, false, kCGRenderingIntentDefault);
	CGDataProviderRelease(dp);
	CGColorSpaceRelease(cs);
	B.win.contentView.layer.contents = (id)i;
	B.win.contentView.layer.contentsScale = B.scale;
	CGImageRelease(i);
	[CATransaction flush];
	}
}

U64 lastframetime(void)
{
	return B.framens;
}

OK keyisdown(U8 k)
{
	return B.keydown[k];
}

OK keywaspressed(U8 k)
{
	return !B.keydown[k] && B.prevkeydown[k];
}

void mouselock(OK on)
{
	@autoreleasepool {
	B.mouselocked = on;
	if (on) {
		CGAssociateMouseAndMouseCursorPosition(false);
		[NSCursor hide];
	} else {
		CGAssociateMouseAndMouseCursorPosition(true);
		[NSCursor unhide];
	}
	}
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

typedef struct {
	U8     key;
	UInt16 code;
} KeyPair;

/* TODO: support more keys */
static KeyPair kmap[] = {
	{' ', kVK_Space},
	{'0', kVK_ANSI_0}, {'1', kVK_ANSI_1}, {'2', kVK_ANSI_2}, {'3', kVK_ANSI_3},
	{'4', kVK_ANSI_4}, {'5', kVK_ANSI_5}, {'6', kVK_ANSI_6}, {'7', kVK_ANSI_7},
	{'8', kVK_ANSI_8}, {'9', kVK_ANSI_9},
	{'a', kVK_ANSI_A}, {'b', kVK_ANSI_B}, {'c', kVK_ANSI_C}, {'d', kVK_ANSI_D},
	{'e', kVK_ANSI_E}, {'f', kVK_ANSI_F}, {'g', kVK_ANSI_G}, {'h', kVK_ANSI_H},
	{'i', kVK_ANSI_I}, {'j', kVK_ANSI_J}, {'k', kVK_ANSI_K}, {'l', kVK_ANSI_L},
	{'m', kVK_ANSI_M}, {'n', kVK_ANSI_N}, {'o', kVK_ANSI_O}, {'p', kVK_ANSI_P},
	{'q', kVK_ANSI_Q}, {'r', kVK_ANSI_R}, {'s', kVK_ANSI_S}, {'t', kVK_ANSI_T},
	{'u', kVK_ANSI_U}, {'v', kVK_ANSI_V}, {'w', kVK_ANSI_W}, {'x', kVK_ANSI_X},
	{'y', kVK_ANSI_Y}, {'z', kVK_ANSI_Z},
};

#define KMAPCOUNT (sizeof(kmap)/sizeof(kmap[0]))

static void onkey(NSEvent *ev, OK isdown)
{
	if (ev.isARepeat)
		return;
	UInt16 code = ev.keyCode;
	for (I i = 0; i < KMAPCOUNT; i++) {
		KeyPair p = kmap[i];
		if (p.code == code) {
			B.keydown[p.key] = isdown;
			return;
		}
	}
}

static void onmousemove(NSEvent *ev)
{
	if (!B.mouselocked)
		return;
	B.mousex += ev.deltaX * B.scale;
	B.mousey += ev.deltaY * B.scale;
}

static void onmousebtn(NSEvent *ev, OK isdown)
{
	if (ev.buttonNumber >= BTNCOUNT)
		return;
	U8 b = ev.buttonNumber;
	B.prevbtndown[b] = B.btndown[b];
	B.btndown[b] = isdown;
}

/* NOTE: unfortunately this style of api where the main loop
 * is owned by the user makes smooth resize on AppKit impossible,
 * since sendEvent will loop until the drag ends */

static void evpoll(void)
{
	for (I i = 0; i < BTNCOUNT; i++)
		B.prevbtndown[i] = B.btndown[i];
	for (I i = 0; i < KEYCOUNT; i++)
		B.prevkeydown[i] = B.keydown[i];
	B.flags = 0;
	for (;;) {
		NSEvent *ev = [B.app nextEventMatchingMask:NSEventMaskAny
			untilDate:[NSDate distantPast]
			inMode:NSDefaultRunLoopMode
			dequeue:YES];
		if (!ev)
			break;
		switch (ev.type) {
		case NSEventTypeKeyDown:
			onkey(ev, 1);
			continue;
		case NSEventTypeKeyUp:
			onkey(ev, 0);
			continue;
		case NSEventTypeMouseMoved:
		case NSEventTypeLeftMouseDragged:
		case NSEventTypeRightMouseDragged:
			onmousemove(ev);
			continue;
		case NSEventTypeLeftMouseDown:
		case NSEventTypeRightMouseDown:
		case NSEventTypeOtherMouseDown:
			onmousebtn(ev, 1);
			break;
		case NSEventTypeLeftMouseUp:
		case NSEventTypeRightMouseUp:
		case NSEventTypeOtherMouseUp:
			onmousebtn(ev, 0);
			break;
		default:
			break;
		}
		[B.app sendEvent:ev];
	}
	if (B.flags & WindowResized)
		onresize();
}

Image* frame(void)
{
	@autoreleasepool {
	if (!B.win)
		return 0;
	flush();
	B.framens = timens() - B.startns;
	if (B.framens < B.targetns)
		sleepns(B.targetns - B.framens);
	B.startns = timens();
	if (B.mouselocked) {
		B.mousex = 0;
		B.mousey = 0;
	} else {
		NSPoint m = B.win.mouseLocationOutsideOfEventStream;
		B.mousex = m.x*B.scale;
		B.mousey = B.fb.h - m.y*B.scale;
	}
	evpoll();
	if (B.flags & WindowClosed)
		return 0;
	return &B.fb;
	}
}

/* TODO: support event-based mode (fps=0) */
/* FIXME: mouse button numbers is diffrent than in x11 */
/* TODO: maybe try rendering through IOSurfaceRef */

void render1(Image *f, F64 t)
{
	drawclear(f, WHITE);
	drawcircle(f, mousex(), mousey(), 100, RED);
}

void render2(Image *f, F64 t)
{
	drawclear(f, RGBA(18, 18, 18, 255));
	for (int i = 0; i < 200; i++) {
		drawsmoothcircle(f, f->w*i/200, f->h/2 + fsin(t)*fsin(t + 4*PI*i/200)*f->h/2, 5, RGBA(110, 70, 70, 255));
		drawsmoothcircle(f, f->w*i/200, f->h/2 + fcos(t)*fcos(t*.8 + 4*PI*i/200)*f->h/2, 5, RGBA(70, 110, 70, 255));
		drawsmoothcircle(f, f->w*i/200, f->h/2 + fsin(t)*fsin(t*.6 + 4*PI*i/200 + PI)*f->h/2, 5, RGBA(70, 70, 110, 255));
		drawsmoothcircle(f, f->w*i/200, f->h/2 + fcos(t)*fsin(t*.4 + 4*PI*i/200 + 3*PI/2)*f->h/2, 5, RGBA(110, 110, 110, 255));
	}
}

int main()
{
	winopen(1000, 600, "Test", 60);
	F64 t = 0;
	while (!keywaspressed('q')) {
		Image *f = frame();
		if (!f)
			break;
		t += lastframetime()/1e9;
		render1(f, t);
	}
	return 0;
}
