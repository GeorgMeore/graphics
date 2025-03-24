#include "types.h"
#include "color.h"
#include "image.h"
#include "win.h"
#include "draw.h"
#include "math.h"

#define BGCOLOR RGBA(18, 18, 18, 255)

int main(int, char **argv)
{
	winopen(600, 600, argv[0], 60);
	F64 t = 0;
	while (!keyisdown('q')) {
		Image *fb = framebegin();
		t += lastframetime()/1e9;
		drawclear(fb, BGCOLOR);
		for (int i = 0; i < 200; i++) {
			drawsmoothcircle(fb, fb->w*i/200, fb->h/2 + fsin(t)*fsin(t + 4*PI*i/200)*fb->h/2, 5, RGBA(110, 70, 70, 255));
			drawsmoothcircle(fb, fb->w*i/200, fb->h/2 + fcos(t)*fcos(t*.8 + 4*PI*i/200)*fb->h/2, 5, RGBA(70, 110, 70, 255));
			drawsmoothcircle(fb, fb->w*i/200, fb->h/2 + fsin(t)*fsin(t*.6 + 4*PI*i/200 + PI)*fb->h/2, 5, RGBA(70, 70, 110, 255));
			drawsmoothcircle(fb, fb->w*i/200, fb->h/2 + fcos(t)*fsin(t*.4 + 4*PI*i/200 + 3*PI/2)*fb->h/2, 5, RGBA(110, 110, 110, 255));
		}
		frameend();
	}
	winclose();
	return 0;
}
