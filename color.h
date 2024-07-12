typedef u32 Color;

#define RGBA(r, g, b, a) ((b)<<(8*0) | (g)<<(8*1) | (r)<<(8*2) | (a)<<(8*3))
#define A(c) (((c)&0xFF000000)>>(8*3))
#define R(c) (((c)&0x00FF0000)>>(8*2))
#define G(c) (((c)&0x0000FF00)>>(8*1))
#define B(c) (((c)&0x000000FF)>>(8*0))

#define GREEN  RGBA(0, 255, 0, 255)
#define RED    RGBA(255, 0, 0, 255)
#define BLUE   RGBA(0, 0, 255, 255)
#define BLACK  RGBA(0, 0, 0, 255)
#define WHITE  RGBA(255, 255, 255, 255)
#define YELLOW RGBA(255, 255, 000, 255)

/* NOTE: this is simple linear blending, no gamma-correction */
#define BLEND(c1, c2) RGBA(\
	CLAMP((R(c1)*(255-A(c2)))/255 + R(c2), 0, 255),\
	CLAMP((G(c1)*(255-A(c2)))/255 + G(c2), 0, 255),\
	CLAMP((B(c1)*(255-A(c2)))/255 + B(c2), 0, 255),\
	CLAMP(255 - (255-A(c1))*(255-A(c2))/255, 0, 255))
