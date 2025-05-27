#include "types.h"
#include "color.h"
#include "image.h"
#include "draw.h"
#include "win.h"
#include "math.h"
#include "alloc.h"
#include "mlib.h"

#define BGCOLOR RGBA(18, 18, 18, 255)

typedef struct {
	F64 x, y, z;
} Vert;

Vert transform(Vert p, F64 ya, F64 zo)
{
	return (Vert){p.x*fcos(ya) - p.z*fsin(ya), p.y, p.x*fsin(ya) + p.z*fcos(ya) + zo};
}

Vert project(Image *i, F64 d, Vert p)
{
	return (Vert){i->w/2*(1 + p.x*d/p.z), i->h/2*(1 - p.y*d/p.z), 1/p.z};
}

void draw3dline(Image *i, Vert a, Vert b, Color c)
{
	Vert ap = project(i, 2, a), bp = project(i, 2, b);
	drawline(i, ap.x, ap.y, bp.x, bp.y, c);
}

void draw3dtriangle(Image *i, Vert p1, Vert p2, Vert p3, F64 *zbuff, Color c)
{
	Vert p1p = project(i, 2, p1);
	Vert p2p = project(i, 2, p2);
	Vert p3p = project(i, 2, p3);
	I64 x1 = p1p.x, x2 = p2p.x, x3 = p3p.x;
	I64 y1 = p1p.y, y2 = p2p.y, y3 = p3p.y;
	I64 xmin = CLIPX(i, MIN3(x1, x2, x3)), xmax = CLIPX(i, MAX3(x1, x2, x3));
	I64 ymin = CLIPY(i, MIN3(y1, y2, y3)), ymax = CLIPY(i, MAX3(y1, y2, y3));
	if ((y3 - y1)*(x2 - x1) - (x3 - x1)*(y2 - y1) < 0) {
		SWAP(p2, p3);
		SWAP(x2, x3);
		SWAP(y2, y3);
	}
	I64 os = (x1 - x3)*(y2 - y3) - (x2 - x3)*(y1 - y3);
	if (!os)
		return;
	I64 o1 = (ymin - y1)*(x2 - x1) - (xmin - x1)*(y2 - y1);
	I64 o2 = (ymin - y2)*(x3 - x2) - (xmin - x2)*(y3 - y2);
	I64 o3 = (ymin - y3)*(x1 - x3) - (xmin - x3)*(y1 - y3);
	for (I64 y = ymin; y <= ymax; y++) {
		for (I64 x = xmin; x <= xmax; x++) {
			F64 zi = (o2/p1.z + o3/p2.z + o1/p3.z)/os;
			if (o1 >= 0 && o2 >= 0 && o3 >= 0 && zi > zbuff[x + y*i->w]) {
				zbuff[x + y*i->w] = zi;
				PIXEL(i, x, y) = c;
			}
			o1 -= y2 - y1; o2 -= y3 - y2; o3 -= y1 - y3;
		}
		o1 += (y2 - y1)*(xmax - xmin + 1) + (x2 - x1);
		o2 += (y3 - y2)*(xmax - xmin + 1) + (x3 - x2);
		o3 += (y1 - y3)*(xmax - xmin + 1) + (x1 - x3);
	}
}

void draw3dtriangletexture(Image *i, Vert p1, Vert p2, Vert p3, F64 *zbuff, Image *t, U16 tx1, U16 ty1, U16 tx2, U16 ty2, U16 tx3, U16 ty3)
{
	Vert p1p = project(i, 2, p1);
	Vert p2p = project(i, 2, p2);
	Vert p3p = project(i, 2, p3);
	I64 x1 = p1p.x, x2 = p2p.x, x3 = p3p.x;
	I64 y1 = p1p.y, y2 = p2p.y, y3 = p3p.y;
	I64 xmin = CLIPX(i, MIN3(x1, x2, x3)), xmax = CLIPX(i, MAX3(x1, x2, x3));
	I64 ymin = CLIPY(i, MIN3(y1, y2, y3)), ymax = CLIPY(i, MAX3(y1, y2, y3));
	if ((y3 - y1)*(x2 - x1) - (x3 - x1)*(y2 - y1) < 0) {
		SWAP(p2p, p3p);
		SWAP(x2, x3);
		SWAP(y2, y3);
		SWAP(tx2, tx3);
		SWAP(ty2, ty3);
	}
	I64 os = (x1 - x3)*(y2 - y3) - (x2 - x3)*(y1 - y3);
	if (!os)
		return;
	I64 o1 = (ymin - y1)*(x2 - x1) - (xmin - x1)*(y2 - y1);
	I64 o2 = (ymin - y2)*(x3 - x2) - (xmin - x2)*(y3 - y2);
	I64 o3 = (ymin - y3)*(x1 - x3) - (xmin - x3)*(y1 - y3);
	for (I64 y = ymin; y <= ymax; y++) {
		for (I64 x = xmin; x <= xmax; x++) {
			F64 zi = (o2*p1p.z + o3*p2p.z + o1*p3p.z)/os;
			if (o1 >= 0 && o2 >= 0 && o3 >= 0 && zi > zbuff[x + y*i->w]) {
				zbuff[x + y*i->w] = zi;
				I64 tx = (tx1*p1p.z*o2 + tx2*p2p.z*o3 + tx3*p3p.z*o1)/os/zi;
				I64 ty = (ty1*p1p.z*o2 + ty2*p2p.z*o3 + ty3*p3p.z*o1)/os/zi;
				PIXEL(i, x, y) = PIXEL(t, tx, ty);
			}
			o1 -= y2 - y1; o2 -= y3 - y2; o3 -= y1 - y3;
		}
		o1 += (y2 - y1)*(xmax - xmin + 1) + (x2 - x1);
		o2 += (y3 - y2)*(xmax - xmin + 1) + (x3 - x2);
		o3 += (y1 - y3)*(xmax - xmin + 1) + (x1 - x3);
	}
}

#define WIDTH  3200
#define HEIGHT 2000

int main(int, char **argv)
{
	Image img = loadppm("assets/pepe.ppm");
	if (!img.p)
		return 1;
	winopen(800, 600, argv[0], 60);
	F64 t = 0, zoff = 5;
	F64 *zbuff = memalloc(WIDTH*HEIGHT*8);
	Vert points[8] = {
		{1,  -1, -1},
		{1,   1, -1},
		{-1,  1, -1},
		{-1, -1, -1},
		{1,  -1,  1},
		{1,   1,  1},
		{-1,  1,  1},
		{-1, -1,  1},
	};
	while (!keyisdown('q')) {
		Image *f = framebegin();
		for (I64 i = 0; i < WIDTH*HEIGHT; i++)
			zbuff[i] = 0;
		if (keyisdown(' '))
			t += lastframetime()/1e9;
		if (keyisdown('w'))
			zoff -= 0.05;
		if (keyisdown('s'))
			zoff += 0.05;
		Vert p[8];
		for (int i = 0; i < 8; i++)
			p[i] = transform(points[i], t, zoff);
		drawclear(f, BGCOLOR);
		for (int i = 0; i < 4; i++) {
			draw3dline(f, p[i], p[(i+1)%4], RED);
			draw3dline(f, p[i], p[4+i], RED);
			draw3dline(f, p[4+i], p[4+(i+1)%4], RED);
		}
		draw3dtriangletexture(f, p[0], p[1], p[2], zbuff, &img, img.w-1, img.h-1, img.w-1, 0, 0, 0);
		draw3dtriangletexture(f, p[2], p[3], p[0], zbuff, &img, 0, 0, 0, img.h-1, img.w-1, img.h-1);
		for (int i = 0; i < 8; i++)
			p[i] = transform(points[i], t+PI/2, zoff);
		draw3dtriangletexture(f, p[0], p[1], p[2], zbuff, &img, img.w-1, img.h-1, img.w-1, 0, 0, 0);
		draw3dtriangletexture(f, p[2], p[3], p[0], zbuff, &img, 0, 0, 0, img.h-1, img.w-1, img.h-1);
		for (int i = 0; i < 8; i++)
			p[i] = transform(points[i], t+PI, zoff);
		draw3dtriangletexture(f, p[0], p[1], p[2], zbuff, &img, img.w-1, img.h-1, img.w-1, 0, 0, 0);
		draw3dtriangletexture(f, p[2], p[3], p[0], zbuff, &img, 0, 0, 0, img.h-1, img.w-1, img.h-1);
		for (int i = 0; i < 8; i++)
			p[i] = transform(points[i], t+3*PI/2, zoff);
		draw3dtriangletexture(f, p[0], p[1], p[2], zbuff, &img, img.w-1, img.h-1, img.w-1, 0, 0, 0);
		draw3dtriangletexture(f, p[2], p[3], p[0], zbuff, &img, 0, 0, 0, img.h-1, img.w-1, img.h-1);
		frameend();
	}
	winclose();
	return 0;
}
