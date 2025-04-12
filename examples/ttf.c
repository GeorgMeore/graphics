#include "types.h"
#include "color.h"
#include "image.h"
#include "win.h"
#include "draw.h"
#include "io.h"
#include "panic.h"
#include "alloc.h"

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

typedef enum {
	OnCurve = 1<<0,
	XShort  = 1<<1,
	YShort  = 1<<2,
	Repeat  = 1<<3,
	XFlag   = 1<<4,
	YFlag   = 1<<5,
} GlyfFlag;

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

typedef struct {
	I16 ncont;
	U16 *ends;
	U16 nvert;
	I16 *xy[2];
	U8  *on;
} GlyfInfo;

GlyfInfo readglyph(IOBuffer *font)
{
	GlyfInfo gi = {};
	gi.ncont = readbe(font, 2);
	if (!gi.ncont)
		return gi;
	skip(font, 2+2+2+2); /* xMin, yMin, xMax, yMax */
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
		for (I16 i = 0; i < gi.nvert; i++) {
			if (flags[i] & cflags[comp][0]) {
				if (flags[i] & cflags[comp][1])
					gi.xy[comp][i] = readbe(font, 1);
				else
					gi.xy[comp][i] = -readbe(font, 1);
			} else {
				if (flags[i] & cflags[comp][1])
					gi.xy[comp][i] = 0;
				else
					gi.xy[comp][i] = readbe(font, 2);
			}
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

//#define FONT "/usr/share/fonts/TTF/IBMPlexSerif-Regular.ttf"
#define FONT "/usr/share/fonts/noto/NotoSerif-Regular.ttf"

int main(int, char **argv)
{
	IOBuffer font = {};
	if (!bopen(&font, FONT, 'r'))
		panic("failed to open the font");
	FontInfo fi = readfontdir(&font);
	if (!bseek(&font, fi.glyf))
		panic("failed to seek the glyf table");
	GlyfInfo gi = readglyphno(&font, fi, 15);
	winopen(1920, 1080, argv[0], 60);
	while (!keyisdown('q')) {
		Image *f = framebegin();
		drawclear(f, RGBA(18, 18, 18, 255));
		I16 xp = 400, yp = 400, xs = 0, ys = 0;
		for (I16 i = 0, contour = 0, begin = 1; i < gi.nvert; i++) {
			I16 xc = xp + gi.xy[0][i];
			I16 yc = yp + gi.xy[1][i];
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
		frameend();
	}
	return 0;
}
