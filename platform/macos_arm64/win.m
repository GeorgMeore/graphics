#import <AppKit/AppKit.h>
#import <QuartzCore/CATransaction.h>
#import <Carbon/Carbon.h>

#include "../../types.h"
#include "../../color.h"
#include "../../image.h"
#include "../../win.h"
#include "../../ntime.h"
#include "../../alloc.h"
#include "../../draw.h"
#include "../../math.h"

#define COUNT 256

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
	OK keydown[COUNT];
	OK prevkeydown[COUNT];
} MacBackend;

/* NOTE: every external function that interacts with AppKit objects
 * should have its body wrapped in an autoreleasepool, because
 * the accessed methods may autorelease objects which would leak
 * without a pool up the stack somewhere */

static MacBackend B;

static void onresize(void)
{
	NSSize s = B.win.contentView.bounds.size;
	CGFloat f = B.win.backingScaleFactor;
	U16 w = s.width;
	U16 h = s.height;
	//B.win.contentView.layer.contentsScale = f;
	//U16 w = s.width*f;
	//U16 h = s.height*f;
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
	CGImageRelease(i);
	[CATransaction flush];
	}
}

/* TODO: support more keys */
static U8 keycode[COUNT] = {
	[' '] = kVK_Space,
	['0'] = kVK_ANSI_0, kVK_ANSI_1, kVK_ANSI_2, kVK_ANSI_3, kVK_ANSI_4,
		kVK_ANSI_5, kVK_ANSI_6, kVK_ANSI_7, kVK_ANSI_8, kVK_ANSI_9,
	['a'] = kVK_ANSI_A, kVK_ANSI_B, kVK_ANSI_C, kVK_ANSI_D, kVK_ANSI_E,
		kVK_ANSI_F, kVK_ANSI_G, kVK_ANSI_H, kVK_ANSI_I, kVK_ANSI_J,
		kVK_ANSI_K, kVK_ANSI_L, kVK_ANSI_M, kVK_ANSI_N, kVK_ANSI_O,
		kVK_ANSI_P, kVK_ANSI_Q, kVK_ANSI_R, kVK_ANSI_S, kVK_ANSI_T,
		kVK_ANSI_U, kVK_ANSI_V, kVK_ANSI_W, kVK_ANSI_X, kVK_ANSI_Y,
		kVK_ANSI_Z,
};

U64 lastframetime(void)
{
	return B.framens;
}

OK keyisdown(U8 k)
{
	return B.keydown[keycode[k]];
}

OK keywaspressed(U8 k)
{
	U8 c = keycode[k];
	return !B.keydown[c] && B.prevkeydown[c];
}

void mouselock(OK on)
{
	@autoreleasepool {
	B.mouselocked = on;
	if (on)
		[NSCursor hide];
	else
		[NSCursor unhide];
	}
}

I mousex(void)
{
	return B.mousex;
}

I mousey(void)
{
	return B.mousey;
}

/* NOTE: unfortunately this style of api where the main loop
 * is owned by the user makes smooth resize on AppKit impossible,
 * since sendEvent will loop until the drag ends */

static void evpoll(void)
{
	for (I i = 0; i < COUNT; i++)
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
			if (!ev.isARepeat && ev.keyCode < COUNT)
				B.keydown[ev.keyCode] = 1;
			continue;
		case NSEventTypeKeyUp:
			if (ev.keyCode < COUNT)
				B.keydown[ev.keyCode] = 0;
			continue;
		/* TODO: mouse buttons */
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
	evpoll();
	if (B.flags & WindowClosed)
		return 0;
	NSPoint m = B.win.mouseLocationOutsideOfEventStream;
	B.mousex = m.x;
	B.mousey = B.fb.h - m.y;
	if (B.mouselocked) {
		B.mousex -= B.fb.w/2;
		B.mousey -= B.fb.h/2;
		NSPoint p = [B.win convertRectToScreen:NSMakeRect(B.fb.w/2, B.fb.h/2, 0, 0)].origin;
		CGFloat h = NSMaxY([[NSScreen screens] firstObject].frame);
		CGWarpMouseCursorPosition(CGPointMake(p.x, h - p.y));
	}
	B.startns = timens();
	return &B.fb;
	}
}

/* TODO: account for backingScaleFactor to support retina */
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
	winopen(1000, 600, "Test", 30);
	F64 t = 0;
	while (!keywaspressed('q')) {
		Image *f = frame();
		if (!f)
			break;
		t += lastframetime()/1e9;
		render2(f, t);
	}
	return 0;
}
