#include "types.h"
#include "color.h"
#include "mlib.h"

/* NOTE: no gamma-correction */
Color blend(Color b, Color t)
{
	U8 rc = DIVROUND(R(b)*(255-A(t)) + R(t)*A(t), 255);
	U8 gc = DIVROUND(G(b)*(255-A(t)) + G(t)*A(t), 255);
	U8 bc = DIVROUND(B(b)*(255-A(t)) + B(t)*A(t), 255);
	return RGBA(rc, gc, bc, 0);
}

/* NOTE: the formula can be derived by taking a base color
 * and then applying simple blending of b and t sequentially and
 * analyzing the result */
Color compose(Color b, Color t)
{
	U32 ac = (A(b) + A(t))*255 - A(b)*A(t);
	if (ac == 0)
		return RGBA(0, 0, 0, 0);
	U8 rc = DIVROUND((255 - A(t))*A(b)*R(b) + A(t)*R(t)*255, ac);
	U8 gc = DIVROUND((255 - A(t))*A(b)*G(b) + A(t)*G(t)*255, ac);
	U8 bc = DIVROUND((255 - A(t))*A(b)*B(b) + A(t)*B(t)*255, ac);
	return RGBA(rc, gc, bc, DIVROUND(ac, 255));
}
