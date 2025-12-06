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
	{{0, -1, 5}, {2, 0, 6}, {-2, 0, 6}, RED},
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

F64 det(F64 m[3][3])
{
	F64 d1 = m[0][0]*(m[1][1]*m[2][2] - m[2][1]*m[1][2]);
	F64 d2 = m[1][0]*(m[0][1]*m[2][2] - m[2][1]*m[0][2]);
	F64 d3 = m[2][0]*(m[0][1]*m[1][2] - m[1][1]*m[0][2]);
	return d1 - d2 + d3;
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

#define UPCOLOR RGBA(36, 36, 36, 255)
#define DOWNCOLOR RGBA(18, 18, 18, 255)

Color ray(Vec o, Vec d)
{
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
			c = tr.c;
			tmin = t;
			f = 1;
		}
	}
	f = CLAMP(ABS(f), 0, 1);
	return RGBA(R(c)*f, G(c)*f, B(c)*f, 0);
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
		case GlobalSpace: return lmmul(m, rm);
		case CameraSpace: return rmmul(m, rm);
	}
}

void yrot(F64 m[3][3], F64 a, XYZ s)
{
	if (!a)
		return;
	F64 rm[3][3] = {{fcos(a), 0, fsin(a)}, {0, 1, 0}, {-fsin(a), 0, fcos(a)}};
	switch (s) {
		case GlobalSpace: return lmmul(m, rm);
		case CameraSpace: return rmmul(m, rm);
	}
}

void zrot(F64 m[3][3], F64 a, XYZ s)
{
	if (!a)
		return;
	F64 rm[3][3] = {{fcos(a), -fsin(a), 0}, {fsin(a), fcos(a), 0}, {0, 0, 1}};
	switch (s) {
		case GlobalSpace: return lmmul(m, rm);
		case CameraSpace: return rmmul(m, rm);
	}
}

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

Vec transform(F64 m[3][3], Vec v)
{
	return (Vec){
		v.x*m[0][0] + v.y*m[0][1] + v.z*m[0][2],
		v.x*m[1][0] + v.y*m[1][1] + v.z*m[1][2],
		v.x*m[2][0] + v.y*m[2][1] + v.z*m[2][2],
	};
}

void raytrace(Image *f, Vec origin, F64 mat[3][3], Camera c)
{
	for (U16 y = 0; y < f->h; y++)
	for (U16 x = 0; x < f->w; x++)
		PIXEL(f, x, y) = ray(origin, transform(mat, fromscreen((Vec){x, y, c.d}, f)));
}

OK invert(F64 mat[3][3], F64 inv[3][3])
{
	F64 tmp[3][6];
	for (I i = 0; i < 3; i++)
	for (I j = 0; j < 3; j++) {
		tmp[i][j] = mat[i][j];
		tmp[i][3+j] = i == j;
	}
	for (I i = 0; i < 3; i++) {
		I p = i;
		for (I j = i; j < 3; j++)
			if (ABS(tmp[j][i]) > ABS(tmp[p][i]))
				p = j;
		F64 v = tmp[p][i];
		if (!v)
			return 0;
		for (I j = i; j < 6; j++) {
			SWAP(tmp[p][j], tmp[i][j]);
			tmp[i][j] /= v;
		}
		for (I k = 0; k < 3; k++) {
			for (I j = i+1; k != i && j < 6; j++)
				tmp[k][j] -= tmp[i][j]*tmp[k][i];
		}
	}
	for (I i = 0; i < 3; i++)
	for (I j = 0; j < 3; j++)
		inv[i][j] = tmp[i][3+j];
	return 1;
}

OK invert2(F64 mat[3][3], F64 inv[3][3])
{
	F64 d1 = mat[0][0]*(mat[1][1]*mat[2][2] - mat[2][1]*mat[1][2]);
	F64 d2 = mat[1][0]*(mat[0][1]*mat[2][2] - mat[2][1]*mat[0][2]);
	F64 d3 = mat[2][0]*(mat[0][1]*mat[1][2] - mat[1][1]*mat[0][2]);
	F64 d = d1 - d2 + d3;
	if (!d)
		return 0;
	F64 tmp[3][3] = {
		{
			mat[1][1]*mat[2][2]-mat[2][1]*mat[1][2],
			mat[2][1]*mat[0][2]-mat[0][1]*mat[2][2],
			mat[0][1]*mat[1][2]-mat[1][1]*mat[0][2]
		},
		{
			mat[2][0]*mat[1][2]-mat[1][0]*mat[2][2],
			mat[0][0]*mat[2][2]-mat[2][0]*mat[0][2],
			mat[1][0]*mat[0][2]-mat[0][0]*mat[1][2]
		},
		{
			mat[1][0]*mat[2][1]-mat[2][0]*mat[1][1],
			mat[2][0]*mat[0][1]-mat[0][0]*mat[2][1],
			mat[0][0]*mat[1][1]-mat[1][0]*mat[0][1]
		},
	};
	for (I i = 0; i < 3; i++)
	for (I j = 0; j < 3; j++)
		inv[i][j] = tmp[i][j]/d;
	return 1;
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
void rasterize(Image *f, Image *z, Vec origin, F64 mat[3][3], Camera c)
{
	/* for orthogonal matrices transposition is inversion */
	F64 inv[3][3] = {
		{mat[0][0], mat[1][0], mat[2][0]},
		{mat[0][1], mat[1][1], mat[2][1]},
		{mat[0][2], mat[1][2], mat[2][2]},
	};
	/* NOTE: dot(up, p) tells you if the point is above or below horizon */
	Vec up = transform(inv, (Vec){0, 1, 0});
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
		if (eq(tr.p1, tr.p2))
			break;
		Vec p1 = transform(inv, sub(tr.p1, origin));
		Vec p2 = transform(inv, sub(tr.p2, origin));
		Vec p3 = transform(inv, sub(tr.p3, origin));
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
			Vec v12 = sub(p2, p1);
			Vec p12 = add(p1, mul(v12, (zclip - p1.z) / v12.z));
			Vec v13 = sub(p3, p1);
			Vec p13 = add(p1, mul(v13, (zclip - p1.z) / v13.z));
			p1 = project(c, f, p12);
			p2 = project(c, f, p2);
			p3 = project(c, f, p3);
			drawtriangle3d2(f, z, p1, p2, p3, tr.c);
			p2 = project(c, f, p13);
			drawtriangle3d2(f, z, p1, p2, p3, tr.c);
		} else if (p3.z > zclip) {
			Vec v13 = sub(p3, p1);
			Vec p13 = add(p1, mul(v13, (zclip - p1.z) / v13.z));
			Vec v23 = sub(p3, p2);
			Vec p23 = add(p2, mul(v23, (zclip - p2.z) / v23.z));
			p1 = project(c, f, p13);
			p2 = project(c, f, p23);
			p3 = project(c, f, p3);
			drawtriangle3d2(f, z, p1, p2, p3, tr.c);
		}
	}
}

#define WIDTH 1920
#define HEIGHT 1080

Image fbuf = {WIDTH, HEIGHT, WIDTH, (Color[WIDTH*HEIGHT]){}};
Image zbuf = {WIDTH, HEIGHT, WIDTH, (Color[WIDTH*HEIGHT]){}};

int main(int, char **argv)
{
	winopen(1920, 1080, argv[0], 60);
	Camera c = {1, 1, 1};
	/* TODO: the position and the rotation matrix should be
	 * a part of the Camera struct */
	Vec origin = {0, 0, 0};
	F64 mat[3][3] = {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}};
	mouselock(1);
	OK trace = 1;
	while (!keyisdown('q')) {
		Image *f = frame();
		yrot(mat, (mousex() / (F64)f->w), GlobalSpace);
		xrot(mat, (mousey() / (F64)f->h), CameraSpace);
		if (keyisdown('h'))
			zrot(mat, .05, CameraSpace);
		if (keyisdown('l'))
			zrot(mat, -.05, CameraSpace);
		if (keyisdown('w'))
			origin = add(origin, transform(mat, (Vec){0, 0, .05}));
		if (keyisdown('s'))
			origin = add(origin, transform(mat, (Vec){0, 0, -.05}));
		if (keyisdown('a'))
			origin = add(origin, transform(mat, (Vec){-.05, 0, 0}));
		if (keyisdown('d'))
			origin = add(origin, transform(mat, (Vec){.05, 0, 0}));
		if (keywaspressed('r'))
			trace = !trace;
		/* TODO: There sometimes are triangle rendering differences
		 * between the rasterizer and the raytracer.
		 * It looks like that happens because camera rotation/movement
		 * is handled differently in the rasterizer and in the raytracer.
		 * I should research that sometime. */
		if (trace)
			raytrace(&fbuf, origin, mat, c);
		else
			rasterize(&fbuf, &zbuf, origin, mat, c);
		for (U16 y = 0; y < f->h; y++)
		for (U16 x = 0; x < f->w; x++)
			PIXEL(f, x, y) = PIXEL(&fbuf, x*WIDTH/f->w, y*HEIGHT/f->h);
	}
	return 0;
}
