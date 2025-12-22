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

typedef enum {
	SegLine,
	SegQuad,
} Segtype;

typedef struct {
	I16 x[3], y[3];
	U8  type;
} Segment;

typedef struct {
	I16     nseg;
	Segment *segs;
	I16     xmin, xmax;
	I16     ymin, ymax;
	U16     advance;
	I16     lsb;
} Glyph;

typedef struct {
	Arena  mem;
	I16    ascend;
	I16    descend;
	I16    linegap;
	U16    upm;
	I16    xmin, xmax;
	I16    ymin, ymax;
	U16    nglyph;
	Glyph  *glyphs;
	U16    npoints;
	U16    *ctable[2];
} Font;

typedef struct {
	I16 ncont;
	U16 *ends;
	U16 nvert;
	I16 *xy[2];
	U8  *on;
} Points;

static void pointstoglyph(Glyph *g, Points *p)
{
	U16 nseg = 0;
	for (I16 cont = 0, start = 0; cont < p->ncont; cont++) {
		U16 n = p->ends[cont] + 1 - start;
		for (U16 i = 0; i < n; i++) {
			U16 curr = start + i;
			U16 prev = start + MOD(i - 1, n);
			U16 next = start + MOD(i + 1, n);
			I16 x1 = p->xy[0][prev], y1 = p->xy[1][prev];
			I16 x2 = p->xy[0][curr], y2 = p->xy[1][curr];
			I16 x3 = p->xy[0][next], y3 = p->xy[1][next];
			if (!p->on[curr]) {
				if (!p->on[prev])
					x1 = (x1 + x2)/2, y1 = (y1 + y2)/2;
				if (!p->on[next])
					x3 = (x3 + x2)/2, y3 = (y3 + y2)/2;
				g->segs[nseg] = (Segment){{x1, x2, x3}, {y1, y2, y3}, SegQuad};
				nseg += 1;
			} else if (p->on[next]) {
				g->segs[nseg] = (Segment){{x2, x3}, {y2, y3}, SegLine};
				nseg += 1;
			}
		}
		start += n;
	}
	g->nseg = nseg;
}

typedef enum {
	OnCurve = 1<<0,
	XShort  = 1<<1,
	YShort  = 1<<2,
	Repeat  = 1<<3,
	XFlag   = 1<<4,
	YFlag   = 1<<5,
} SimpFlag;

static OK parsesimpleglyph(IOBuffer *b, Points *p, I16 ncont, U16 maxconts, U16 maxpts)
{
	if (p->ncont + ncont > maxconts)
		return 0;
	U16 nvert = 0;
	for (I16 i = 0; i < ncont; i++) {
		U16 idx = readbe(b, 2);
		p->ends[p->ncont+i] = p->nvert + idx;
		if (nvert > idx)
			return 0;
		nvert = idx + 1;
	}
	if (p->nvert + nvert > maxpts)
		return 0;
	U16 ilen = readbe(b, 2);
	skip(b, ilen); /* instructions */
	for (I16 i = 0; i < nvert; i++) {
		p->on[p->nvert+i] = readbe(b, 1);
		if (p->on[p->nvert+i] & Repeat) {
			U8 count = readbe(b, 1);
			if (count >= nvert - i)
				return 0;
			for (; count; count--, i++)
				p->on[p->nvert+i+1] = p->on[p->nvert+i];
		}
	}
	U8 cflags[2][2] = {{XShort, XFlag}, {YShort, YFlag}};
	for (I16 comp = 0; comp < 2; comp++) {
		for (I16 i = 0, prev = 0; i < nvert; i++) {
			if (p->on[p->nvert+i] & cflags[comp][0]) {
				if (p->on[p->nvert+i] & cflags[comp][1])
					p->xy[comp][p->nvert+i] = prev + readbe(b, 1);
				else
					p->xy[comp][p->nvert+i] = prev - readbe(b, 1);
			} else {
				if (p->on[p->nvert+i] & cflags[comp][1])
					p->xy[comp][p->nvert+i] = prev;
				else
					p->xy[comp][p->nvert+i] = prev + readbe(b, 2);
			}
			prev = p->xy[comp][p->nvert+i];
		}
	}
	for (I16 i = 0; i < nvert; i++)
		p->on[p->nvert+i] &= OnCurve;
	p->nvert += nvert;
	p->ncont += ncont;
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

static OK parsecompoundglyph(IOBuffer *b, Points *p, U32 glyf, U32 *locations, U16 maxconts, U16 maxpts)
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
		U16 start = p->nvert;
		if (locations[index] == locations[index+1])
			continue;
		bseek(b, glyf + locations[index]);
		I16 ncont = readbe(b, 2);
		skip(b, 2+2+2+2); /* xMin, yMin, xMax, yMax */
		OK ok;
		if (ncont > 0)
			ok = parsesimpleglyph(b, p, ncont, maxconts, maxpts);
		if (ncont < 0)
			ok = parsecompoundglyph(b, p, glyf, locations, maxconts, maxpts);
		if (!ok)
			return 0;
		for (U16 i = start; i < p->nvert; i++) {
			p->xy[0][i] += x;
			p->xy[1][i] += y;
		}
		bseek(b, curr);
		if (~flags & MoreComp)
			break;
	}
	return 1;
}

static OK parseglyph(IOBuffer *b, Font *f, Points *p, U16 index, U32 glyf, U32 *locations, U16 maxconts, U16 maxpts)
{
	if (index+1 < f->nglyph && locations[index] == locations[index+1])
		return 1;
	bseek(b, glyf + locations[index]);
	I16 ncont = readbe(b, 2);
	if (ncont > maxconts)
		return 0;
	if (ncont == 0)
		return 1;
	Glyph *g = &f->glyphs[index];
	g->xmin = readbe(b, 2);
	g->ymin = readbe(b, 2);
	g->xmax = readbe(b, 2);
	g->ymax = readbe(b, 2);
	OK ok = g->xmin < g->xmax && g->ymin < g->ymax &&
		g->xmin >= f->xmin && g->xmax <= f->xmax &&
		g->ymin >= f->ymin && g->ymax <= f->ymax;
	if (!ok)
		return 0;
	p->nvert = p->ncont = 0;
	if (ncont > 0)
		ok = parsesimpleglyph(b, p, ncont, maxconts, maxpts);
	else
		ok = parsecompoundglyph(b, p, glyf, locations, maxconts, maxpts);
	if (!ok)
		return 0;
	for (U16 i = 0; i < p->nvert; i++) {
		if (p->xy[0][i] < g->xmin || p->xy[0][i] > g->xmax)
			return 0;
		if (p->xy[1][i] < g->ymin || p->xy[1][i] > g->ymax)
			return 0;
	}
	pointstoglyph(g, p);
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
	f->xmin = readbe(b, 2);
	f->ymin = readbe(b, 2);
	f->xmax = readbe(b, 2);
	f->ymax = readbe(b, 2);
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
	/* NOTE: a contour of n points consists of no more than n+1 segments */
	U16 maxsegs = maxpts + maxconts;
	for (U16 i = 0; i < f->nglyph; i++)
		f->glyphs[i].segs = aralloc(&f->mem, maxsegs * sizeof(Segment));
	Points p = {
		.on    = aralloc(&f->mem, maxpts),
		.ends  = aralloc(&f->mem, maxconts * sizeof(U16)),
		.xy[0] = aralloc(&f->mem, maxpts * sizeof(I16)),
		.xy[1] = aralloc(&f->mem, maxpts * sizeof(I16)),
	};
	for (U16 i = 0; i < f->nglyph; i++)
		parseglyph(b, f, &p, i, glyf, locations, maxconts, maxpts);
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

/* TODO: maybe read the whole font file into memory (or mmap)? */
/* NOTE: Checking every read/seek operation for errors would be tedious,
 * so there is just one check for io errors when all of the parsing is done.
 *
 * Together with some basic validation it gives "good enough" error resilience IMO. */
Font parsettf(IOBuffer *b)
{
	Font f = {0};
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
	if (!ok || b->error) {
		arfree(&f.mem);
		f = (Font){0};
	}
	return f;
}

Font openttf(const char *path)
{
	IOBuffer b = {0};
	if (!bopen(&b, path, 'r'))
		panic("failed to open the b");
	Font f = parsettf(&b);
	bclose(&b);
	return f;
}

U16 findglyph(Font f, U64 code)
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
 * This guarantees correct handling of segment joins (no false intersections
 * on peaks and no double-counting), e.g.
 *
 *                             _   _/  /
 *                            / \ /    |
 *
 * For parabolas it's a bit more tricky, but the idea is the same.
 */

typedef struct {
	F64 x[2];
	I8  wn[2];
	U8  n;
} Hit;

static Hit isectline(I16 ry, I16 x1, I16 y1, I16 x2, I16 y2)
{
	Hit h = {0};
	if (y1 == y2)
		return h;
	h.n = 1;
	if (ry == y1) {
		h.x[0] = x1, h.wn[0] = SIGN(y2 - y1);
	} else if (ry == y2) {
		h.x[0] = x2, h.wn[0] = SIGN(y2 - y1);
	} else {
		h.x[0] = (x1*(y2 - y1) + (ry - y1)*(x2 - x1))/(F64)(y2 - y1);
		h.wn[0] = 2*SIGN(y2 - y1);
	}
	return h;
}

static Hit isectcurve(I16 ry, I16 x1, I16 y1, I16 x2, I16 y2, I16 x3, I16 y3)
{
	F64 a = y1 - 2*y2 + y3;
	F64 b = 2*(y2 - y1);
	F64 c = y1 - ry;
	Poly x = {{x1, 2*(x2 - x1), x1 - 2*x2 + x3}, 2};
	Hit h = {0};
	if (a == 0) {
		F64 t = -c/b;
		if (t >= 0 && t <= 1) {
			h.n = 1;
			h.x[0] = eval(x, t);
			if (t == 0 || t == 1)
				h.wn[0] = SIGN(y2 - y1);
			else
				h.wn[0] = 2*SIGN(y2 - y1);
		}
		return h;
	}
	I64 d = b*b - 4*a*c;
	if (d < 0)
		return h;
	if (d == 0) {
		F64 t = -b/(2*a);
		if (t == 0 || t == 1) {
			h.n = 1;
			h.x[0] = eval(x, t);
			h.wn[0] = SIGN(y3 - y1);
		}
		return h;
	}
	F64 t[2] = {(-b - fsqrt(d))/(2*a), (-b + fsqrt(d))/(2*a)};
	for (I j = 0; j < 2; j++) {
		if (t[j] >= 0 && t[j] <= 1) {
			h.x[h.n] = eval(x, t[j]);
			if (t[j] == 0)
				h.wn[h.n] = SIGN(y2 - y1);
			else if (t[j] == 1)
				h.wn[h.n] = SIGN(y3 - y2);
			else if (t[j] < -b/(2*a))
				h.wn[h.n] = 2*SIGN(y2 - y1);
			else
				h.wn[h.n] = 2*SIGN(y3 - y2);
			h.n += 1;
		}
	}
	return h;
}

void drawbmp(Image *f, I16 x0, I16 y0, Glyph g, F64 scale)
{
	if (!g.nseg)
		return;
	I16 xmin = g.xmin*scale - 1, xmax = g.xmax*scale + 1;
	I16 ymin = g.ymin*scale - 1, ymax = g.ymax*scale + 1;
	for (I16 y = CLIPY(f, y0 - ymax); y < CLIPY(f, y0 - ymin + 1); y++)
	for (I16 x = CLIPX(f, x0 + xmin); x < CLIPX(f, x0 + xmax + 1); x++)
		PIXEL(f, x, y) = 0;
	for (U16 i = 0; i < g.nseg; i++) {
		Segment s = g.segs[i];
		I16 x1 = s.x[0]*scale, y1 = s.y[0]*scale;
		I16 x2 = s.x[1]*scale, y2 = s.y[1]*scale;
		I16 x3 = s.x[2]*scale, y3 = s.y[2]*scale;
		I16 ylo, yhi;
		if (s.type == SegQuad)
			ylo = MIN3(y1, y2, y3), yhi = MAX3(y1, y2, y3);
		else
			ylo = MIN(y1, y2), yhi = MAX(y1, y2);
		for (I16 y = CLIPY(f, y0 - yhi); y < CLIPY(f, y0 - ylo + 1); y++) {
			Hit h;
			if (s.type == SegQuad)
				h = isectcurve(y0 - y, x1, y1, x2, y2, x3, y3);
			else
				h = isectline(y0 - y, x1, y1, x2, y2);
			for (I16 j = 0; j < h.n; j++) {
				I16 x = CLIPX(f, x0 + h.x[j] + 1) - 1;
				if (x >= 0)
					PIXEL(f, x, y) += h.wn[j];
			}
		}
	}
	for (I16 y = CLIPY(f, y0 - ymax); y < CLIPY(f, y0 - ymin + 1); y++) {
		I32 wn = 0;
		for (I16 x = CLIPX(f, x0 + xmin); x < CLIPX(f, x0 + xmax + 1); x++)
			wn += PIXEL(f, x, y);
		for (I16 x = CLIPX(f, x0 + xmin); x < CLIPX(f, x0 + xmax + 1); x++) {
			I32 dwn = PIXEL(f, x, y);
			PIXEL(f, x, y) = BOOL(wn);
			wn -= dwn;
		}
	}
}

static F64 distline(F64 x, F64 y, F64 x1, F64 y1, F64 x2, F64 y2)
{
	F64 dx = x2 - x1, dy = y2 - y1;
	F64 wx = x - x1,  wy = y - y1;
	F64 ux = x - x2,  uy = y - y2;
	F64 l2 = dx*dx + dy*dy, t = wx*dx + wy*dy;
	if (t < 0)
		return wx*wx + wy*wy;
	if (t >= l2)
		return ux*ux + uy*uy;
	F64 n = wx*dy - wy*dx;
	return n*n / l2;
}

static F64 disttriangle(F64 x, F64 y, F64 x1, F64 y1, F64 x2, F64 y2, F64 x3, F64 y3)
{
	F64 o1 = (x - x1)*(y2 - y1) - (y - y1)*(x2 - x1);
	F64 o2 = (x - x2)*(y3 - y2) - (y - y2)*(x3 - x2);
	F64 o3 = (x - x3)*(y1 - y3) - (y - y3)*(x1 - x3);
	if (SIGN(o1) == SIGN(o2) && SIGN(o2) == SIGN(o3))
		return 0;
	return MIN3(
		distline(x, y, x1, y1, x2, y2),
		distline(x, y, x2, y2, x3, y3),
		distline(x, y, x3, y3, x1, y1)
	);
}

static F64 distcurve(F64 x, F64 y, F64 x1, F64 y1, F64 x2, F64 y2, F64 x3, F64 y3, F64 dmin)
{
	/* NOTE: if the current minimal distance is less than the distance
	 * to the bouding triangle, we can safely skip this curve */
	if (dmin < disttriangle(x, y, x1, y1, x2, y2, x3, y3))
		return dmin;
	/* NOTE: search for extremums of the square distance function */
	Poly dx = {{(x1 - x), 2*(x2 - x1), x3 + x1 - 2*x2}, 2};
	Poly dy = {{(y1 - y), 2*(y2 - y1), y3 + y1 - 2*y2}, 2};
	Poly d2 = padd(pmul(dx, dx), pmul(dy, dy));
	Roots r = roots(ddx(d2));
	F64 d = MIN(eval(d2, 0), eval(d2, 1));
	for (U8 i = 0; i < r.n; i++)
		if (r.v[i] > 0 && r.v[i] < 1)
			d = MIN(d, eval(d2, r.v[i]));
	return d;
}

void drawsdf(Image *f, I16 x0, I16 y0, Glyph g, F64 scale)
{
	if (!g.nseg)
		return;
	drawbmp(f, x0, y0, g, scale); /* calculate signs */
	I16 xmin = g.xmin*scale - 1, xmax = g.xmax*scale + 1;
	I16 ymin = g.ymin*scale - 1, ymax = g.ymax*scale + 1;
	for (I16 x = CLIPX(f, x0 + xmin); x < CLIPX(f, x0 + xmax + 1); x++)
	for (I16 y = CLIPY(f, y0 - ymax); y < CLIPY(f, y0 - ymin + 1); y++) {
		F64 d = INF;
		for (I i = 0; i < g.nseg; i++) {
			Segment s = g.segs[i];
			I16 x1 = s.x[0]*scale, y1 = s.y[0]*scale;
			I16 x2 = s.x[1]*scale, y2 = s.y[1]*scale;
			I16 x3 = s.x[2]*scale, y3 = s.y[2]*scale;
			if (s.type == SegQuad)
				d = MIN(d, distcurve(x - x0, y0 - y, x1, y1, x2, y2, x3, y3, d));
			else
				d = MIN(d, distline(x - x0, y0 - y, x1, y1, x2, y2));
		}
		*(F32 *)&PIXEL(f, x, y) = fsetsign(d, PIXEL(f, x, y));
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

void drawoutline(Image *f, I16 x0, I16 y0, Glyph g, Color c, F64 scale)
{
	for (I i = 0; i < g.nseg; i++) {
		Segment s = g.segs[i];
		I16 x1 = s.x[0]*scale, y1 = s.y[0]*scale;
		I16 x2 = s.x[1]*scale, y2 = s.y[1]*scale;
		I16 x3 = s.x[2]*scale, y3 = s.y[2]*scale;
		if (s.type == SegQuad)
			drawbezier(f, x0+x1, y0-y1, x0+x2, y0-y2, x0+x3, y0-y3, c);
		else
			drawline(f, x0+x1, y0-y1, x0+x2, y0-y2, c);
	}
}

OK utf8(char **s, U64 *c)
{
	U8 mask = 0b10000000, n = 0;
	while (**s & mask) {
		n += 1;
		mask >>= 1;
	}
	*c = **s & ((mask << 1) - 1);
	for (*s += 1; n > 1; n--) {
		if ((**s & 0b11000000) != 0b10000000)
			return 0;
		*c = (*c << 6) | (**s & 0b00111111);
		*s += 1;
	}
	return 1;
}

#define CACHESIZE 128

typedef struct {
	Image bmp;
	I16 xoff, yoff, advance;
} GBitmap;

/* NOTE: this is a basic LRU cache */
typedef struct {
	Arena   mem;
	F64     px;
	GBitmap bmps[CACHESIZE];
	Image   tmp;
	U64     idxs[CACHESIZE];
	U16     next[CACHESIZE];
	U16     n, last;
} GCache;

void addglyph(GCache *c, Font fn, GBitmap *b, U16 idx, OK create)
{
	c->idxs[c->last] = idx;
	F64 scale = c->px/fn.upm;
	F64 w = (fn.xmax - fn.xmin)*scale + 2;
	F64 h = (fn.ymax - fn.ymin)*scale + 2;
	if (create) {
		b->bmp.w = w;
		b->bmp.s = w;
		b->bmp.h = h;
		b->bmp.p = aralloc(&c->mem, w*h * sizeof(Color));
	}
	I8 ss = 4;
	if (c->n == 1) {
		c->tmp.w = w*ss;
		c->tmp.s = w*ss;
		c->tmp.h = h*ss;
		c->tmp.p = aralloc(&c->mem, (ss*ss)*w*h * sizeof(Color));
	}
	if (c->px > 100)
		ss = 2;
	Glyph g = fn.glyphs[idx];
	b->xoff = g.xmin * scale;
	b->yoff = 1 - (h + g.ymin * scale);
	b->advance = g.advance * scale;
	drawclear(&c->tmp, 0);
	drawbmp(&c->tmp, -b->xoff*ss, -b->yoff*ss, g, scale*ss);
	for (I y = 0; y < h; y++)
	for (I x = 0; x < w; x++) {
		I n = 0;
		for (I dy = 0; dy < ss; dy++)
		for (I dx = 0; dx < ss; dx++)
			n += PIXEL(&c->tmp, x*ss + dx, y*ss + dy);
		*(F32 *)&PIXEL(&b->bmp, x, y) = n / (F64)(ss*ss);
	}
}

GBitmap *lookup(Font fn, GCache *c, U64 idx)
{
	U16 *pp = &c->last;
	OK found = 0, create = 0;
	for (U16 i = 0; i < c->n; i++) {
		if (c->idxs[*pp] == idx) {
			found = 1;
			break;
		}
		pp = &c->next[*pp];
	}
	if (!found && c->n < CACHESIZE) {
		c->next[c->n] = c->last;
		c->last = c->n;
		c->n += 1;
		create = 1;
	} else if (*pp != c->last) {
		U16 p = *pp;
		*pp = c->next[p];
		c->next[p] = c->last;
		c->last = p;
	}
	GBitmap *b = &c->bmps[c->last];
	if (!found)
		addglyph(c, fn, b, idx, create);
	return b;
}

void clear(GCache *c)
{
	arclear(&c->mem);
	c->n = 0;
}

void drawgbitmap(Image *f, I16 x0, I16 y0, Image bmp, Color c)
{
	for (I16 y = CLIPY(f, y0); y < CLIPY(f, y0 + bmp.h); y++)
	for (I16 x = CLIPX(f, x0); x < CLIPX(f, x0 + bmp.w); x++) {
		if (PIXEL(&bmp, x - x0, y - y0)) {
			U8 a = *(F32 *)&PIXEL(&bmp, x - x0, y - y0) * 255.0;
			PIXEL(f, x, y) = blend(PIXEL(f, x, y), RGBA(R(c), G(c), B(c), a));
		}
	}
}

I16 drawchar(Image *f, I16 x, I16 y, Font fn, GCache *gc, U64 code, Color c)
{
	U16 idx = findglyph(fn, code);
	GBitmap *b = lookup(fn, gc, idx);
	drawgbitmap(f, x+b->xoff, y+b->yoff, b->bmp, c);
	return b->advance;
}

#define MAXRUNES 4096

int main(int argc, char **argv)
{
	if (argc != 3) {
		println("usage: ", argv[0], " FONT.ttf STRING");
		return 1;
	}
	Font fn = openttf(argv[1]);
	U64 n, r[MAXRUNES];
	for (n = 0; *argv[2]; n++) {
		if (!utf8(&argv[2], &r[n]))
			panic("invalid UTF8 string");
		if (n >= MAXRUNES)
			panic("string too long");
	}
	GCache c = {.px = 17};
	winopen(1920, 1080, argv[0], 0);
	while (!keyisdown('q')) {
		Image *f = frame();
		if (btnwaspressed(4)) {
			c.px *= 1.1;
			clear(&c);
		}
		if (btnwaspressed(5)) {
			c.px /= 1.1;
			clear(&c);
		}
		drawclear(f, RGBA(18, 18, 18, 255));
		I16 x = mousex(), y = mousey();
		for (U32 i = 0; i < n; i++)
			x += drawchar(f, x, y, fn, &c, r[i], RGBA(255, 255, 255, 200));
	}
	winclose();
	arfree(&fn.mem);
	return 0;
}
