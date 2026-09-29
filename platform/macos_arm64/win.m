#import <AppKit/AppKit.h>
#import <QuartzCore/CATransaction.h>
#import <Carbon/Carbon.h>

/* NOTE: need absolute paths because of filename conflicts with apple stuff */
#include "../../types.h"
#include "../../color.h"
#include "../../image.h"
#include "../../win.h"
#include "../../ntime.h"
#include "../../alloc.h"
#include "../../math.h"

/* NOTE: this module is compiled without ARC, so no problems
 * with storing object pointers in C structs */
typedef struct {
	Image         fb;
	NSApplication *app;
	NSWindow      *win;
	CGFloat       scale;
	U64           targetns;
	U64           startns;
	U64           framens;
	OK            mouselocked;
	I             mousex;
	I             mousey;
	U64           keys, prevkeys;
	U64           btns, prevbtns;
	OK            gotinput;
	OK            dead, resized;
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
	/* NOTE: by setting contents to nil and flushing we aim to
	 * ensure that the to-be-freed buffer won't be accessed through
	 * a still alive CGImage, this is a hack */
	B.win.contentView.layer.contents = nil;
	[CATransaction flush];
	pfree(B.fb.p);
	B.fb.p = palloc(w*h*sizeof(Color));
	B.fb.w = w;
	B.fb.h = h;
	B.fb.s = w;
}

static void evwatch(NSNotificationName name, OK *flag)
{
	[[NSNotificationCenter defaultCenter] addObserverForName:name
		object:B.win
		queue:nil
		usingBlock:^(NSNotification *){
			*flag = 1;
		}];
}

/* TODO: maybe try rendering through IOSurfaceRef */
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
	evwatch(NSWindowDidResizeNotification, &B.resized);
	evwatch(NSWindowDidChangeBackingPropertiesNotification, &B.resized);
	evwatch(NSWindowWillCloseNotification, &B.dead);
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
	B.win.contentView.wantsLayer = YES;
	onresize();
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

OK keyisdown(Key k)
{
	return BOOL(B.keys & k);
}

OK keywaspressed(Key k)
{
	return BOOL(~B.keys & B.prevkeys & k);
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

static void onkey(UInt16 code, OK isdown)
{
	Key k;
	switch (code) {
	case kVK_ANSI_Grave:        k = KeyGrave;     break;
	case kVK_ANSI_0:            k = Key0;         break;
	case kVK_ANSI_1:            k = Key1;         break;
	case kVK_ANSI_2:            k = Key2;         break;
	case kVK_ANSI_3:            k = Key3;         break;
	case kVK_ANSI_4:            k = Key4;         break;
	case kVK_ANSI_5:            k = Key5;         break;
	case kVK_ANSI_6:            k = Key6;         break;
	case kVK_ANSI_7:            k = Key7;         break;
	case kVK_ANSI_8:            k = Key8;         break;
	case kVK_ANSI_9:            k = Key9;         break;
	case kVK_ANSI_Minus:        k = KeyMinus;     break;
	case kVK_ANSI_Equal:        k = KeyEqual;     break;
	case kVK_ANSI_A:            k = KeyA;         break;
	case kVK_ANSI_B:            k = KeyB;         break;
	case kVK_ANSI_C:            k = KeyC;         break;
	case kVK_ANSI_D:            k = KeyD;         break;
	case kVK_ANSI_E:            k = KeyE;         break;
	case kVK_ANSI_F:            k = KeyF;         break;
	case kVK_ANSI_G:            k = KeyG;         break;
	case kVK_ANSI_H:            k = KeyH;         break;
	case kVK_ANSI_I:            k = KeyI;         break;
	case kVK_ANSI_J:            k = KeyJ;         break;
	case kVK_ANSI_K:            k = KeyK;         break;
	case kVK_ANSI_L:            k = KeyL;         break;
	case kVK_ANSI_M:            k = KeyM;         break;
	case kVK_ANSI_N:            k = KeyN;         break;
	case kVK_ANSI_O:            k = KeyO;         break;
	case kVK_ANSI_P:            k = KeyP;         break;
	case kVK_ANSI_Q:            k = KeyQ;         break;
	case kVK_ANSI_R:            k = KeyR;         break;
	case kVK_ANSI_S:            k = KeyS;         break;
	case kVK_ANSI_T:            k = KeyT;         break;
	case kVK_ANSI_U:            k = KeyU;         break;
	case kVK_ANSI_V:            k = KeyV;         break;
	case kVK_ANSI_W:            k = KeyW;         break;
	case kVK_ANSI_X:            k = KeyX;         break;
	case kVK_ANSI_Y:            k = KeyY;         break;
	case kVK_ANSI_Z:            k = KeyZ;         break;
	case kVK_ANSI_LeftBracket:  k = KeyLBracket;  break;
	case kVK_ANSI_RightBracket: k = KeyRBracket;  break;
	case kVK_ANSI_Backslash:    k = KeyBackslash; break;
	case kVK_ANSI_Semicolon:    k = KeySemicolon; break;
	case kVK_ANSI_Quote:        k = KeyQuote;     break;
	case kVK_ANSI_Comma:        k = KeyComma;     break;
	case kVK_ANSI_Period:       k = KeyDot;       break;
	case kVK_ANSI_Slash:        k = KeySlash;     break;
	case kVK_Tab:               k = KeyTab;       break;
	case kVK_CapsLock:          k = KeyCaps;      break;
	case kVK_Shift:             k = KeyLShift;    break;
	case kVK_Control:           k = KeyLCtrl;     break;
	case kVK_Command:           k = KeyLWin;      break;
	case kVK_Option:            k = KeyLAlt;      break;
	case kVK_Space:             k = KeySpace;     break;
	case kVK_RightOption:       k = KeyRAlt;      break;
	case kVK_RightCommand:      k = KeyRWin;      break;
	case kVK_RightControl:      k = KeyRCtrl;     break;
	case kVK_RightShift:        k = KeyRShift;    break;
	case kVK_Return:            k = KeyEnter;     break;
	case kVK_Delete:            k = KeyBackspace; break;
	default:
		return;
	}
	if (isdown)
		B.keys |= k;
	else
		B.keys &= ~k;
	B.gotinput = 1;
}

static void onmousemove(NSEvent *ev)
{
	if (!B.mouselocked)
		return;
	B.mousex += ev.deltaX * B.scale;
	B.mousey += ev.deltaY * B.scale;
	B.gotinput = 1;
}

static void onmousebtn(NSInteger num, OK isdown)
{
	Btn b;
	switch (num) {
	case 0: b = BtnLeft;   break;
	case 1: b = BtnRight;  break;
	case 2: b = BtnMiddle; break;
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

static void onflagschanged(NSUInteger flags)
{
	onkey(kVK_Shift,        BOOL(flags & NX_DEVICELSHIFTKEYMASK));
	onkey(kVK_RightShift,   BOOL(flags & NX_DEVICERSHIFTKEYMASK));
	onkey(kVK_Control,      BOOL(flags & NX_DEVICELCTLKEYMASK));
	onkey(kVK_RightControl, BOOL(flags & NX_DEVICERCTLKEYMASK));
	onkey(kVK_Command,      BOOL(flags & NX_DEVICELCMDKEYMASK));
	onkey(kVK_RightCommand, BOOL(flags & NX_DEVICERCMDKEYMASK));
	onkey(kVK_Option,       BOOL(flags & NX_DEVICELALTKEYMASK));
	onkey(kVK_RightOption,  BOOL(flags & NX_DEVICERALTKEYMASK));
	/* NOTE: this actually tracks caps mode not press/release */
	onkey(kVK_CapsLock,     BOOL(flags & NSEventModifierFlagCapsLock));
}

static void onscrollwheel(CGFloat deltaY)
{
	Btn b;
	if (deltaY > 0)
		b = BtnUp;
	else if (deltaY < 0)
		b = BtnDown;
	else
		return;
	B.prevbtns |= b;
	B.gotinput = 1;
}

/* NOTE: unfortunately this style of api where the main loop
 * is owned by the user makes smooth resize on AppKit impossible,
 * since sendEvent will loop until the drag ends */

static NSEvent *nextev(BOOL dequeue)
{
	return [B.app nextEventMatchingMask:NSEventMaskAny
		untilDate:[NSDate distantPast]
		inMode:NSDefaultRunLoopMode
		dequeue:dequeue];
}

static void evpoll(void)
{
	B.prevkeys = B.keys;
	B.prevbtns = B.btns;
	if (!B.targetns && !B.gotinput) {
		while (!nextev(NO))
			sleepns(1.5e6);
	}
	B.resized = B.gotinput = 0;
	for (;;) {
		NSEvent *ev = nextev(YES);
		if (!ev)
			break;
		switch (ev.type) {
		case NSEventTypeKeyDown:
			if (!ev.isARepeat)
				onkey(ev.keyCode, 1);
			continue;
		case NSEventTypeKeyUp:
			onkey(ev.keyCode, 0);
			continue;
		case NSEventTypeMouseMoved:
		case NSEventTypeLeftMouseDragged:
		case NSEventTypeRightMouseDragged:
			onmousemove(ev);
			continue;
		case NSEventTypeLeftMouseDown:
		case NSEventTypeRightMouseDown:
		case NSEventTypeOtherMouseDown:
			onmousebtn(ev.buttonNumber, 1);
			break;
		case NSEventTypeLeftMouseUp:
		case NSEventTypeRightMouseUp:
		case NSEventTypeOtherMouseUp:
			onmousebtn(ev.buttonNumber, 0);
			break;
		case NSEventTypeFlagsChanged:
			onflagschanged(ev.modifierFlags);
			break;
		case NSEventTypeScrollWheel:
			onscrollwheel(ev.deltaY);
			break;
		default:
			break;
		}
		[B.app sendEvent:ev];
	}
	if (B.resized)
		onresize();
}

Image* frame(void)
{
	@autoreleasepool {
	if (!B.win || B.dead)
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
	if (B.dead)
		return 0;
	return &B.fb;
	}
}
