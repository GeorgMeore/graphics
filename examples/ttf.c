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

static U64 readbe(IOBuffer *b, U8 bytes)
{
	U64 n = 0;
	for (U8 i = 0; i < bytes; i++) {
		I c = bread(b);
		if (c == -1)
			return 0;
		n = n << 8 | c;
	}
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

/* NOTE: When starting to parse a glyph we don't know how many points we'll have
 * to restore, but we can be sure that after the restoration the number of points
 * can at most double. We also know the max number of points a glyph can have.
 *
 * So we can do the following:
 *  1) allocate enough memory to hold twice the max number of points
 *  2) read all the points from the font file into the "upper" half
 *  3) move the points into the "lower" half, while restoring missing points */
static OK restorepts(Glyph *g, U16 maxpts)
{
	for (I16 cont = 0, start = maxpts, j = 0; cont < g->ncont; cont++) {
		U16 n = g->ends[cont] + 1 - start;
		I16 first = j;
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
		if (!g->on[first])
			return 0; /* TODO: handle first off curve */
		g->ends[cont] = j-1;
		start += n;
		g->nvert = j;
	}
	return 1;
}

typedef enum {
	OnCurve = 1<<0,
	XShort  = 1<<1,
	YShort  = 1<<2,
	Repeat  = 1<<3,
	XFlag   = 1<<4,
	YFlag   = 1<<5,
} SimpFlag;

static OK parsesimpleglyph(IOBuffer *b, Glyph *g, I16 ncont, U16 maxconts, U16 maxpts)
{
	if (g->ncont + ncont > maxconts)
		return 0;
	U16 nvert = 0;
	for (I16 i = 0; i < ncont; i++) {
		U16 idx = readbe(b, 2);
		g->ends[g->ncont+i] = g->nvert + idx;
		if (nvert > idx)
			return 0;
		nvert = idx + 1;
	}
	if (g->nvert + nvert > 2*maxpts)
		return 0;
	U16 ilen = readbe(b, 2);
	skip(b, ilen); /* instructions */
	for (I16 i = 0; i < nvert; i++) {
		g->on[g->nvert+i] = readbe(b, 1);
		if (g->on[g->nvert+i] & Repeat) {
			U8 count = readbe(b, 1);
			if (count >= nvert - i)
				return 0;
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
	return 1;
}

typedef enum {
	ArgsWords    = 1<<0,
	ArgsXY       = 1<<1,
	HaveScale    = 1<<3,
	MoreComp     = 1<<5,
	HaveXYScale  = 1<<6,
	Have2x2      = 1<<7,
} CompFlag;

static OK parsecompoundglyph(IOBuffer *b, Glyph *g, U32 glyf, U32 *locations, U16 maxconts, U16 maxpts)
{
	for (;;) {
		U16 flags = readbe(b, 2);
		if (!(flags & ArgsXY))
			return 0; /* TODO: point offsets */
		if (flags & (HaveScale|HaveXYScale|Have2x2))
			return 0; /* TODO: scaling */
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
		if (locations[index] == locations[index+1])
			continue;
		bseek(b, glyf + locations[index]);
		I16 ncont = readbe(b, 2);
		skip(b, 2+2+2+2); /* xMin, yMin, xMax, yMax */
		OK ok;
		if (ncont > 0)
			ok = parsesimpleglyph(b, g, ncont, maxconts, maxpts);
		if (ncont < 0)
			ok = parsecompoundglyph(b, g, glyf, locations, maxconts, maxpts);
		if (!ok)
			return 0;
		for (U16 i = start; i < g->nvert; i++) {
			g->xy[0][i] += x;
			g->xy[1][i] += y;
		}
		bseek(b, curr);
		if (~flags & MoreComp)
			break;
	}
	return 1;
}

static OK parseglyph(IOBuffer *b, Font *f, U16 index, U32 glyf, U32 *locations, U16 maxconts, U16 maxpts)
{
	if (locations[index] == locations[index+1])
		return 1;
	bseek(b, glyf + locations[index]);
	I16 ncont = readbe(b, 2);
	if (ncont > maxconts)
		return 0;
	if (ncont == 0)
		return 1;
	Glyph *g = &f->glyphs[index];
	g->lim[0][0] = readbe(b, 2);
	g->lim[1][0] = readbe(b, 2);
	g->lim[0][1] = readbe(b, 2);
	g->lim[1][1] = readbe(b, 2);
	OK ok = g->lim[0][0] < g->lim[0][1] && g->lim[1][0] < g->lim[1][1] &&
		g->lim[0][0] >= f->lim[0][0] && g->lim[0][1] <= f->lim[0][1] &&
		g->lim[1][0] >= f->lim[1][0] && g->lim[1][1] <= f->lim[1][1];
	if (!ok)
		return 0;
	g->nvert = maxpts;
	g->on = aralloc(&f->mem, maxpts*2);
	g->ends = aralloc(&f->mem, maxconts * sizeof(U16));
	g->xy[0] = aralloc(&f->mem, maxpts*2 * sizeof(I16));
	g->xy[1] = aralloc(&f->mem, maxpts*2 * sizeof(I16));
	if (ncont > 0)
		ok = parsesimpleglyph(b, g, ncont, maxconts, maxpts);
	else
		ok = parsecompoundglyph(b, g, glyf, locations, maxconts, maxpts);
	if (!ok)
		return 0;
	for (I16 comp = 0; comp < 2; comp++)
	for (U16 i = maxpts; i < g->nvert; i++)
		if (g->xy[comp][i] < g->lim[comp][0] || g->xy[comp][i] > g->lim[comp][1])
			return 0;
	if (!restorepts(g, maxpts))
		return 0;
	return 1;
}

static OK parseglyphs(IOBuffer *b, Font *f, U32 head, U32 maxp, U32 glyf, U32 loca)
{
	bseek(b, maxp);
	skip(b, 4); /* version */
	f->nglyph = readbe(b, 2);
	U16 maxsimppts = readbe(b, 2);
	U16 maxsimpconts = readbe(b, 2);
	U16 maxcomppts = readbe(b, 2);
	U16 maxcompconts = readbe(b, 2);
	U16 maxpts = MAX(maxsimppts, maxcomppts);
	U16 maxconts = MAX(maxsimpconts, maxcompconts);
	bseek(b, head);
	skip(b, 4+4+4+4+2); /* version, fontRevision, checkSumAdjustment, magicNumber, flags */
	f->upm = readbe(b, 2);
	skip(b, 8+8); /* created, modified */
	f->lim[0][0] = readbe(b, 2);
	f->lim[1][0] = readbe(b, 2);
	f->lim[0][1] = readbe(b, 2);
	f->lim[1][1] = readbe(b, 2);
	skip(b, 2+2+2); /* macStyle, lowestRecPPEM, fontDirectionHint */
	I16 indextolocformat = readbe(b, 2);
	if (indextolocformat != 0 && indextolocformat != 1)
		return 0;
	I16 locasize = 2 << indextolocformat;
	I16 locascale = 2 - indextolocformat;
	bseek(b, loca);
	U32 *locations = aralloc(&f->mem, f->nglyph * sizeof(U32));
	for (U16 i = 0; i < f->nglyph; i++) {
		U32 offset = readbe(b, locasize);
		locations[i] = offset*locascale;
	}
	f->glyphs = aralloc(&f->mem, f->nglyph * sizeof(Glyph));
	for (U16 i = 0; i < f->nglyph; i++) {
		f->glyphs[i] = (Glyph){};
		parseglyph(b, f, i, glyf, locations, maxconts, maxpts);
	}
	return 1;
}

static OK parsemetrics(IOBuffer *b, Font *f, U32 hmtx, U32 hhea)
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
	if (numhw > f->nglyph)
		return 0;
	bseek(b, hmtx);
	for (U16 i = 0, advance = 0; i < f->nglyph; i++) {
		if (i < numhw)
			advance = readbe(b, 2);
		U16 lsb = readbe(b, 2);
		f->glyphs[i].advance = advance;
		f->glyphs[i].lsb = lsb;
	}
	return 1;
}

/* TODO: support format 12 tables */
static OK parsectable(IOBuffer *b, Font *f, U32 cmap)
{
	bseek(b, cmap);
	skip(b, 2); /* version */
	U16 ntab = readbe(b, 2);
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
		for (U16 i = 0; i < segcnt; i++) {
			bseek(b, table + i*2);
			U16 end = readbe(b, 2);
			skip(b, segcnt*2);
			U16 start = readbe(b, 2);
			if (start > end)
				return 0;
			npoints += end - start + 1;
		}
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
			for (U32 p = start; p <= end; p++, j++) {
				f->ctable[0][j] = p;
				if (!idroff)
					f->ctable[1][j] = iddelta + p;
				else
					f->ctable[1][j] = readbe(b, 2);
				if (f->ctable[1][j] >= f->nglyph)
					return 0;
			}
		}
		return 1;
	}
	return 0;
}

/* NOTE: Checking every read/seek operation for errors would be tedious,
 * so there is just one check for io errors when all of the parsing is done.
 *
 * Together with some basic validation it gives "good enough" error resilience IMO. */
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
	OK ok = parseglyphs(b, &f, head, maxp, glyf, loca) &&
		parsemetrics(b, &f, hmtx, hhea) &&
		parsectable(b, &f, cmap);
	if (!ok || b->error)
		panic("parsing failed");
	return f;
}

Font openttf(const char *path)
{
	IOBuffer b = {};
	if (!bopen(&b, path, 'r'))
		panic("failed to open the b");
	Font f = parsettf(&b);
	bclose(&b);
	return f;
}

U16 findglyph(Font f, U32 code)
{
	if (!f.ctable[0])
		return 0;
	U16 l = 0, r = f.npoints;
	while (l < r) {
		U16 m = l + (r - l)/2;
		if (f.ctable[0][m] == code)
			return f.ctable[1][m];
		if (f.ctable[0][m] < code)
			l = m + 1;
		else
			r = m;
	}
	return 0;
}

/*
 * To get a correct winding count with join points, I used a trick where if a ray hits
 * exactly a segment`s start or end point, we add a "half" of it`s y-direction at that point.
 *
 * For lines the rules are:
 *
 * y            (dy > 0)            (dy < 0)
 * ^          ^                   v
 * |  ray     |                   |
 * | ----->   |  +2     .  +1     |  -2     .  -1   (and horizontals give +0)
 * |          |         |         |         |
 * |          ^         ^         v         v
 * '----------------------------------------------->x
 *
 * This guarantees correct handling of segment joins, e.g.
 *
 *               _   _/  /
 *              / \ /    |
 *
 * For parabolas it's a bit more tricky, but the idea is the same.
 */

static I32 isectline(I16 rx, I16 ry, I16 x1, I16 y1, I16 x2, I16 y2)
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

/* TODO: we could use a "tesselating" approach (just split the curve into a bunch of lines),
 * this way we could try to apply AVX to paralellize the winding number computations. */
static I32 isectcurve(I16 rx, I16 ry, I16 x1, I16 y1, I16 x2, I16 y2, I16 x3, I16 y3)
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
	for (I i = 0; i < 2; i++) {
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

void drawraster(Image *f, I16 x0, I16 y0, Glyph g, Color c, F64 scale, U8 ss)
{
	if (!g.ncont)
		return;
	I16 xmin = g.lim[0][0]*scale - 1, xmax = g.lim[0][1]*scale + 1;
	I16 ymin = g.lim[1][0]*scale - 1, ymax = g.lim[1][1]*scale + 1;
	for (I16 x = CLIPX(f, x0 + xmin); x < CLIPX(f, x0 + xmax + 1); x++)
	for (I16 y = CLIPY(f, y0 - ymax); y < CLIPY(f, y0 - ymin + 1); y++) {
		I32 hits = 0;
		for (I16 dx = 0; dx < ss; dx++)
		for (I16 dy = 0; dy < ss; dy++) {
			I32 wn = 0;
			for (I16 cont = 0, start = 0; cont < g.ncont; cont++) {
				U16 n = g.ends[cont]+1-start;
				for (U16 i = 0; i < n;) {
					U16 curr = start + i, next = start + MOD(i+1, n), last = start + MOD(i+2, n);
					I16 x1 = g.xy[0][curr]*ss*scale, y1 = g.xy[1][curr]*ss*scale;
					I16 x2 = g.xy[0][next]*ss*scale, y2 = g.xy[1][next]*ss*scale;
					I16 x3 = g.xy[0][last]*ss*scale, y3 = g.xy[1][last]*ss*scale;
					if (g.on[next]) {
						wn += isectline((x-x0)*ss + dx, (y0-y)*ss + dy, x1, y1, x2, y2);
						i += 1;
					} else {
						wn += isectcurve((x-x0)*ss + dx, (y0-y)*ss + dy, x1, y1, x2, y2, x3, y3);
						i += 2;
					}
				}
				start += n;
			}
			hits += wn != 0;
		}
		if (hits)
			PIXEL(f, x, y) = RGBA(R(c)*hits/(ss*ss), G(c)*hits/(ss*ss), B(c)*hits/(ss*ss), A(c)*hits/(ss*ss));
	}
}

/* TODO: more stuff for SDF */
void drawsdf(Image *f, I16 x0, I16 y0, Glyph g, F64 scale)
{
	if (!g.ncont)
		return;
	I16 xmin = g.lim[0][0]*scale - 1, xmax = g.lim[0][1]*scale + 1;
	I16 ymin = g.lim[1][0]*scale - 1, ymax = g.lim[1][1]*scale + 1;
	for (I16 x = CLIPX(f, x0 + xmin); x < CLIPX(f, x0 + xmax + 1); x++)
	for (I16 y = CLIPY(f, y0 - ymax); y < CLIPY(f, y0 - ymin + 1); y++) {
		I32 wn = 0;
		F64 d = INF;
		for (I16 cont = 0, start = 0; cont < g.ncont; cont++) {
			U16 n = g.ends[cont]+1-start;
			for (U16 i = 0; i < n;) {
				U16 curr = start + i, next = start + MOD(i+1, n), last = start + MOD(i+2, n);
				I16 x1 = g.xy[0][curr]*scale, y1 = g.xy[1][curr]*scale;
				I16 x2 = g.xy[0][next]*scale, y2 = g.xy[1][next]*scale;
				I16 x3 = g.xy[0][last]*scale, y3 = g.xy[1][last]*scale;
				if (g.on[next]) {
					wn += isectline(x - x0, y0 - y, x1, y1, x2, y2);
					i += 1;
					x3 = x2, y3 = y2;
				} else {
					wn += isectcurve(x - x0, y0 - y, x1, y1, x2, y2, x3, y3);
					i += 2;
				}
				Poly dx = {{(x1 - x + x0), 2*(x2 - x1), x3 + x1 - 2*x2}, 2};
				Poly dy = {{(y1 - y0 + y), 2*(y2 - y1), y3 + y1 - 2*y2}, 2};
				Poly d2 = padd(pmul(dx, dx), pmul(dy, dy));
				Roots r = roots(ddx(d2));
				d = MIN3(d, eval(d2, 0), eval(d2, 1));
				for (U8 i = 0; i < r.n; i++)
					if (r.v[i] > 0 && r.v[i] < 1)
						d = MIN(d, eval(d2, r.v[i]));
			}
			start += n;
		}
		*(F32 *)&PIXEL(f, x, y) = fsetsign(d, wn);
	}
	/* NOTE: this code is for debugging */
	/*
	 * for (I16 x = CLIPX(f, x0 + xmin); x < CLIPX(f, x0 + xmax + 1); x++)
	 * for (I16 y = CLIPY(f, y0 - ymax); y < CLIPY(f, y0 - ymin + 1); y++) {
	 * 	F32 d = *(F32 *)&PIXEL(f, x, y);
	 * 	F32 a = smoothstep(5, 0, ABS(d)) * 255;
	 * 	PIXEL(f, x, y) = RGBA(a, a, a, 255);
	 * }
	 */
}

void drawraster2(Image *f, I16 x0, I16 y0, Glyph g, Color c, F64 scale)
{
	if (!g.ncont)
		return;
	I16 xmin = g.lim[0][0]*scale - 1, xmax = g.lim[0][1]*scale + 1;
	I16 ymin = g.lim[1][0]*scale - 1, ymax = g.lim[1][1]*scale + 1;
	for (I16 x = CLIPX(f, x0 + xmin); x < CLIPX(f, x0 + xmax + 1); x++)
	for (I16 y = CLIPY(f, y0 - ymax); y < CLIPY(f, y0 - ymin + 1); y++)
		PIXEL(f, x, y) = 0;
	for (I16 cont = 0, start = 0; cont < g.ncont; cont++) {
		U16 n = g.ends[cont]+1-start;
		for (U16 i = 0; i < n;) {
			U16 curr = start + i, next = start + MOD(i+1, n), last = start + MOD(i+2, n);
			I16 x1 = g.xy[0][curr]*scale, y1 = g.xy[1][curr]*scale;
			I16 x2 = g.xy[0][next]*scale, y2 = g.xy[1][next]*scale;
			I16 x3 = g.xy[0][last]*scale, y3 = g.xy[1][last]*scale;
			if (g.on[next]) {
				I16 xlo = MIN(x1, x2), ylo = MIN(y1, y2);
				I16 xhi = MAX(x1, x2), yhi = MAX(y1, y2);
				for (I16 y = CLIPY(f, y0 - yhi); y < CLIPY(f, y0 - ylo + 1); y++) {
					I32 left = isectline(xlo, y0 - y, x1, y1, x2, y2);
					for (I16 x = CLIPX(f, x0 + xmin); x < CLIPX(f, x0 + xlo + 1); x++)
						PIXEL(f, x, y) += left;
					for (I16 x = CLIPX(f, x0 + xlo + 1); x < CLIPX(f, x0 + xhi + 1); x++)
						PIXEL(f, x, y) += isectline(x - x0, y0 - y, x1, y1, x2, y2);
				}
				i += 1;
			} else {
				I16 xlo = MIN3(x1, x2, x3), ylo = MIN3(y1, y2, y3);
				I16 xhi = MAX3(x1, x2, x3), yhi = MAX3(y1, y2, y3);
				for (I16 y = CLIPY(f, y0 - yhi); y < CLIPY(f, y0 - ylo + 1); y++) {
					I32 left = isectcurve(xlo, y0 - y, x1, y1, x2, y2, x3, y3);
					for (I16 x = CLIPX(f, x0 + xmin); x < CLIPX(f, x0 + xlo + 1); x++)
						PIXEL(f, x, y) += left;
					for (I16 x = CLIPX(f, x0 + xlo + 1); x < CLIPX(f, x0 + xhi + 1); x++)
						PIXEL(f, x, y) += isectcurve(x - x0, y0 - y, x1, y1, x2, y2, x3, y3);
				}
				i += 2;
			}
		}
		start += n;
	}
	for (I16 x = CLIPX(f, x0 + xmin); x < CLIPX(f, x0 + xmax + 1); x++)
	for (I16 y = CLIPY(f, y0 - ymax); y < CLIPY(f, y0 - ymin + 1); y++)
		if (PIXEL(f, x, y))
			PIXEL(f, x, y) = c;
}

void drawoutline(Image *f, I16 x0, I16 y0, Glyph g, Color c, F64 scale)
{
	for (I16 cont = 0, start = 0; cont < g.ncont; cont++) {
		U16 n = g.ends[cont]+1-start;
		for (U16 i = 0; i < n;) {
			U16 curr = start + i, next = start + MOD(i+1, n), last = start + MOD(i+2, n);
			I16 x1 = g.xy[0][curr]*scale, y1 = g.xy[1][curr]*scale;
			I16 x2 = g.xy[0][next]*scale, y2 = g.xy[1][next]*scale;
			I16 x3 = g.xy[0][last]*scale, y3 = g.xy[1][last]*scale;
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

/* TODO: maybe read the whole font file into memory (or mmap)? */

#define ARRSIZE(a) (sizeof(a)/sizeof((a)[0]))

int main(int argc, char **argv)
{
	if (argc != 2) {
		println("usage: ", argv[0], " FONT.ttf");
		return 1;
	}
	Font fn = openttf(argv[1]);
	winopen(1920, 1080, argv[0], 0);
	U32 s[] = {'$', ' ', 'T', 'e', 's', 't', ' ', '1', '2', '3', '!'};
	Glyph g[ARRSIZE(s)] = {};
	F64 scale = 40.0/fn.upm;
	I16 len = 0;
	for (U64 i = 0; i < ARRSIZE(s); i++) {
		g[i] = fn.glyphs[findglyph(fn, s[i])];
		len += g[i].advance;
	}
	while (!keyisdown('q')) {
		Image *f = frame();
		if (keywaspressed('u'))
			scale *= 1.1;
		if (keywaspressed('d'))
			scale /= 1.1;
		drawclear(f, RGBA(18, 18, 18, 255));
		I16 x = mousex() - len*scale/2, y = mousey();
		for (U32 i = 0; i < ARRSIZE(s); i++) {
			drawraster(f, x, y, g[i], RGBA(255, 255, 255, 200), scale, 3);
			x += g[i].advance*scale;
		}
	}
	winclose();
	arfree(&fn.mem);
	return 0;
}
