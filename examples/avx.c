#include "types.h"
#include "alloc.h"
#include "mlib.h"
#include "color.h"
#include "image.h"
#include "win.h"
#include "io.h"
#include "draw.h"

#define PROFENABLED
#include "prof.h"

#include <immintrin.h>

typedef union {
	F64     v[4];
	__m256d a;
} V4;

__m256d _mm256_and3_pd(__m256d x, __m256d y, __m256d z)
{
	return _mm256_and_pd(x, _mm256_and_pd(y, z));
}

/* TODO: figure out color blending with avx */
void drawtriangleavx(Image *i, F64 x1, F64 y1, F64 x2, F64 y2, F64 x3, F64 y3, Color c)
{
	I64 xmin = CLIPX(i, MIN3(x1, x2, x3)), xmax = CLIPX(i, 1+MAX3(x1, x2, x3));
	I64 ymin = CLIPY(i, MIN3(y1, y2, y3)), ymax = CLIPY(i, 1+MAX3(y1, y2, y3));
	if ((y3 - y1)*(x2 - x1) - (x3 - x1)*(y2 - y1) < 0) {
		SWAP(x2, x3);
		SWAP(y2, y3);
	}
	__m256d xv = _mm256_set_pd(xmin+3, xmin+2, xmin+1, xmin);
	__m256d o1 = _mm256_fmsub_pd(
		_mm256_set1_pd(ymin - y1), _mm256_set1_pd(x2 - x1),
		_mm256_mul_pd(_mm256_sub_pd(xv, _mm256_set1_pd(x1)), _mm256_set1_pd(y2 - y1)));
	__m256d o2 = _mm256_fmsub_pd(
		_mm256_set1_pd(ymin - y2), _mm256_set1_pd(x3 - x2),
		_mm256_mul_pd(_mm256_sub_pd(xv, _mm256_set1_pd(x2)), _mm256_set1_pd(y3 - y2)));
	__m256d o3 = _mm256_fmsub_pd(
		_mm256_set1_pd(ymin - y3), _mm256_set1_pd(x1 - x3),
		_mm256_mul_pd(_mm256_sub_pd(xv, _mm256_set1_pd(x3)), _mm256_set1_pd(y1 - y3)));
	__m256d do1x = _mm256_set1_pd(4*(y2 - y1));
	__m256d do2x = _mm256_set1_pd(4*(y3 - y2));
	__m256d do3x = _mm256_set1_pd(4*(y1 - y3));
	__m256d do1y = _mm256_fmadd_pd(do1x, _mm256_set1_pd((xmax-xmin+3)/4), _mm256_set1_pd(x2 - x1));
	__m256d do2y = _mm256_fmadd_pd(do2x, _mm256_set1_pd((xmax-xmin+3)/4), _mm256_set1_pd(x3 - x2));
	__m256d do3y = _mm256_fmadd_pd(do3x, _mm256_set1_pd((xmax-xmin+3)/4), _mm256_set1_pd(x1 - x3));
	for (I32 y = ymin; y < ymax; y += 1) {
		for (I32 x = xmin; x < xmax; x += 4) {
			__m256d mask = _mm256_and3_pd(
					_mm256_cmp_pd(o1, _mm256_set1_pd(0), _CMP_GE_OS),
					_mm256_cmp_pd(o2, _mm256_set1_pd(0), _CMP_GE_OS),
					_mm256_cmp_pd(o3, _mm256_set1_pd(0), _CMP_GE_OS));
			mask = _mm256_and_pd(mask, _mm256_cmp_pd(_mm256_set_pd(x+3,x+2,x+1,x), _mm256_set1_pd(xmax), _CMP_LT_OS));
			_mm_maskstore_ps((void *)&i->p[y*i->w + x],
				_mm_castps_si128(_mm256_cvtpd_ps(mask)),
				_mm_castsi128_ps(_mm_set1_epi32(c)));
			o1 = _mm256_sub_pd(o1, do1x);
			o2 = _mm256_sub_pd(o2, do2x);
			o3 = _mm256_sub_pd(o3, do3x);
		}
		o1 = _mm256_add_pd(o1, do1y);
		o2 = _mm256_add_pd(o2, do2y);
		o3 = _mm256_add_pd(o3, do3y);
	}
}

__m256 _mm256_and3_ps(__m256 x, __m256 y, __m256 z)
{
	return _mm256_and_ps(x, _mm256_and_ps(y, z));
}

/* This version is supposed to be more parallel, but is still slower */
void drawtriangleavx2(Image *i, I16 x1, I16 y1, I16 x2, I16 y2, I16 x3, I16 y3, Color c)
{
	I64 xmin = CLIPX(i, MIN3(x1, x2, x3)), xmax = CLIPX(i, 1+MAX3(x1, x2, x3));
	I64 ymin = CLIPY(i, MIN3(y1, y2, y3)), ymax = CLIPY(i, 1+MAX3(y1, y2, y3));
	if ((y3 - y1)*(x2 - x1) - (x3 - x1)*(y2 - y1) < 0) {
		SWAP(x2, x3);
		SWAP(y2, y3);
	}
	__m256 xoff = _mm256_set_ps(7, 6, 5, 4, 3, 2, 1, 0);
	__m256 xv = _mm256_add_ps(_mm256_set1_ps(xmin), xoff);
	__m256 o1 = _mm256_fmsub_ps(
		_mm256_set1_ps(ymin - y1), _mm256_set1_ps(x2 - x1),
		_mm256_mul_ps(_mm256_sub_ps(xv, _mm256_set1_ps(x1)), _mm256_set1_ps(y2 - y1)));
	__m256 o2 = _mm256_fmsub_ps(
		_mm256_set1_ps(ymin - y2), _mm256_set1_ps(x3 - x2),
		_mm256_mul_ps(_mm256_sub_ps(xv, _mm256_set1_ps(x2)), _mm256_set1_ps(y3 - y2)));
	__m256 o3 = _mm256_fmsub_ps(
		_mm256_set1_ps(ymin - y3), _mm256_set1_ps(x1 - x3),
		_mm256_mul_ps(_mm256_sub_ps(xv, _mm256_set1_ps(x3)), _mm256_set1_ps(y1 - y3)));
	for (I64 y = ymin; y < ymax; y += 1) {
		__m256 o1y = _mm256_fmadd_ps(_mm256_set1_ps(x2 - x1), _mm256_set1_ps(y - ymin), o1);
		__m256 o2y = _mm256_fmadd_ps(_mm256_set1_ps(x3 - x2), _mm256_set1_ps(y - ymin), o2);
		__m256 o3y = _mm256_fmadd_ps(_mm256_set1_ps(x1 - x3), _mm256_set1_ps(y - ymin), o3);
		for (I64 x = xmin; x < xmax; x += 8) {
			__m256 to1 = _mm256_fmadd_ps(_mm256_set1_ps(y1 - y2), _mm256_set1_ps(x - xmin), o1y);
			__m256 to2 = _mm256_fmadd_ps(_mm256_set1_ps(y2 - y3), _mm256_set1_ps(x - xmin), o2y);
			__m256 to3 = _mm256_fmadd_ps(_mm256_set1_ps(y3 - y1), _mm256_set1_ps(x - xmin), o3y);
			__m256 mask = _mm256_and3_ps(
					_mm256_cmp_ps(to1, _mm256_set1_ps(0), _CMP_GE_OS),
					_mm256_cmp_ps(to2, _mm256_set1_ps(0), _CMP_GE_OS),
					_mm256_cmp_ps(to3, _mm256_set1_ps(0), _CMP_GE_OS));
			mask = _mm256_and_ps(mask,
				_mm256_cmp_ps(_mm256_add_ps(_mm256_set1_ps(x), xoff), _mm256_set1_ps(xmax), _CMP_LT_OS));
			_mm256_maskstore_epi32((void *)&i->p[y*i->w + x], (__m256i)mask, _mm256_set1_epi32(c));
		}
	}
}

void drawtrianglesimp(Image *i, I32 x1, I32 y1, I32 x2, I32 y2, I32 x3, I32 y3, Color c)
{
	I64 xmin = CLIPX(i, MIN3(x1, x2, x3)), xmax = CLIPX(i, 1+MAX3(x1, x2, x3));
	I64 ymin = CLIPY(i, MIN3(y1, y2, y3)), ymax = CLIPY(i, 1+MAX3(y1, y2, y3));
	if ((y3 - y1)*(x2 - x1) - (x3 - x1)*(y2 - y1) < 0) {
		SWAP(x2, x3);
		SWAP(y2, y3);
	}
	I64 o1 = (ymin - y1)*(x2 - x1) - (xmin - x1)*(y2 - y1);
	I64 o2 = (ymin - y2)*(x3 - x2) - (xmin - x2)*(y3 - y2);
	I64 o3 = (ymin - y3)*(x1 - x3) - (xmin - x3)*(y1 - y3);
	for (I64 y = ymin; y < ymax; y++) {
		for (I64 x = xmin; x < xmax; x++) {
			if (o1 >= 0 && o2 >= 0 && o3 >= 0)
				PIXEL(i, x, y) = c/*BLEND(PIXEL(i, x, y), c)*/;
			o1 -= y2 - y1; o2 -= y3 - y2; o3 -= y1 - y3;
		}
		o1 += (y2 - y1)*(xmax - xmin) + (x2 - x1);
		o2 += (y3 - y2)*(xmax - xmin) + (x3 - x2);
		o3 += (y1 - y3)*(xmax - xmin) + (x1 - x3);
	}
}

void updatepoint(Image *i, int p[2])
{
	static void *curr = 0;
	int mx = mousex(), my = mousey();
	if (!btnisdown(1))
		curr = 0;
	else if (!curr && SQUARE(p[0] - mx) + SQUARE(p[1] - my) < SQUARE(6))
		curr = p;
	if (curr == p) {
		p[0] = CLAMP(mx, 0, i->w);
		p[1] = CLAMP(my, 0, i->h);
	}
}

int main(int, char **argv)
{
	winopen(600, 600, argv[0], 60);
	int pt[3][2] = {{100, 100}, {200, 200}, {300, 100}};
	Color c[3] = {RED, GREEN, BLUE};
	int toggle = 1;
	while (!keyisdown('q')) {
		Image *fb = framebegin();
		for (int i = 0; i < 3; i++)
			updatepoint(fb, pt[i]);
		if (keywaspressed('s')) {
			toggle = !toggle;
			profreset("triangle");
		}
		drawclear(fb, BLACK);
		for (int i = 0; i < 3; i++)
			drawsmoothcircle(fb, pt[i][0], pt[i][1], 5, c[i]);
		profbegin("triangle");
		if (toggle)
			drawtriangleavx(fb, pt[0][0], pt[0][1], pt[1][0], pt[1][1], pt[2][0], pt[2][1], BLUE);
		else
			drawtriangleavx2(fb, pt[0][0], pt[0][1], pt[1][0], pt[1][1], pt[2][0], pt[2][1], RED);
		profend();
		profdump();
		frameend();
	}
	winclose();
	return 0;
}
