#include "types.h"
#include "mlib.h"
#include "color.h"
#include "image.h"
#include "win.h"
#include "draw.h"
#include "math.h"

#define BGCOLOR RGBA(18, 18, 18, 255)

#define N 6

/* TODO: research the ways to implement the simulation more properly */
/* TODO: path traces */
int main(int, char **argv)
{
	winopen(1920, 1080, argv[0], 120);
	F64 c[N][2] = {{1000, 500}, {1100, 600}, {750, 933}, {500, 500}, {2000, 1000}, {1000, 2000}};
	F64 v[N][2] = {0};
	F64 m[N] = {300, 300, 200000, 300, 300, 100000};
	F64 ms = 0;
	for (int i = 0; i < N; i++)
		ms += m[i];
	F64 r[N];
	for (int i = 0; i < N; i++)
		r[i] = 5 + m[i]*50/ms;
	Color col[N] = {
		RGBA(110, 70, 70, 200),
		RGBA(70, 110, 70, 200),
		RGBA(70, 70, 110, 200),
		RGBA(110, 110, 110, 200),
		RGBA(70, 110, 110, 200),
		RGBA(110, 110, 70, 200)
	};
	while (!keyisdown('q')) {
		Image *f = framebegin();
		if (keyisdown('z')) {
			for (int i = 0; i < N; i++) {
				v[i][0] = 0;
				v[i][1] = 0;
			}
		}
		if (!keyisdown(' ')) {
			F64 dt = lastframetime()/8e9;
			for (int i = 0; i < N; i++)
			for (int j = i+1; j < N; j++) {
				F64 dx = c[j][0] - c[i][0], dy = c[j][1] - c[i][1];
				F64 d = fsqrt(dx*dx + dy*dy);
				if (d < r[i]+r[j]) {
					F64 s = r[i] + r[j] - d;
					v[i][0] -= 1e6 * s / m[i] * dx/d * dt;
					v[j][0] += 1e6 * s / m[j] * dy/d * dt;
					v[i][1] -= 1e6 * s / m[i] * dx/d * dt;
					v[j][1] += 1e6 * s / m[j] * dy/d * dt;
				} else {
					v[i][0] += 1e4 * m[j] * dx / (d*d*d) * dt;
					v[j][0] -= 1e4 * m[i] * dx / (d*d*d) * dt;
					v[i][1] += 1e4 * m[j] * dy / (d*d*d) * dt;
					v[j][1] -= 1e4 * m[i] * dy / (d*d*d) * dt;
				}
			}
			for (int i = 0; i < N; i++) {
				c[i][0] += dt*v[i][0];
				if (c[i][0] <= r[i])
					c[i][0] = (3*r[i] - c[i][0])/2, v[i][0] *= -.6;
				if (c[i][0] + r[i] >= f->w)
					c[i][0] = (3*f->w - c[i][0] - 3*r[i])/2, v[i][0] *= -.6;
				c[i][1] += dt*v[i][1];
				if (c[i][1] < r[i])
					c[i][1] = (3*r[i] - c[i][1])/2, v[i][1] *= -.6;
				if (c[i][1] + r[i] >= f->h)
					c[i][1] = (3*f->h - c[i][1] - 3*r[i])/2, v[i][1] *= -.6;
			}
		}
		drawclear(f, BGCOLOR);
		for (int i = 0; i < N; i++)
			drawsmoothcircle(f, c[i][0], c[i][1], r[i], col[i]);
		frameend();
	}
	return 0;
}
