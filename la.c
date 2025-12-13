#include "types.h"
#include "math.h"
#include "la.h"

Vec vsub(Vec a, Vec b)
{
	return (Vec){a.x - b.x, a.y - b.y, a.z - b.z};
}

Vec vadd(Vec a, Vec b)
{
	return (Vec){a.x + b.x, a.y + b.y, a.z + b.z};
}

Vec vmul(Vec a, F64 c)
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

Vec reflect(Vec v, Vec n)
{
	Vec p = vmul(n, -dot(v, n) / dot(n, n));
	return vadd(v, vmul(p, 2));
}

Vec refract(Vec v, Vec n, F64 eta)
{
	F64 lnsq = dot(n, n);
	if (dot(v, n) > 0) {
		eta = 1/eta;
		n = vmul(n, -1);
	}
	Vec p1 = vmul(n, -dot(v, n) / lnsq);
	Vec t1 = vadd(v, p1);
	Vec t2 = vmul(t1, eta);
	F64 lp2sq = dot(v, v) - dot(t2, t2);
	if (lp2sq < 0)
		return (Vec){0}; /* total internal reflection */
	Vec p2 = vmul(n, -fsqrt(lp2sq / lnsq));
	return vadd(p2, t2);
}

F64 det(Mat m)
{
	F64 d1 = m.m[0][0]*(m.m[1][1]*m.m[2][2] - m.m[2][1]*m.m[1][2]);
	F64 d2 = m.m[1][0]*(m.m[0][1]*m.m[2][2] - m.m[2][1]*m.m[0][2]);
	F64 d3 = m.m[2][0]*(m.m[0][1]*m.m[1][2] - m.m[1][1]*m.m[0][2]);
	return d1 - d2 + d3;
}

Mat mmul(Mat a, Mat b)
{
	Mat o = {0};
	for (I i = 0; i < 3; i++)
	for (I j = 0; j < 3; j++)
	for (I k = 0; k < 3; k++)
			o.m[i][j] += a.m[i][k]*b.m[k][j];
	return o;
}

Mat transp(Mat m)
{
	return (Mat) {{
		{m.m[0][0], m.m[1][0], m.m[2][0]},
		{m.m[0][1], m.m[1][1], m.m[2][1]},
		{m.m[0][2], m.m[1][2], m.m[2][2]}
	}};
}

Mat rotx(F64 a)
{
	return (Mat){{
		{1, 0,        0      },
		{0, fcos(a), -fsin(a)},
		{0, fsin(a),  fcos(a)}
	}};
}

Mat roty(F64 a)
{
	return (Mat){{
		{ fcos(a), 0, fsin(a)},
		{ 0,       1, 0      },
		{-fsin(a), 0, fcos(a)}
	}};
}

Mat rotz(F64 a)
{
	return (Mat){{
		{fcos(a), -fsin(a), 0},
		{fsin(a),  fcos(a), 0},
		{0,        0,       1}
	}};
}

Vec mapply(Mat m, Vec v)
{
	return (Vec){
		v.x*m.m[0][0] + v.y*m.m[0][1] + v.z*m.m[0][2],
		v.x*m.m[1][0] + v.y*m.m[1][1] + v.z*m.m[1][2],
		v.x*m.m[2][0] + v.y*m.m[2][1] + v.z*m.m[2][2],
	};
}
