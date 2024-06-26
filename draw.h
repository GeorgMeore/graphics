typedef struct {
	u16 w, h;
	Color p[];
} Image;

Image *Img(u16 w, u16 h);
void drawclear(Image *i, Color c);
void drawtriangle(Image *i, int x1, int y1, int x2, int y2, int x3, int y3, Color c);
void drawcircle(Image *i, int xc, int yc, int r, Color c);
void drawrect(Image *i, int xtl, int ytl, int w, int h, Color c);
void drawline(Image *i, int x1, int y1, int x2, int y2, Color c);
