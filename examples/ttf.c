#include "types.h"
#include "color.h"
#include "image.h"
#include "win.h"
#include "draw.h"
#include "io.h"
#include "panic.h"
#include "alloc.h"
#include "mlib.h"
#include "poly.h"
#include "math.h"

/* TODO: use this sometime later to search for leaks
#include <stdlib.h>
#define memalloc malloc
#define memfree  free
*/

U64 readbe(IOBuffer *b, U8 bytes)
{
	U64 n = 0;
	for (U8 i = 0; i < bytes; i++) {
		I c = bread(b);
		if (c == -1)
			return -1;
		n = n << 8 | c;
	}
	return n;
}

U64 readle(IOBuffer *b, U8 bytes)
{
	U64 n = 0;
	for (U8 i = 0; i < bytes; i++) {
		I c = bread(b);
		if (c == -1)
			return -1;
		n |= c << (8*i);
	}
	return n;
}

void skip(IOBuffer *b, U32 bytes)
{
	for (U32 i = 0; i < bytes; i++)
		bread(b);
}

U32 strtag(char s[4])
{
	return s[0]<<(0*8)|s[1]<<(1*8)|s[2]<<(2*8)|s[3]<<(3*8);
}

typedef struct {
	U32 glyf;
	U32 cmap;
	U32 maxp;
	U32 head;
	U32 loca;
	U8  locasz;
	U16 maxpts;
	U16 maxconts;
	U16 maxcpts;
	U16 maxcconts;
} FontInfo;

FontInfo readfontdir(IOBuffer *font)
{
	FontInfo fi = {};
	skip(font, 4); /* scaler type */
	U16 numTables = readbe(font, 2);
	skip(font, 2+2+2); /* searchRange, entrySelector, rangeShift */
	for (U16 t = 0; t < numTables; t++) {
		U32 tag = readle(font, 4);
		skip(font, 4); /* checkSum */
		U32 offset = readbe(font, 4);
		skip(font, 4); /* length */
		if (tag == strtag("glyf"))
			fi.glyf = offset;
		if (tag == strtag("cmap"))
			fi.cmap = offset;
		if (tag == strtag("maxp"))
			fi.maxp = offset;
		if (tag == strtag("head"))
			fi.head = offset;
		if (tag == strtag("loca"))
			fi.loca = offset;
	}
	if (fi.head && bseek(font, fi.head)) {
		/* version, fontRevision, checkSumAdjustment, magicNumber,
		 * flags, unitsPerEm, created, modified, xMin, yMin, xMax, yMax,
		 * macStyle, lowestRecPPEM, fontDirectionHint */
		skip(font, 4+4+4+4+2+2+8+8+2+2+2+2+2+2+2);
		I16 indexToLocFormat = readbe(font, 2);
		if (indexToLocFormat == 0)
			fi.locasz = 2;
		if (indexToLocFormat == 1)
			fi.locasz = 4;
	}
	if (fi.maxp && bseek(font, fi.maxp)) {
		skip(font, 4+2); /* version, numGlyphs */
		fi.maxpts = readbe(font, 2);
		fi.maxconts = readbe(font, 2);
		fi.maxcpts = readbe(font, 2);
		fi.maxcconts = readbe(font, 2);
	}
	return fi;
}

typedef enum {
	OnCurve = 1<<0,
	XShort  = 1<<1,
	YShort  = 1<<2,
	Repeat  = 1<<3,
	XFlag   = 1<<4,
	YFlag   = 1<<5,
} GlyfFlag;

typedef struct {
	I16 ncont;
	U16 *ends;
	U16 nvert;
	I16 *xy[2];
	I16 lim[2][2];
	U8  *on;
} GlyfInfo;

GlyfInfo readsimpleglyph(IOBuffer *font, GlyfInfo gi)
{
	gi.ends = memalloc(gi.ncont * sizeof(U16));
	for (I16 i = 0; i < gi.ncont; i++)
		gi.ends[i] = readbe(font, 2);
	gi.nvert = gi.ends[gi.ncont-1] + 1;
	U16 ilen = readbe(font, 2);
	skip(font, ilen); /* instructions */
	U8 *flags = memalloc(gi.nvert);
	for (I16 i = 0; i < gi.nvert; i++) {
		flags[i] = readbe(font, 1);
		if (flags[i] & Repeat) {
			U8 count = readbe(font, 1);
			for (; count; count--, i++)
				flags[i+1] = flags[i];
		}
	}
	/* NOTE: after filling missing point we'll have at most twice as much points */
	gi.xy[0] = memalloc(gi.nvert*2 * sizeof(I16));
	gi.xy[1] = memalloc(gi.nvert*2 * sizeof(I16));
	U8 cflags[2][2] = {{XShort, XFlag}, {YShort, YFlag}};
	for (I16 comp = 0; comp < 2; comp++) {
		for (I16 i = 0, prev = 0; i < gi.nvert; i++) {
			if (flags[i] & cflags[comp][0]) {
				if (flags[i] & cflags[comp][1])
					gi.xy[comp][gi.nvert + i] = prev + readbe(font, 1);
				else
					gi.xy[comp][gi.nvert + i] = prev - readbe(font, 1);
			} else {
				if (flags[i] & cflags[comp][1])
					gi.xy[comp][gi.nvert + i] = prev;
				else
					gi.xy[comp][gi.nvert + i] = prev + readbe(font, 2);
			}
			prev = gi.xy[comp][gi.nvert + i];
		}
	}
	/* NOTE: now we can actually restore the missing control points */
	gi.on = memalloc(gi.nvert*2);
	for (I16 cont = 0, start = 0, j = 0; cont < gi.ncont; cont++) {
		U16 n = gi.ends[cont]+1-start;
		for (U16 i = 0; i < n; i++, j++) {
			U16 curr = start + i, prev = start + MOD(i-1, n);
			I16 x1 = gi.xy[0][gi.nvert+curr], y1 = gi.xy[1][gi.nvert+curr];
			if (~flags[curr] & ~flags[prev] & OnCurve) {
				I16 x2 = gi.xy[0][gi.nvert+prev], y2 = gi.xy[1][gi.nvert+prev];
				gi.xy[0][j] = (x1+x2)/2, gi.xy[1][j] = (y1+y2)/2, gi.on[j] = 1;
				j++;
			}
			gi.xy[0][j] = x1, gi.xy[1][j] = y1, gi.on[j] = flags[curr] & OnCurve;
		}
		/* TODO: handle the [off, on, ..., on] case */
		gi.ends[cont] = j-1;
		start += n;
	}
	memfree(flags);
	return gi;
}

typedef enum {
	ArgsWords    = 1<<0,
	ArgsXY       = 1<<1,
	RoundXY      = 1<<2,
	HaveScale    = 1<<3,
	MoreComp     = 1<<5,
	HaveXYScale  = 1<<6,
	Have2x2      = 1<<7,
	HaveInst     = 1<<8,
	UseMyMetrics = 1<<9,
	OverlapComp  = 1<<10,
} CompFlag;

GlyfInfo readcompoundglyph(IOBuffer *font, GlyfInfo gi)
{
	I16 n = -gi.ncont;
	for (I16 comp = 0; comp < n; comp++) {
		U16 flags = readbe(font, 2);
		U16 index = readbe(font, 2);
		if (flags & ArgsWords) {
			skip(font, 2);
		}
	}
	return gi;
}

GlyfInfo readglyph(IOBuffer *font)
{
	GlyfInfo gi = {};
	gi.ncont = readbe(font, 2);
	if (gi.ncont == 0)
		return gi;
	gi.lim[0][0] = readbe(font, 2);
	gi.lim[1][0] = readbe(font, 2);
	gi.lim[0][1] = readbe(font, 2);
	gi.lim[1][1] = readbe(font, 2);
	if (gi.ncont > 0)
		return readsimpleglyph(font, gi);
	else
		return readcompoundglyph(font, gi);
}

GlyfInfo readglyphno(IOBuffer *font, FontInfo fi, U16 no)
{
	bseek(font, fi.loca + no*fi.locasz);
	U32 offset = readbe(font, fi.locasz);
	if (fi.locasz == 2)
		offset *= 2;
	bseek(font, fi.glyf + offset);
	return readglyph(font);
}

I32 isectline(I16 rx, I16 ry, I16 x1, I16 y1, I16 x2, I16 y2)
{
	if (y1 == y2 || ry < MIN(y1, y2) || ry > MAX(y1, y2) || rx > MAX(x1, x2))
		return 0;
	if ((ry == y1 && rx <= x1) || (ry == y2 && rx <= x2))
		return SIGN(y2 - y1);
	I64 o = SIGN((ry - y1)*(x2 - x1) - (rx - x1)*(y2 - y1)) * SIGN(y2 - y1);
	if (o < 0)
		return 0;
	return 2*SIGN(y2 - y1);
}

I32 isectcurve(I16 rx, I16 ry, I16 x1, I16 y1, I16 x2, I16 y2, I16 x3, I16 y3)
{
	if (ry < MIN3(y1, y2, y3) || ry > MAX3(y1, y2, y3) || rx > MAX3(x1, x2, x3))
		return 0;
	F64 a = y1 - 2*y2 + y3;
	F64 b = 2*(y2 - y1);
	F64 c = y1 - ry;
	Poly x = {{x1, 2*(x2 - x1), x1 - 2*x2 + x3}, 2};
	if (a == 0) {
		F64 t = -c/b;
		if (t >= 0 && t <= 1 && rx <= eval(x, t)) {
			if (t == 0 || t == 1)
				return SIGN(y2 - y1);
			return 2*(SIGN(y2 - y1));
		}
		return 0;
	}
	I64 d = b*b - 4*a*c;
	if (d < 0)
		return 0;
	if (d == 0) {
		F64 t = -b/(2*a);
		if (t >= 0 && t <= 1 && rx <= eval(x, t))
			if (t == 0 || t == 1)
				return SIGN(y3 - y1);
		return 0;
	}
	I32 wn = 0;
	F64 t[2] = {(-b - fsqrt(d))/(2*a), (-b + fsqrt(d))/(2*a)};
	for (int i = 0; i < 2; i++) {
		if (t[i] >= 0 && t[i] <= 1 && rx <= eval(x, t[i])) {
			if (t[i] == 0)
				wn += SIGN(y2 - y1);
			else if (t[i] == 1)
				wn += SIGN(y3 - y2);
			else if (t[i] < -b/(2*a))
				wn += 2*SIGN(y2 - y1);
			else
				wn += 2*SIGN(y3 - y2);
		}
	}
	return wn;
}

I32 isectcurve2(I16 rx, I16 ry, I16 x1, I16 y1, I16 x2, I16 y2, I16 x3, I16 y3)
{
	if (ry < MIN3(y1, y2, y3) || ry > MAX3(y1, y2, y3) || rx > MAX3(x1, x2, x3))
		return 0;
	const I64 n = 32;
	I32 wn = 0;
	for (I64 t = 1, xp = x1, yp = y1; t <= n; t++) {
		I64 x = DIVROUND(SQUARE(n-t)*x1 + 2*(n-t)*t*x2 + SQUARE(t)*x3, SQUARE(n));
		I64 y = DIVROUND(SQUARE(n-t)*y1 + 2*(n-t)*t*y2 + SQUARE(t)*y3, SQUARE(n));
		if (x != xp || y != yp)
			wn += isectline(rx, ry, xp, yp, x, y);
		xp = x, yp = y;
	}
	return wn;
}

void drawraster(Image *f, I16 x0, I16 y0, GlyfInfo gi, Color c)
{
	for (I16 x = gi.lim[0][0]; x <= gi.lim[0][1]; x++)
	for (I16 y = gi.lim[1][0]; y <= gi.lim[1][1]; y++) {
		I32 wn = 0;
		for (I16 cont = 0; cont < gi.ncont; cont++) {
			U16 start = cont > 0 ? gi.ends[cont-1]+1 : 0, n = gi.ends[cont]+1-start;
			for (U16 i = 0; i < n;) {
				U16 curr = start + i, next = start + MOD(i+1, n), last = start + MOD(i+2, n);
				I16 x1 = gi.xy[0][curr], y1 = gi.xy[1][curr];
				I16 x2 = gi.xy[0][next], y2 = gi.xy[1][next];
				I16 x3 = gi.xy[0][last], y3 = gi.xy[1][last];
				if (gi.on[next]) {
					wn += isectline(x, y, x1, y1, x2, y2);
					i += 1;
				} else {
					wn += isectcurve(x, y, x1, y1, x2, y2, x3, y3);
					i += 2;
				}
			}
		}
		if (wn && CHECKX(f, x0+x) && CHECKY(f, y0-y))
			PIXEL(f, (x0+x), (y0-y)) = c;
	}
}

void drawoutline(Image *f, I16 x0, I16 y0, GlyfInfo gi, Color c)
{
	for (I16 cont = 0; cont < gi.ncont; cont++) {
		U16 start = cont > 0 ? gi.ends[cont-1]+1 : 0, n = gi.ends[cont]+1-start;
		for (U16 i = 0; i < n;) {
			U16 curr = start + i, next = start + MOD(i+1, n), last = start + MOD(i+2, n);
			I16 x1 = gi.xy[0][curr], y1 = gi.xy[1][curr];
			I16 x2 = gi.xy[0][next], y2 = gi.xy[1][next];
			I16 x3 = gi.xy[0][last], y3 = gi.xy[1][last];
			if (gi.on[next]) {
				drawline(f, x0+x1, y0-y1, x0+x2, y0-y2, c);
				i += 1;
			} else {
				drawbezier(f, x0+x1, y0-y1, x0+x2, y0-y2, x0+x3, y0-y3, c);
				i += 2;
			}
		}
	}
}

/* TODO: check out SDF */

#define FONT "/usr/share/fonts/TTF/IBMPlexSerif-Regular.ttf"
//#define FONT "/usr/share/fonts/TTF/IBMPlexMono-Regular.ttf"
//#define FONT "/usr/share/fonts/noto/NotoSerif-Regular.ttf"

int main(int, char **argv)
{
	IOBuffer font = {};
	if (!bopen(&font, FONT, 'r'))
		panic("failed to open the font");
	FontInfo fi = readfontdir(&font);
	U16 n = 13;
	GlyfInfo gi = readglyphno(&font, fi, n);
	winopen(1920, 1080, argv[0], 60);
	while (!keyisdown('q')) {
		Image *f = framebegin();
		drawclear(f, RGBA(18, 18, 18, 255));
		if (keywaspressed('p') && n > 0) {
			n -= 1;
			println(OD(n));
			gi = readglyphno(&font, fi, n);
		}
		if (keywaspressed('n')) {
			n += 1;
			println(OD(n));
			gi = readglyphno(&font, fi, n);
		}
		if (keyisdown('r'))
			drawraster(f, 200, 800, gi, RGBA(200, 200, 20, 50));
		else
			drawoutline(f, 200, 800, gi, RGBA(200, 200, 20, 50));
		frameend();
	}
	return 0;
}
