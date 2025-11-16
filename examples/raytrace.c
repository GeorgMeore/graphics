#include "types.h"
#include "color.h"
#include "image.h"
#include "win.h"
#include "mlib.h"
#include "math.h"

typedef struct {
	F64 d;
	F64 vw, vh;
} Camera;

typedef struct {
	F64 x, y, z;
} Vec;

Vec sub(Vec a, Vec b)
{
	return (Vec){a.x - b.x, a.y - b.y, a.z - b.z};
}

Vec add(Vec a, Vec b)
{
	return (Vec){a.x + b.x, a.y + b.y, a.z + b.z};
}

Vec mul(Vec a, F64 c)
{
	return (Vec){a.x*c, a.y*c, a.z*c};
}

Vec cross(Vec a, Vec b)
{
	return (Vec){a.y*b.z - b.y*a.z, a.z*b.x - b.z*a.x, a.x*b.y - b.x*a.y};
}

F64 dot(Vec a, Vec b)
{
	return a.x*b.x + a.y*b.y + a.z*b.z;
}

OK eq(Vec a, Vec b)
{
	return a.x == b.x && a.y == b.y && a.z == b.z;
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

Sphere spheres[] = {
	{{0, -1, 3}, 0.5, RED},
	{{2, 0, 4},  0.5, BLUE},
	{{-2, 0, 4}, 0.5, GREEN},
	{}
};

Triangle triangles[] = {
	{{0, -1, 3}, {2, 0, 4}, {-2, 0, 4}, YELLOW},
	{}
};

F64 testsphere(Sphere s, Vec o, Vec d)
{
	Vec po = sub(o, s.p);
	F64 a = dot(d, d);
	F64 b = 2*dot(po, d);
	F64 c = dot(po, po) - s.r*s.r;
	F64 D = b*b - 4*a*c;
	F64 t = INF;
	if (D >= 0) {
		F64 t1 = (fsqrt(D)-b) / (2*a), t2 = (-fsqrt(D)-b) / (2*a);
		if (t1 > 0)
			t = t1;
		if (t2 > 0 && t2 < t)
			t = t2;
	}
	return t;
}

OK gauss(F64 s[3][4], Vec *o)
{
	I m[3] = {0, 1, 2};
	for (I i = 0; i < 3; i++) {
		for (I j = i; j < 3; j++)
			if (ABS(s[m[j]][i]) > ABS(s[m[i]][i]))
				SWAP(m[j], m[i]);
		if (!s[m[i]][i])
			return 0;
		for (I j = i+1; j < 3; j++)
			for (I k = i+1; k < 4; k++)
				s[m[j]][k] -= s[m[i]][k]*s[m[j]][i]/s[m[i]][i];
	}
	F64 r[3];
	for (I i = 2; i >= 0; i--) {
		r[i] = s[m[i]][3];
		for (I j = i + 1; j < 3; j++)
			r[i] -= s[m[i]][j]*r[j];
		r[i] /= s[m[i]][i];
	}
	o->z = r[2];
	o->y = r[1];
	o->x = r[0];
	return 1;
}

OK cramer(F64 s[3][4], Vec *o)
{
	F64 d[4];
	for (I i = 3; i >= 0; i--) {
		I m[4] = {0, 1, 2, 3};
		m[i] = 3;
		F64 d1 = s[0][m[0]]*(s[1][m[1]]*s[2][m[2]] - s[2][m[1]]*s[1][m[2]]);
		F64 d2 = s[1][m[0]]*(s[0][m[1]]*s[2][m[2]] - s[2][m[1]]*s[0][m[2]]);
		F64 d3 = s[2][m[0]]*(s[0][m[1]]*s[1][m[2]] - s[1][m[1]]*s[0][m[2]]);
		d[i] = d1 - d2 + d3;
	}
	if (!d[3])
		return 0;
	o->x = d[0] / d[3];
	o->y = d[1] / d[3];
	o->z = d[2] / d[3];
	return 1;
}

F64 testtriangle(Triangle t, Vec o, Vec d)
{
	/* o + t*d = p1 + u*(p2 - p1) + v*(p3 - p1) */
	F64 s[3][4] = {
		{d.x, t.p1.x - t.p2.x, t.p1.x - t.p3.x, t.p1.x - o.x},
		{d.y, t.p1.y - t.p2.y, t.p1.y - t.p3.y, t.p1.y - o.y},
		{d.z, t.p1.z - t.p2.z, t.p1.z - t.p3.z, t.p1.z - o.z}
	};
	Vec v;
	/* TODO: I need to research numerical methods more,
	 * Cramer`s method here seems to be doing better while Gauss`s
	 * looses pixels sometimes */
	if (!cramer(s, &v))
		return INF;
	if (v.x <= 0 || v.y < 0 || v.z < 0 || v.y + v.z > 1)
		return INF;
	return v.x;
}

Vec reflect(Vec v, Vec n)
{
	Vec p = mul(n, -dot(v, n) / dot(n, n));
	return add(p, add(v, p));
}

Color ray(Vec o, Vec d)
{
	Color color = d.y < 0 ? RGBA(18, 18, 18, 255) : RGBA(36, 36, 36, 255);
	F64 tmin = INF;
	F64 f = 1;
	for (U i = 0;; i++) {
		Sphere s = spheres[i];
		if (!s.r)
			break;
		F64 t = testsphere(s, o, d);
		if (t < tmin) {
			color = s.c;
			tmin = t;
			Vec n = mul(sub(add(o, mul(d, t)), s.p), 1/s.r);
			f = dot(n, d)/dot(d, d);
		}
	}
	for (U i = 0;; i++) {
		Triangle tr = triangles[i];
		if (eq(tr.p1, tr.p2))
			break;
		F64 t = testtriangle(tr, o, d);
		if (t < tmin) {
			color = tr.c;
			tmin = t;
			f = 1;
		}
	}
	f = CLAMP(ABS(f), 0, 1);
	return RGBA(R(color)*f, G(color)*f, B(color)*f, 0);
}

Vec shoot(Camera c, Image *i, U16 x, U16 y)
{
	return (Vec){(x - i->w/2)*c.vw/i->w, (i->h/2 - y)*c.vh/i->h, c.d};
}

void rmmul(F64 dst[3][3], F64 right[3][3])
{
	for (I r = 0; r < 3; r++) {
		F64 tmp[3] = {0};
		for (I c = 0; c < 3; c++)
			for (I j = 0; j < 3; j++)
				tmp[c] += dst[r][j]*right[j][c];
		for (I c = 0; c < 3; c++)
			dst[r][c] = tmp[c];
	}
}

void lmmul(F64 dst[3][3], F64 left[3][3])
{
	for (I c = 0; c < 3; c++) {
		F64 tmp[3] = {0};
		for (I r = 0; r < 3; r++)
			for (I j = 0; j < 3; j++)
				tmp[r] += left[r][j]*dst[j][c];
		for (I r = 0; r < 3; r++)
			dst[r][c] = tmp[r];
	}
}

typedef enum {
	CameraSpace,
	GlobalSpace,
} XYZ;

void xrot(F64 m[3][3], F64 a, XYZ s)
{
	if (!a)
		return;
	F64 rm[3][3] = {{1, 0, 0}, {0, fcos(a), -fsin(a)}, {0, fsin(a), fcos(a)}};
	switch (s) {
		case CameraSpace: return lmmul(m, rm);
		case GlobalSpace: return rmmul(m, rm);
	}
}

void yrot(F64 m[3][3], F64 a, XYZ s)
{
	if (!a)
		return;
	F64 rm[3][3] = {{fcos(a), 0, fsin(a)}, {0, 1, 0}, {-fsin(a), 0, fcos(a)}};
	switch (s) {
		case CameraSpace: return lmmul(m, rm);
		case GlobalSpace: return rmmul(m, rm);
	}
}

void zrot(F64 m[3][3], F64 a, XYZ s)
{
	if (!a)
		return;
	F64 rm[3][3] = {{fcos(a), -fsin(a), 0}, {fsin(a), fcos(a), 0}, {0, 0, 1}};
	switch (s) {
		case CameraSpace: return lmmul(m, rm);
		case GlobalSpace: return rmmul(m, rm);
	}
}

Vec transform(F64 m[3][3], Vec v)
{
	return (Vec){
		v.x*m[0][0] + v.y*m[0][1] + v.z*m[0][2],
		v.x*m[1][0] + v.y*m[1][1] + v.z*m[1][2],
		v.x*m[2][0] + v.y*m[2][1] + v.z*m[2][2],
	};
}

int main(int, char **argv)
{
	winopen(500, 500, argv[0], 60);
	Camera c = {1, 1, 1};
	/* TODO: the position and the rotation matrix should be
	 * a part of the Camera struct */
	Vec origin = {0, 0, 0};
	F64 mat[3][3] = {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}};
	mouselock(1);
	while (!keyisdown('q')) {
		Image *f = frame();
		yrot(mat, (mousex() / (F64)f->w), CameraSpace);
		xrot(mat, (mousey() / (F64)f->h), GlobalSpace);
		if (keyisdown('w'))
			origin = add(origin, transform(mat, (Vec){0, 0, .05}));
		if (keyisdown('s'))
			origin = add(origin, transform(mat, (Vec){0, 0, -.05}));
		if (keyisdown('a'))
			origin = add(origin, transform(mat, (Vec){-.05, 0, 0}));
		if (keyisdown('d'))
			origin = add(origin, transform(mat, (Vec){.05, 0, 0}));
		if (keyisdown('h'))
			yrot(mat, -.02, CameraSpace);
		if (keyisdown('l'))
			yrot(mat, .02, CameraSpace);
		if (keyisdown('j'))
			xrot(mat, .02, GlobalSpace);
		if (keyisdown('k'))
			xrot(mat, -.02, GlobalSpace);
		for (U16 y = 0; y < f->h; y++)
		for (U16 x = 0; x < f->w; x++)
			PIXEL(f, x, y) = ray(origin, transform(mat, shoot(c, f, x, y)));
	}
	return 0;
}
