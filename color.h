typedef U32 Color;

#define RGBA(r, g, b, a) (\
	((U32)(b))<<(8*0)|\
	((U32)(g))<<(8*1)|\
	((U32)(r))<<(8*2)|\
	((U32)(a))<<(8*3))

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

/* NOTE: no gamma-correction (not even sure it is correct, but looks ok) */
#define BLEND(b, t) RGBA(\
	CLAMP((R(b)*(255-A(t)) + R(t)*A(t))/255, 0, 255),\
	CLAMP((G(b)*(255-A(t)) + G(t)*A(t))/255, 0, 255),\
	CLAMP((B(b)*(255-A(t)) + B(t)*A(t))/255, 0, 255),\
	CLAMP((A(t)*255 + A(b)*(255 - A(t)))/255, 0, 255))
