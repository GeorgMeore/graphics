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

/* TODO: use this sometime later to search for leaks and oob
#include <stdlib.h>
#define memalloc malloc
#define memfree  free
*/

static U64 readbe(IOBuffer *b, U8 bytes)
{
	U64 n = 0;
	for (U8 i = 0; i < bytes; i++)
		n = n << 8 | bread(b);
	return n;
}

static void skip(IOBuffer *b, U32 bytes)
{
	for (U32 i = 0; i < bytes; i++)
		bread(b);
}

static U32 strtag(char s[4])
{
	return s[3]<<(0*8)|s[2]<<(1*8)|s[1]<<(2*8)|s[0]<<(3*8);
}

typedef struct {
	I16 ncont;
	U16 *ends;
	U16 nvert;
	I16 *xy[2];
	I16 lim[2][2];
	U8  *on;
	U16 advance;
	I16 lsb;
} Glyph;

typedef struct {
	Arena mem;
	I16   ascend;
	I16   descend;
	I16   linegap;
	U16   upm;
	I16   lim[2][2];
	U16   nglyph;
	Glyph *glyphs;
	U16   npoints;
	U16   *ctable[2];
} Font;

typedef enum {
	OnCurve = 1<<0,
	XShort  = 1<<1,
	YShort  = 1<<2,
	Repeat  = 1<<3,
	XFlag   = 1<<4,
	YFlag   = 1<<5,
} SimpFlag;

/* NOTE: Checking every read/seek operation would be too tedious, so instead I only
 * do that at specific checkpoints (e.g. loops/allocations based on what's read)
 * to find out if any error has occured up to this point (and early exit).
 *
 * To handle malformed files we also do some assertions, which at least guarantee
 * that there won't be any out-of-bounds reads or writes. */

#define CHECKPOINT(b) ({if ((b)->error) return;})
#define ASSERT(b, e) ({if (!(e)) {(b)->error = 1; return;}})

static void parsesimpleglyph(IOBuffer *b, Glyph *g, I16 ncont, U16 maxpts)
{
	U16 nvert = 0;
	for (I16 i = 0; i < ncont; i++) {
		U16 idx = readbe(b, 2);
		g->ends[g->ncont+i] = g->nvert + idx;
		ASSERT(b, nvert < idx + 1);
		nvert = idx + 1;
	}
	ASSERT(b, g->nvert + nvert <= 2*maxpts);
	U16 ilen = readbe(b, 2);
	skip(b, ilen); /* instructions */
	CHECKPOINT(b);
	for (I16 i = 0; i < nvert; i++) {
		g->on[g->nvert+i] = readbe(b, 1);
		if (g->on[g->nvert+i] & Repeat) {
			U8 count = readbe(b, 1);
			ASSERT(b, count < nvert - i);
			for (; count; count--, i++)
				g->on[g->nvert+i+1] = g->on[g->nvert+i];
		}
	}
	U8 cflags[2][2] = {{XShort, XFlag}, {YShort, YFlag}};
	for (I16 comp = 0; comp < 2; comp++) {
		for (I16 i = 0, prev = 0; i < nvert; i++) {
			if (g->on[g->nvert+i] & cflags[comp][0]) {
				if (g->on[g->nvert+i] & cflags[comp][1])
					g->xy[comp][g->nvert+i] = prev + readbe(b, 1);
				else
					g->xy[comp][g->nvert+i] = prev - readbe(b, 1);
			} else {
				if (g->on[g->nvert+i] & cflags[comp][1])
					g->xy[comp][g->nvert+i] = prev;
				else
					g->xy[comp][g->nvert+i] = prev + readbe(b, 2);
			}
			prev = g->xy[comp][g->nvert+i];
		}
	}
	for (I16 i = 0; i < nvert; i++)
		g->on[g->nvert+i] &= OnCurve;
	g->nvert += nvert;
	g->ncont += ncont;
}

typedef enum {
	ArgsWords    = 1<<0,
	ArgsXY       = 1<<1,
	HaveScale    = 1<<3,
	MoreComp     = 1<<5,
	HaveXYScale  = 1<<6,
	Have2x2      = 1<<7,
} CompFlag;

static void parsecompoundglyph(IOBuffer *b, Glyph *g, U32 glyf, U32 *locations, U16 maxconts, U16 maxpts)
{
	for (;;) {
		U16 flags = readbe(b, 2);
		CHECKPOINT(b);
		if (~flags & ArgsXY)
			panic("point offsets are not yet supported");
		if (flags & (HaveScale|HaveXYScale|Have2x2))
			panic("scaling is not yet supported");
		U16 index = readbe(b, 2);
		I16 x, y;
		if (flags & ArgsWords) {
			x = readbe(b, 2);
			y = readbe(b, 2);
		} else {
			x = (I8)readbe(b, 1);
			y = (I8)readbe(b, 1);
		}
		U64 curr = b->pos;
		U16 start = g->nvert;
		bseek(b, glyf + locations[index]);
		I16 ncont = readbe(b, 2);
		ASSERT(b, g->ncont + ncont <= maxconts);
		skip(b, 2+2+2+2); /* xMin, yMin, xMax, yMax */
		CHECKPOINT(b);
		if (ncont > 0)
			parsesimpleglyph(b, g, ncont, maxpts);
		if (ncont < 0)
			parsecompoundglyph(b, g, glyf, locations, maxconts, maxpts);
		CHECKPOINT(b);
		for (U16 i = start; i < g->nvert; i++) {
			g->xy[0][i] += x;
			g->xy[1][i] += y;
		}
		bseek(b, curr);
		if (~flags & MoreComp)
			break;
	}
}

static void parseglyph(IOBuffer *b, Font *f, U16 index, U32 glyf, U32 *locations, U16 maxconts, U16 maxpts)
{
	bseek(b, glyf + locations[index]);
	I16 ncont = readbe(b, 2);
	ASSERT(b, ncont <= maxconts);
	Glyph *g = &f->glyphs[index];
	g->lim[0][0] = readbe(b, 2);
	g->lim[1][0] = readbe(b, 2);
	g->lim[0][1] = readbe(b, 2);
	g->lim[1][1] = readbe(b, 2);
	CHECKPOINT(b);
	if (ncont == 0)
		return;
	/* NOTE: after restoring missing point we'll have at most twice as much points */
	g->ncont = 0;
	g->nvert = maxpts;
	g->on = aralloc(&f->mem, maxpts*2);
	g->ends = aralloc(&f->mem, maxconts * sizeof(U16));
	g->xy[0] = aralloc(&f->mem, maxpts*2 * sizeof(I16));
	g->xy[1] = aralloc(&f->mem, maxpts*2 * sizeof(I16));
	if (ncont > 0)
		parsesimpleglyph(b, g, ncont, maxpts);
	else
		parsecompoundglyph(b, g, glyf, locations, maxconts, maxpts);
	CHECKPOINT(b);
	/* NOTE: now we can actually restore the missing control points */
	for (I16 cont = 0, start = maxpts, j = 0; cont < g->ncont; cont++) {
		U16 n = g->ends[cont]+1-start;
		for (U16 i = 0; i < n; i++, j++) {
			U16 curr = start + i, prev = start + MOD(i-1, n);
			I16 x1 = g->xy[0][curr], y1 = g->xy[1][curr];
			if (!g->on[curr] && !g->on[prev]) {
				I16 x2 = g->xy[0][prev], y2 = g->xy[1][prev];
				g->xy[0][j] = (x1+x2)/2, g->xy[1][j] = (y1+y2)/2, g->on[j] = 1;
				j++;
			}
			g->xy[0][j] = x1, g->xy[1][j] = y1, g->on[j] = g->on[curr];
		}
		/* TODO: handle the [off, on, ..., on] case */
		g->ends[cont] = j-1;
		start += n;
		g->nvert = j;
	}
}

static void parseglyphs(IOBuffer *b, Font *f, U32 head, U32 maxp, U32 glyf, U32 loca)
{
	bseek(b, maxp);
	skip(b, 4); /* version */
	U16 nglyph = readbe(b, 2);
	U16 maxsimppts = readbe(b, 2);
	U16 maxsimpconts = readbe(b, 2);
	U16 maxcomppts = readbe(b, 2);
	U16 maxcompconts = readbe(b, 2);
	U16 maxpts = MAX(maxsimppts, maxcomppts);
	U16 maxconts = MAX(maxsimpconts, maxcompconts);
	bseek(b, head);
	/* version, fontRevision, checkSumAdjustment, magicNumber, flags */
	skip(b, 4+4+4+4+2);
	f->upm = readbe(b, 2);
	skip(b, 8+8); /* created, modified */
	f->lim[0][0] = readbe(b, 2);
	f->lim[1][0] = readbe(b, 2);
	f->lim[0][1] = readbe(b, 2);
	f->lim[1][1] = readbe(b, 2);
	skip(b, 2+2+2); /* macStyle, lowestRecPPEM, fontDirectionHint */
	I16 indextolocformat = readbe(b, 2);
	ASSERT(b, indextolocformat == 0 || indextolocformat == 1);
	I16 locasize = 2 << indextolocformat;
	I16 locascale = 2 - indextolocformat;
	bseek(b, loca);
	CHECKPOINT(b);
	U32 *locations = aralloc(&f->mem, nglyph * sizeof(U32));
	for (U16 i = 0; i < nglyph; i++) {
		U32 offset = readbe(b, locasize);
		locations[i] = offset*locascale;
	}
	CHECKPOINT(b);
	f->nglyph = nglyph;
	f->glyphs = aralloc(&f->mem, f->nglyph * sizeof(Glyph));
	for (U16 i = 0; i < f->nglyph; i++)
		parseglyph(b, f, i, glyf, locations, maxconts, maxpts);
}

static void parsemetrics(IOBuffer *b, Font *f, U32 hmtx, U32 hhea)
{
	bseek(b, hhea);
	skip(b, 2+2); /* majorVersion, minorVersion */
	f->ascend = readbe(b, 2);
	f->descend = readbe(b, 2);
	f->linegap = readbe(b, 2);
	/* advanceWidthMax, minLeftSideBearing, minRightSideBearing,
	 * xMaxExtent, caretSlopeRise, caretSlopeRun, caretOffset,
	 * reserved(4*2), metricDataFormat */
	skip(b, 2+2+2+2+2+2+2+4*2+2);
	U16 numhw = readbe(b, 2);
	ASSERT(b, numhw <= f->nglyph);
	bseek(b, hmtx);
	CHECKPOINT(b);
	for (U16 i = 0, advance = 0; i < f->nglyph; i++) {
		if (i < numhw)
			advance = readbe(b, 2);
		U16 lsb = readbe(b, 2);
		f->glyphs[i].advance = advance;
		f->glyphs[i].lsb = lsb;
	}
}

/* TODO: support format 12 tables */
static void parsectable(IOBuffer *b, Font *f, U32 cmap)
{
	bseek(b, cmap);
	skip(b, 2); /* version */
	U16 ntab = readbe(b, 2);
	CHECKPOINT(b);
	for (U16 i = 0; i < ntab; i++) {
		U16 platformid = readbe(b, 2);
		skip(b, 2); /* platformSpecificID */
		U32 offset = readbe(b, 4);
		if (platformid != 0)
			continue;
		bseek(b, cmap + offset);
		U16 format = readbe(b, 2);
		if (format != 4)
			continue;
		skip(b, 2+2); /* length, language */
		U16 segcnt = readbe(b, 2)/2;
		skip(b, 2+2+2); /* searchRange, entrySelector, rangeShift */
		U64 table = b->pos;
		U32 npoints = 0;
		CHECKPOINT(b);
		for (U16 i = 0; i < segcnt; i++) {
			bseek(b, table + i*2);
			U16 end = readbe(b, 2);
			skip(b, segcnt*2);
			U16 start = readbe(b, 2);
			ASSERT(b, start <= end);
			npoints += end - start + 1;
		}
		CHECKPOINT(b);
		f->npoints = npoints;
		f->ctable[0] = aralloc(&f->mem, npoints*sizeof(U16));
		f->ctable[1] = aralloc(&f->mem, npoints*sizeof(U16));
		for (U16 i = 0, j = 0; i < segcnt; i++) {
			bseek(b, table + i*2);
			U16 end = readbe(b, 2);
			skip(b, segcnt*2);
			U16 start = readbe(b, 2);
			skip(b, segcnt*2 - 2);
			U16 iddelta = readbe(b, 2);
			skip(b, segcnt*2 - 2);
			U16 idroff = readbe(b, 2);
			if (idroff)
				skip(b, idroff - 2);
			CHECKPOINT(b);
			for (U32 p = start; p <= end; p++, j++) {
				f->ctable[0][j] = p;
				if (!idroff)
					f->ctable[1][j] = iddelta + p;
				else
					f->ctable[1][j] = readbe(b, 2);
				ASSERT(b, f->ctable[1][j] < f->nglyph);
			}
		}
		return;
	}
}

Font parsettf(IOBuffer *b)
{
	Font f = {};
	skip(b, 4); /* scaler type */
	U16 ntab = readbe(b, 2);
	skip(b, 2+2+2); /* searchRange, entrySelector, rangeShift */
	U32 glyf = 0, cmap = 0, hmtx = 0, hhea = 0, maxp = 0, head = 0, loca = 0;
	for (U16 t = 0; t < ntab; t++) {
		U32 tag = readbe(b, 4);
		skip(b, 4); /* checkSum */
		U32 offset = readbe(b, 4);
		skip(b, 4); /* length */
		if (tag == strtag("glyf"))
			glyf = offset;
		if (tag == strtag("cmap"))
			cmap = offset;
		if (tag == strtag("hmtx"))
			hmtx = offset;
		if (tag == strtag("hhea"))
			hhea = offset;
		if (tag == strtag("maxp"))
			maxp = offset;
		if (tag == strtag("head"))
			head = offset;
		if (tag == strtag("loca"))
			loca = offset;
	}
	if (!glyf || !cmap || !hmtx || !hhea || !maxp || !head || !loca)
		return f;
	parseglyphs(b, &f, head, maxp, glyf, loca);
	parsemetrics(b, &f, hmtx, hhea);
	parsectable(b, &f, cmap);
	if (b->error)
		panic("parsing failed");
	return f;
}

Glyph findglyph(Font f, U32 code)
{
	if (!f.ctable[0])
		return f.glyphs[0];
	U16 idx = 0;
	U16 l = 0, r = f.npoints;
	while (l < r && !idx) {
		U16 m = l + (r - l)/2;
		if (f.ctable[0][m] == code)
			idx = f.ctable[1][m];
		else if (f.ctable[0][m] < code)
			l = m + 1;
		else
			r = m;
	}
	return f.glyphs[idx];
}

/* TODO: this and isectcurve2 are easily avx-able, I should try it out */
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

void drawraster(Image *f, I16 x0, I16 y0, Glyph g, Color c)
{
	for (I16 x = g.lim[0][0]; x <= g.lim[0][1]; x++)
	for (I16 y = g.lim[1][0]; y <= g.lim[1][1]; y++) {
		I32 wn = 0;
		for (I16 cont = 0, start = 0; cont < g.ncont; cont++) {
			U16 n = g.ends[cont]+1-start;
			for (U16 i = 0; i < n;) {
				U16 curr = start + i, next = start + MOD(i+1, n), last = start + MOD(i+2, n);
				I16 x1 = g.xy[0][curr], y1 = g.xy[1][curr];
				I16 x2 = g.xy[0][next], y2 = g.xy[1][next];
				I16 x3 = g.xy[0][last], y3 = g.xy[1][last];
				if (g.on[next]) {
					wn += isectline(x, y, x1, y1, x2, y2);
					i += 1;
				} else {
					wn += isectcurve(x, y, x1, y1, x2, y2, x3, y3);
					i += 2;
				}
			}
			start += n;
		}
		if (wn && CHECKX(f, x0+x) && CHECKY(f, y0-y))
			PIXEL(f, (x0+x), (y0-y)) = c;
	}
}

void drawoutline(Image *f, I16 x0, I16 y0, Glyph g, Color c)
{
	for (I16 cont = 0, start = 0; cont < g.ncont; cont++) {
		U16 n = g.ends[cont]+1-start;
		for (U16 i = 0; i < n;) {
			U16 curr = start + i, next = start + MOD(i+1, n), last = start + MOD(i+2, n);
			I16 x1 = g.xy[0][curr], y1 = g.xy[1][curr];
			I16 x2 = g.xy[0][next], y2 = g.xy[1][next];
			I16 x3 = g.xy[0][last], y3 = g.xy[1][last];
			if (g.on[next]) {
				drawline(f, x0+x1, y0-y1, x0+x2, y0-y2, c);
				i += 1;
			} else {
				drawbezier(f, x0+x1, y0-y1, x0+x2, y0-y2, x0+x3, y0-y3, c);
				i += 2;
			}
		}
		start += n;
	}
}

/* TODO: check out SDF */
/* TODO: fuck buffered io, just read the whole font file into memory (or mmap) */

#define FONT "/usr/share/fonts/TTF/IBMPlexSerif-Regular.ttf"
//#define FONT "/usr/share/fonts/TTF/IBMPlexMono-Regular.ttf"
//#define FONT "/usr/share/fonts/noto/NotoSerif-Regular.ttf"
//#define FONT "/usr/share/fonts/liberation/LiberationSans-Regular.ttf"

int main(int, char **argv)
{
	IOBuffer file = {};
	if (!bopen(&file, FONT, 'r'))
		panic("failed to open the file");
	Font fn = parsettf(&file);
	bclose(&file);
	winopen(1920, 1080, argv[0], 60);
	U32 s[] = {'T', 'e', 's', 't'};
	U32 n = sizeof(s)/sizeof(s[0]);
	while (!keyisdown('q')) {
		Image *f = framebegin();
		drawclear(f, RGBA(18, 18, 18, 255));
		I16 x = 200, y = 800;
		for (U32 i = 0; i < n; i++) {
			Glyph g = findglyph(fn, s[i]);
			drawoutline(f, x+g.lsb, y, g, RGBA(200, 200, 20, 50));
			x += g.advance;
		}
		frameend();
	}
	return 0;
}
