#include "types.h"
#include "color.h"
#include "image.h"
#include "io.h"
#include "font.h"
#include "alloc.h"
#include "math.h"
#include "fontparse.h"

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
	I16 (*xy)[2];
	U8  *on;
} Points;

static void pointstoglyph(Glyph *g, Points *p)
{
	U16 nseg = 0;
	for (I16 cont = 0, start = 0; cont < p->ncont; cont++) {
		U16 n = p->ends[cont] + 1 - start;
		for (U16 i = 0; i < n; i++) {
			U16 curr = start + i;
			U16 prev = start + imod((I32)i - 1, n);
			U16 next = start + imod((I32)i + 1, n);
			I16 x1 = p->xy[prev][0], y1 = p->xy[prev][1];
			I16 x2 = p->xy[curr][0], y2 = p->xy[curr][1];
			I16 x3 = p->xy[next][0], y3 = p->xy[next][1];
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
	U16 *ends = &p->ends[p->ncont];
	for (I16 i = 0; i < ncont; i++) {
		U16 idx = readbe(b, 2);
		ends[i] = p->nvert + idx;
		if (nvert > idx)
			return 0;
		nvert = idx + 1;
	}
	if (p->nvert + nvert > maxpts)
		return 0;
	U16 ilen = readbe(b, 2);
	skip(b, ilen); /* instructions */
	U8 *on = &p->on[p->nvert];
	for (I16 i = 0; i < nvert; i++) {
		on[i] = readbe(b, 1);
		if (on[i] & Repeat) {
			U8 count = readbe(b, 1);
			if (count >= nvert - i)
				return 0;
			for (; count; count--, i++)
				on[i+1] = on[i];
		}
	}
	I16 (*xy)[2] = &p->xy[p->nvert];
	for (I16 comp = 0; comp < 2; comp++) {
		for (I16 i = 0, prev = 0; i < nvert; i++) {
			if (on[i] & (XShort << comp)) {
				if (on[i] & (XFlag << comp))
					xy[i][comp] = prev + readbe(b, 1);
				else
					xy[i][comp] = prev - readbe(b, 1);
			} else {
				if (on[i] & (XFlag << comp))
					xy[i][comp] = prev;
				else
					xy[i][comp] = prev + readbe(b, 2);
			}
			prev = xy[i][comp];
		}
	}
	for (I16 i = 0; i < nvert; i++)
		on[i] &= OnCurve;
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
			p->xy[i][0] += x;
			p->xy[i][1] += y;
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
		if (p->xy[i][0] < g->xmin || p->xy[i][0] > g->xmax)
			return 0;
		if (p->xy[i][1] < g->ymin || p->xy[i][1] > g->ymax)
			return 0;
	}
	pointstoglyph(g, p);
	return 1;
}

static OK parseglyphs(IOBuffer *b, Arena *a, Font *f, U32 head, U32 maxp, U32 glyf, U32 loca)
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
	U32 *locations = aralloc(a, f->nglyph * sizeof(U32));
	for (U16 i = 0; i < f->nglyph; i++) {
		U32 offset = readbe(b, locasize);
		locations[i] = offset*locascale;
	}
	f->glyphs = aralloc(a, f->nglyph * sizeof(Glyph));
	/* NOTE: a contour of n points consists of no more than n+1 segments */
	U16 maxsegs = maxpts + maxconts;
	for (U16 i = 0; i < f->nglyph; i++)
		f->glyphs[i].segs = aralloc(a, maxsegs * sizeof(Segment));
	Points p = {
		.on    = aralloc(a, maxpts),
		.ends  = aralloc(a, maxconts * sizeof(U16)),
		.xy    = aralloc(a, maxpts * sizeof(I16) * 2),
	};
	for (U16 i = 0; i < f->nglyph; i++) {
		OK ok = parseglyph(b, f, &p, i, glyf, locations, maxconts, maxpts);
		if (!ok)
			return 0;
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

static OK parsefmt14(IOBuffer *b, Arena *a, Font *f)
{
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
	f->ctable[0] = aralloc(a, npoints*sizeof(U16));
	f->ctable[1] = aralloc(a, npoints*sizeof(U16));
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

/* TODO: support format 12 tables */
static OK parsectable(IOBuffer *b, Arena *a, Font *f, U32 cmap)
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
		if (format == 4)
			return parsefmt14(b, a, f);
	}
	return 0;
}

/* TODO: maybe read the whole font file into memory (or mmap)? */
/* NOTE: Checking every read/seek operation for errors would be tedious,
 * so there is just one check for io errors when all of the parsing is done.
 *
 * Together with some basic validation it gives "good enough" error resilience IMO. */
Font parsettf(IOBuffer *b, Arena *a)
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
		return (Font){0};
	OK ok = parseglyphs(b, a, &f, head, maxp, glyf, loca) &&
		parsemetrics(b, &f, hmtx, hhea) &&
		parsectable(b, a, &f, cmap);
	if (!ok || b->error)
		return (Font){0};
	return f;
}

/* TODO: move all file format parsing into separate modules */
Font openttf(const char *path, Arena *a)
{
	IOBuffer b = {0};
	if (!bopen(&b, path, 'r'))
		return (Font){0};
	Font f = parsettf(&b, a);
	bclose(&b);
	return f;
}
