#include "types.h"
#include "color.h"
#include "image.h"
#include "draw.h"
#include "io.h"
#include "panic.h"
#include "math.h"

#define IMAGE(wv, hv) (Image){.w = (wv), .s = (wv), .h = (hv), .p = (Color[(wv)*(hv)]){}}

#define WIDTH  1920
#define HEIGHT 1080
#define FPS    60

U8 Y(Color c)
{
	return 16 + (65738*R(c) + 129057*G(c) + 25064*B(c)) / 256000;
}

U8 Cb(Color c)
{
	return 128 + (-37945*R(c) - 74494*G(c) + 112439*B(c)) / 256000;
}

U8 Cr(Color c)
{
	return 128 + (112439*R(c) - 94154*G(c) - 18285*B(c)) / 256000;
}

#define BGCOLOR RGBA(18, 18, 18, 255)

void renderframe(Image *f, int n)
{
	F64 t = n / (F64)FPS;
	drawclear(f, BGCOLOR);
	for (int i = 0; i < 200; i++) {
		drawsmoothcircle(f, f->w*i/200, f->h/2 + fsin(t)*fsin(t + 4*PI*i/200)*f->h/2, 5, RGBA(110, 70, 70, 255));
		drawsmoothcircle(f, f->w*i/200, f->h/2 + fcos(t)*fcos(t*.8 + 4*PI*i/200)*f->h/2, 5, RGBA(70, 110, 70, 255));
		drawsmoothcircle(f, f->w*i/200, f->h/2 + fsin(t)*fsin(t*.6 + 4*PI*i/200 + PI)*f->h/2, 5, RGBA(70, 70, 110, 255));
		drawsmoothcircle(f, f->w*i/200, f->h/2 + fcos(t)*fsin(t*.4 + 4*PI*i/200 + 3*PI/2)*f->h/2, 5, RGBA(110, 110, 110, 255));
	}
}

/* TODO: make y4m a backend, that implements win.h, then it would be possible
 * to experiment interactively and then use the same code to generate a video. */
int main(void)
{
	IOBuffer video;
	if (!bopen(&video, "video.y4m", 'w')) {
		panic("failed to open the output file!");
		return 1;
	}
	Image frame = IMAGE(WIDTH, HEIGHT);
	bprintln(&video, "YUV4MPEG2 W", OD(WIDTH), " H", OD(HEIGHT), " F60:1 A1:1 C444");
	for (int f = 0; f < FPS*10; f++) {
		bprintln(&video, "FRAME");
		renderframe(&frame, f);
		for (U32 y = 0; y < HEIGHT; y++)
		for (U32 x = 0; x < WIDTH; x++)
			bwrite(&video, Y(PIXEL(&frame, x, y)));
		for (U32 y = 0; y < HEIGHT; y++)
		for (U32 x = 0; x < WIDTH; x++)
			bwrite(&video, Cb(PIXEL(&frame, x, y)));
		for (U32 y = 0; y < HEIGHT; y++)
		for (U32 x = 0; x < WIDTH; x++)
			bwrite(&video, Cr(PIXEL(&frame, x, y)));
		print("\rframe ", OD(f));
	}
	print("\n");
	return !bclose(&video);
}
