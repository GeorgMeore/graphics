#include "types.h"
#include "color.h"
#include "image.h"
#include "draw.h"
#include "win.h"
#include "math.h"

#define BGCOLOR RGBA(18, 18, 18, 255)

typedef struct {
	F64 x, y, z;
} V;

void draw3dline(Image *i, V a, V b, Color c)
{
	drawline(i, i->w/2+a.x/a.z, i->h/2+a.y/a.z, i->w/2+b.x/b.z, i->h/2+b.y/b.z, c);
}

int main(int, char **argv)
{
	winopen(800, 600, argv[0], 60);
	F64 t = 0, zoff = 3, scale = 2000;
	while (!keyisdown('q')) {
		Image *f = framebegin();
		if (keyisdown(' '))
			t += lastframetime()/1e9;
		if (keyisdown('w'))
			zoff -= 0.05;
		if (keyisdown('s'))
			zoff += 0.05;
		drawclear(f, BGCOLOR);
		for (int i = 0; i < 4; i++) {
			draw3dline(f,
				(V){scale*fsin(t), scale*0.7, fcos(t) + zoff},
				(V){scale*fsin(t), -scale*0.7, fcos(t) + zoff}, RED);
			draw3dline(f,
				(V){scale*fsin(t),  scale*0.7, fcos(t) + zoff},
				(V){scale*fsin(t+PI/2), scale*0.7, fcos(t+PI/2) + zoff}, RED);
			draw3dline(f,
				(V){scale*fsin(t), -scale*0.7, fcos(t) + zoff},
				(V){scale*fsin(t+PI/2), -scale*0.7, fcos(t+PI/2) + zoff}, RED);
			t += PI/2;
		}
		frameend();
	}
	winclose();
	return 0;
}
