#include "types.h"
#include "color.h"
#include "image.h"
#include "win.h"
#include "draw.h"
#include "io.h"
#include "panic.h"
#include "alloc.h"
#include "mlib.h"

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

GlyfInfo readglyph(IOBuffer *font)
{
	GlyfInfo gi = {};
	gi.ncont = readbe(font, 2);
	if (!gi.ncont)
		return gi;
	gi.lim[0][0] = readbe(font, 2);
	gi.lim[1][0] = readbe(font, 2);
	gi.lim[0][1] = readbe(font, 2);
	gi.lim[1][1] = readbe(font, 2);
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
	gi.on = memalloc(gi.nvert);
	gi.xy[0] = memalloc(gi.nvert * sizeof(I16));
	gi.xy[1] = memalloc(gi.nvert * sizeof(I16));
	U8 cflags[2][2] = {{XShort, XFlag}, {YShort, YFlag}};
	for (I16 comp = 0; comp < 2; comp++) {
		for (I16 i = 0, prev = 0; i < gi.nvert; i++) {
			if (flags[i] & cflags[comp][0]) {
				if (flags[i] & cflags[comp][1])
					gi.xy[comp][i] = prev + readbe(font, 1);
				else
					gi.xy[comp][i] = prev - readbe(font, 1);
			} else {
				if (flags[i] & cflags[comp][1])
					gi.xy[comp][i] = prev;
				else
					gi.xy[comp][i] = prev + readbe(font, 2);
			}
			prev = gi.xy[comp][i];
			gi.on[i] = flags[i] & OnCurve;
		}
	}
	return gi;
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

typedef struct {
	I16 x1, y1;
	I16 x2, y2;
} Segment;

typedef struct {
	I16 *xy[2];
	U16 n;
} Outline;

void drawbezier2(Image *i, I16 x1, I16 y1, I16 x2, I16 y2, I16 x3, I16 y3, Color c)
{
	I16 x = DIVROUND((x1 + x2) + (x2 + x3), 4);
	I16 y = DIVROUND((y1 + y2) + (y2 + y3), 4);
	if (ABS(x1 - x) + ABS(x2 - x) + ABS(y1 - y) + ABS(y2 - y) >= 5) {
		drawbezier2(i, x1, y1, (x1+x2)/2, (y1+y2)/2, x, y, c);
		drawbezier2(i, x, y, (x2+x3)/2, (y2+y3)/2, x3, y3, c);
	} else {
		drawline(i, x1, y1, x, y, c);
		drawline(i, x, y, x3, y3, c);
	}
}

void outline(Image *f, I16 x0, I16 y0, GlyfInfo gi)
{
	//Outline o = {};
	for (I16 cont = 0; cont < gi.ncont; cont++) {
		U16 start = cont > 0 ? gi.ends[cont-1]+1 : 0, end = gi.ends[cont], n = end - start + 1;
		for (U16 i = 0; i < n; i++) {
			U16 curr = start + i;
			I16 x1, y1, x2, y2, x3, y3;
			if (gi.on[curr]) {
				U16 next = start + MOD(i+1, n), nnext = start + MOD(i+2, n);
				x1 = gi.xy[0][curr], y1 = gi.xy[1][curr];
				x2 = gi.xy[0][next], y2 = gi.xy[1][next];
				if (gi.on[next]) {
					drawline(f, x0+x1, y0-y1, x0+x2, y0-y2, RGBA(200, 200, 20, 50));
				} else {
					if (gi.on[nnext])
						x3 = gi.xy[0][nnext], y3 = gi.xy[1][nnext];
					else
						x3 = (gi.xy[0][next]+gi.xy[0][nnext])/2, y3 = (gi.xy[1][next]+gi.xy[1][nnext])/2;
					drawbezier2(f, x0+x1, y0-y1, x0+x2, y0-y2, x0+x3, y0-y3, RGBA(200, 200, 20, 50));
					i += 1;
				}
			} else {
				U16 prev = start + MOD(i-1, n), next = start + MOD(i+1, n);
				x2 = gi.xy[0][curr], y2 = gi.xy[1][curr];
				if (gi.on[prev])
					x1 = gi.xy[0][prev], y1 = gi.xy[1][prev];
				else
					x1 = (gi.xy[0][prev]+gi.xy[0][curr])/2, y1 = (gi.xy[1][prev]+gi.xy[1][curr])/2;
				if (gi.on[next])
					x3 = gi.xy[0][next], y3 = gi.xy[1][next];
				else
					x3 = (gi.xy[0][next]+gi.xy[0][curr])/2, y3 = (gi.xy[1][next]+gi.xy[1][curr])/2;
				drawbezier2(f, x0+x1, y0-y1, x0+x2, y0-y2, x0+x3, y0-y3, RGBA(200, 200, 20, 50));
			}
		}
	}
	//return o;
}

void fill(Image *f, Outline o)
{
}

void drawroughoutline(Image *f, I16 x, I16 y, GlyfInfo gi)
{
	I16 xp = x, yp = y, xs = 0, ys = 0;
	for (I16 i = 0, contour = 0, begin = 1; i < gi.nvert; i++) {
		I16 xc = x + gi.xy[0][i];
		I16 yc = y + gi.xy[1][i];
		if (begin) {
			xs = xc, ys = yc;
			begin = 0;
		} else {
			drawline(f, xp, yp, xc, yc, WHITE);
			begin = (i == gi.ends[contour]);
			if (begin) {
				contour += 1;
				drawline(f, xs, ys, xc, yc, WHITE);
			}
		}
		drawsmoothcircle(f, xc, yc, 5, WHITE);
		xp = xc, yp = yc;
	}
}

#define FONT "/usr/share/fonts/TTF/IBMPlexSerif-Regular.ttf"
//#define FONT "/usr/share/fonts/noto/NotoSerif-Regular.ttf"

int main(int, char **argv)
{
	IOBuffer font = {};
	if (!bopen(&font, FONT, 'r'))
		panic("failed to open the font");
	FontInfo fi = readfontdir(&font);
	U16 n = 11;
	GlyfInfo gi = readglyphno(&font, fi, n);
	//Outline o = convert(gi);
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
		outline(f, 200, 800, gi);
		frameend();
	}
	return 0;
}
