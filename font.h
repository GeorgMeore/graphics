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

Font parsettf(IOBuffer *b);
Font openttf(const char *path);

U16 findglyph(Font f, U32 code);

void drawbmp(Image *f, I16 x0, I16 y0, Glyph g, F64 scale);
void drawoutline(Image *f, I16 x0, I16 y0, Glyph g, Color c, F64 scale);
void drawsdf(Image *f, I16 x0, I16 y0, Glyph g, F64 scale);
