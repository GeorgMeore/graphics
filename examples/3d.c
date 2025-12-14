#include "types.h"
#include "color.h"
#include "image.h"
#include "win.h"
#include "mlib.h"
#include "math.h"
#include "la.h"

typedef struct {
	F64 d;
	F64 vw, vh;
} Camera;

Vec toscreen(Vec p, Image *f)
{
	p.x = f->w*(1 + p.x)/2.0;
	p.y = f->h*(1 - p.y)/2.0;
	return p;
}

Vec fromscreen(Vec p, Image *f)
{
	p.x = 2.0*(p.x - f->w/2.0)/f->w;
	p.y = 2.0*(f->h/2.0 - p.y)/f->h;
	return p;
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

Sphere lense = {{5, 5, 5}, 1, 0};

Triangle triangles[] = {
	{{0, -1, 3}, {2, 0, 4}, {-2, 0, 4}, YELLOW},
	{{0, -1, 5}, {2, 0, 6}, {-2, 0, 6}, RED},
	{}
};

F64 testsphere(Sphere s, Vec o, Vec d)
{
	Vec po = vsub(o, s.p);
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

OK cramer(Mat m, Vec x, Vec *o)
{
	F64 d = det(m);
	if (!d)
		return 0;
	F64 dm[3];
	for (I i = 0; i < 3; i++) {
		Mat t = m;
		t.m[0][i] = x.x;
		t.m[1][i] = x.y;
		t.m[2][i] = x.z;
		dm[i] = det(t);
	}
	o->x = dm[0] / d;
	o->y = dm[1] / d;
	o->z = dm[2] / d;
	return 1;
}

F64 testtriangle(Triangle t, Vec o, Vec d)
{
	/* o + t*d = p1 + u*(p2 - p1) + v*(p3 - p1) */
	Mat m = {{
		{d.x, t.p1.x - t.p2.x, t.p1.x - t.p3.x},
		{d.y, t.p1.y - t.p2.y, t.p1.y - t.p3.y},
		{d.z, t.p1.z - t.p2.z, t.p1.z - t.p3.z}
	}};
	Vec x = {t.p1.x - o.x, t.p1.y - o.y, t.p1.z - o.z};
	Vec v;
	if (!cramer(m, x, &v))
		return INF;
	if (v.x <= 0 || v.y < 0 || v.z < 0 || v.y + v.z > 1)
		return INF;
	return v.x;
}

#define UPCOLOR RGBA(36, 36, 36, 255)
#define DOWNCOLOR RGBA(18, 18, 18, 255)

Color ray(Vec o, Vec d)
{
	/* I use "last" rendered object pointer to
	 * avoid some precision-related artifacts, where
	 * a ray hits same object in the same spot */
	static void *last = 0;
	Color c;
	if (d.y < 0)
		c = DOWNCOLOR;
	else
		c = UPCOLOR;
	F64 tmin = INF;
	F64 f = 1;
	for (U i = 0;; i++) {
		Sphere s = spheres[i];
		if (!s.r)
			break;
		F64 t = testsphere(s, o, d);
		if (t < tmin) {
			c = s.c;
			tmin = t;
			Vec n = vmul(vsub(vadd(o, vmul(d, t)), s.p), 1/s.r);
			f = dot(n, d)/dot(d, d);
		}
	}
	for (U i = 0;; i++) {
		Triangle tr = triangles[i];
		if (dot(tr.p1, tr.p1) + dot(tr.p2, tr.p2) == 0)
			break;
		F64 t = testtriangle(tr, o, d);
		if (t < tmin) {
			c = tr.c;
			tmin = t;
			f = 1;
		}
	}
	if (last != &lense) {
		F64 t = testsphere(lense, o, d);
		if (t < tmin) {
			last = &lense;
			Vec p = vmul(d, t);
			Vec o2 = vadd(o, p);
			Vec n = vsub(o2, lense.p);
			Vec d2 = refract(d, n, 1/1.6);
			if (dot(d, n) < 0) {
				/* calculate p inside sphere */
				p = vmul(d2, -2 * dot(n, d2) / dot(d2, d2));
				n = vadd(n, p);
				d2 = refract(d2, n, 1/1.6);
				o2 = vadd(o2, p);
			}
			/* TODO: use p length to calculate opacity */
			return blend(ray(o2, d2), RGBA(255, 255, 255, 2));
		}
	}
	last = 0;
	f = CLAMP(ABS(f), 0, 1);
	return RGBA(R(c)*f, G(c)*f, B(c)*f, 0);
}

void raytrace(Image *f, Vec origin, Mat m, Camera c)
{
	for (U16 y = 0; y < f->h; y++)
	for (U16 x = 0; x < f->w; x++)
		PIXEL(f, x, y) = ray(origin, mapply(m, fromscreen((Vec){x, y, c.d}, f)));
}

/* NOTE: The task of projecting a point (px, py, pz) onto a plane at z=d
 * can be rephrased as finding the coordinates of the collinear vector (px', py', d).
 * It then is immediately obvious that the answer is to scale the point vector by d/pz.
 * z is inverted to simplify interpolation. */
Vec project(Camera c, Image *f, Vec p)
{
	return toscreen((Vec){p.x*c.d/p.z, p.y*c.d/p.z, 1/p.z}, f);
}

void drawtriangle3d1(Image *i, Image *zb, Vec p1, Vec p2, Vec p3, Color c)
{
	I64 xmin = CLIPX(i, fceil(MIN3(p1.x, p2.x, p3.x)));
	I64 xmax = CLIPX(i, ffloor(MAX3(p1.x, p2.x, p3.x))+1);
	I64 ymin = CLIPY(i, fceil(MIN3(p1.y, p2.y, p3.y)));
	I64 ymax = CLIPY(i, ffloor(MAX3(p1.y, p2.y, p3.y))+1);
	if ((p3.y - p1.y)*(p2.x - p1.x) - (p3.x - p1.x)*(p2.y - p1.y) < 0)
		SWAP(p2, p3);
	F64 os = (p1.x - p3.x)*(p2.y - p3.y) - (p2.x - p3.x)*(p1.y - p3.y);
	if (!os)
		return;
	F64 o1 = (ymin - p1.y)*(p2.x - p1.x) - (xmin - p1.x)*(p2.y - p1.y);
	F64 o2 = (ymin - p2.y)*(p3.x - p2.x) - (xmin - p2.x)*(p3.y - p2.y);
	F64 o3 = (ymin - p3.y)*(p1.x - p3.x) - (xmin - p3.x)*(p1.y - p3.y);
	for (I64 y = ymin; y < ymax; y++) {
		for (I64 x = xmin; x < xmax; x++) {
			F64 z = (o2*p1.z + o3*p2.z + o1*p3.z)/os;
			if (o1 >= 0 && o2 >= 0 && o3 >= 0 && z > *(F32 *)&PIXEL(zb, x, y)) {
				*(F32 *)&PIXEL(zb, x, y) = z;
				PIXEL(i, x, y) = blend(PIXEL(i, x, y), c);
			}
			o1 -= p2.y - p1.y; o2 -= p3.y - p2.y; o3 -= p1.y - p3.y;
		}
		o1 += (p2.y - p1.y)*(xmax - xmin) + (p2.x - p1.x);
		o2 += (p3.y - p2.y)*(xmax - xmin) + (p3.x - p2.x);
		o3 += (p1.y - p3.y)*(xmax - xmin) + (p1.x - p3.x);
	}
}

/* NOTE: this version performs slightly better */
void drawtriangle3d2(Image *f, Image *zb, Vec p1, Vec p2, Vec p3, Color c)
{
	if (p3.y < p1.y)
		SWAP(p1, p3);
	if (p3.y < p2.y)
		SWAP(p2, p3);
	else if (p2.y < p1.y)
		SWAP(p1, p2);
	F64 kx12 = (p2.x - p1.x)/(p2.y - p1.y), kz12 = (p2.z - p1.z)/(p2.y - p1.y);
	F64 kx13 = (p3.x - p1.x)/(p3.y - p1.y), kz13 = (p3.z - p1.z)/(p3.y - p1.y);
	F64 kx23 = (p3.x - p2.x)/(p3.y - p2.y), kz23 = (p3.z - p2.z)/(p3.y - p2.y);
	I64 ymin = CLIPY(f, fceil(p1.y));
	I64 ymax = CLIPY(f, ffloor(p3.y)+1);
	for (I16 y = ymin; y < ymax; y++) {
		F64 x1 = p1.x + (y - p1.y)*kx13;
		F64 z1 = p1.z + (y - p1.y)*kz13;
		F64 x2, z2;
		if (y < p2.y) {
			x2 = p1.x + (y - p1.y)*kx12;
			z2 = p1.z + (y - p1.y)*kz12;
		} else {
			x2 = p2.x + (y - p2.y)*kx23;
			z2 = p2.z + (y - p2.y)*kz23;
		}
		I64 xmin = CLIPX(f, fceil(MIN(x1, x2)));
		I64 xmax = CLIPX(f, ffloor(MAX(x1, x2))+1);
		F64 kz = (z2 - z1)/(x2 - x1);
		for (I64 x = xmin; x < xmax; x++) {
			F64 z = z1 + (x - x1)*kz;
			if (z > *(F32 *)&PIXEL(zb, x, y)) {
				*(F32 *)&PIXEL(zb, x, y) = z;
				PIXEL(f, x, y) = blend(PIXEL(f, x, y), c);
			}
		}
	}
}

/* TODO: figure out how to rasterize spheres with depth buffering and clipping */
void rasterize(Image *f, Image *z, Vec origin, Mat m, Camera c)
{
	/* NOTE: for orthogonal matrices transposition is inversion */
	Mat inv = transp(m);
	/* NOTE: dot(up, p) tells you if the point is above or below horizon */
	Vec up = mapply(inv, (Vec){0, 1, 0});
	for (U16 y = 0; y < f->h; y++)
	for (U16 x = 0; x < f->w; x++) {
		/* TODO: this is suboptimal, since we can find where dot(...) == 0 */
		Vec p = fromscreen((Vec){x, y, 1}, f);
		if (dot(p, up) >= 0)
			PIXEL(f, x, y) = UPCOLOR;
		else
			PIXEL(f, x, y) = DOWNCOLOR;
		PIXEL(z, x, y) = 0;
	}
	static F64 zclip = .035; /* TODO: this also should be a part of Camera */
	for (U i = 0;; i++) {
		Triangle tr = triangles[i];
		if (dot(tr.p1, tr.p1) + dot(tr.p2, tr.p2) == 0)
			break;
		Vec p1 = mapply(inv, vsub(tr.p1, origin));
		Vec p2 = mapply(inv, vsub(tr.p2, origin));
		Vec p3 = mapply(inv, vsub(tr.p3, origin));
		if (p3.z < p1.z)
			SWAP(p1, p3);
		if (p3.z < p2.z)
			SWAP(p2, p3);
		else if (p2.z < p1.z)
			SWAP(p1, p2);
		/* TODO: i'll need to lerp vertex attributes */
		if (p1.z > zclip) {
			p1 = project(c, f, p1);
			p2 = project(c, f, p2);
			p3 = project(c, f, p3);
			drawtriangle3d2(f, z, p1, p2, p3, tr.c);
		} else if (p2.z > zclip) {
			Vec v12 = vsub(p2, p1);
			Vec p12 = vadd(p1, vmul(v12, (zclip - p1.z) / v12.z));
			Vec v13 = vsub(p3, p1);
			Vec p13 = vadd(p1, vmul(v13, (zclip - p1.z) / v13.z));
			p1 = project(c, f, p12);
			p2 = project(c, f, p2);
			p3 = project(c, f, p3);
			drawtriangle3d2(f, z, p1, p2, p3, tr.c);
			p2 = project(c, f, p13);
			drawtriangle3d2(f, z, p1, p2, p3, tr.c);
		} else if (p3.z > zclip) {
			Vec v13 = vsub(p3, p1);
			Vec p13 = vadd(p1, vmul(v13, (zclip - p1.z) / v13.z));
			Vec v23 = vsub(p3, p2);
			Vec p23 = vadd(p2, vmul(v23, (zclip - p2.z) / v23.z));
			p1 = project(c, f, p13);
			p2 = project(c, f, p23);
			p3 = project(c, f, p3);
			drawtriangle3d2(f, z, p1, p2, p3, tr.c);
		}
	}
}

#define WIDTH  600
#define HEIGHT 600

Image fbuf = {WIDTH, HEIGHT, WIDTH, (Color[WIDTH*HEIGHT]){}};
Image zbuf = {WIDTH, HEIGHT, WIDTH, (Color[WIDTH*HEIGHT]){}};

int main(int, char **argv)
{
	winopen(WIDTH, HEIGHT, argv[0], 60);
	Camera c = {1, 1, 1};
	/* TODO: the position and the rotation matrix should be
	 * a part of the Camera struct */
	Vec origin = {0, 0, 0};
	Mat m = {{{1, 0, 0}, {0, 1, 0}, {0, 0, 1}}};
	mouselock(1);
	OK trace = 1;
	while (!keyisdown('q')) {
		Image *f = frame();
		if (mousex())
			m = mmul(roty(mousex() / (F64)f->w), m);
		if (mousey())
			m = mmul(m, rotx(mousey() / (F64)f->h));
		if (keyisdown('h'))
			m = mmul(m, rotz(.05));
		if (keyisdown('l'))
			m = mmul(m, rotz(-.05));
		if (keyisdown('w'))
			origin = vadd(origin, mapply(m, (Vec){0, 0, .05}));
		if (keyisdown('s'))
			origin = vadd(origin, mapply(m, (Vec){0, 0, -.05}));
		if (keyisdown('a'))
			origin = vadd(origin, mapply(m, (Vec){-.05, 0, 0}));
		if (keyisdown('d'))
			origin = vadd(origin, mapply(m, (Vec){.05, 0, 0}));
		if (keywaspressed('r'))
			trace = !trace;
		/* TODO: There sometimes are triangle rendering differences
		 * between the rasterizer and the raytracer.
		 * It looks like that happens because camera rotation/movement
		 * is handled differently in the rasterizer and in the raytracer.
		 * I should research that sometime. */
		if (trace)
			raytrace(&fbuf, origin, m, c);
		else
			rasterize(&fbuf, &zbuf, origin, m, c);
		for (U16 y = 0; y < f->h; y++)
		for (U16 x = 0; x < f->w; x++)
			PIXEL(f, x, y) = PIXEL(&fbuf, x*WIDTH/f->w, y*HEIGHT/f->h);
	}
	return 0;
}
