#include "types.h"
#include "color.h"
#include "image.h"
#include "win.h"
#include "mlib.h"
#include "math.h"

#include <immintrin.h>
#include <pthread.h>

typedef struct {
	F64 d;
	F64 vw, vh;
} Camera;

typedef struct {
	F64 x, y, z;
} Vec;

Vec sub(Vec a, Vec b)
{
	Vec c = {a.x - b.x, a.y - b.y, a.z - b.z};
	return c;
}

F64 dot(Vec a, Vec b)
{
	return a.x*b.x + a.y*b.y + a.z*b.z;
}

typedef struct {
	Vec p;
	F64 r;
	Color c;
} Sphere;

typedef struct {
	Vec p1, p2, p3;
	Color c;
} Triangle;

Sphere scene[] = {
	{{0, -1, 3}, 1, RGBA(255, 0, 0, 255)},
	{{2, 0, 4},  1, RGBA(0, 0, 255, 255)},
	{{-2, 0, 4}, 1, RGBA(0, 255, 0, 255)},
};

#define N (sizeof(scene)/sizeof(scene[0]))

F64 testsphere(Sphere s, Vec o, Vec d, F64 zmin)
{
	Vec po = sub(o, s.p);
	F64 a = dot(d, d);
	F64 b = 2*dot(po, d);
	F64 c = dot(po, po) - s.r*s.r;
	F64 D = b*b - 4*a*c;
	F64 z = INF;
	if (D >= 0) {
		F64 t1 = (fsqrt(D)-b) / (2*a), t2 = (-fsqrt(D)-b) / (2*a);
		F64 z1 = o.z + t1*d.z, z2 = o.z + t2*d.z;
		if (t1 >= 0 && z1 >= zmin)
			z = z1;
		if (t2 >= 0 && z2 >= zmin && z2 < z)
			z = z2;
	}
	return z;
}

/*
 o + t*d
 a + u*(b - a) + v*(c - a)
 */
F64 testtriangle(Triangle t, Vec o, Vec d, F64 zmin)
{
}

Color ray(Camera cam, Vec o, Vec d)
{
	Color color = RGBA(18, 18, 18, 255);
	F64 zmin = INF;
	for (U i = 0; i < N; i++) {
		Sphere s = scene[i];
		F64 z = testsphere(s, o, d, cam.d);
		if (z < zmin) {
			color = s.c;
			zmin = z;
		}
	}
	return color;
}

Vec shoot(Camera c, Image *i, U16 x, U16 y)
{
	Vec v = {(x - i->w/2)*c.vw/i->w, (i->h/2 - y)*c.vh/i->h, c.d};
	return v;
}

int main(int, char **argv)
{
	winopen(500, 500, argv[0], 60);
	Camera c = {1, 1, 1};
	Vec origin = {0, 0, 0};
	while (!keyisdown('q')) {
		Image *f = framebegin();
		for (U16 y = 0; y < f->h; y++)
		for (U16 x = 0; x < f->w; x++)
			PIXEL(f, x, y) = ray(c, origin, shoot(c, f, x, y));
		frameend();
	}
	return 0;
}
