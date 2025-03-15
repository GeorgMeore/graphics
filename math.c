#include "types.h"
#include "math.h"

U64 isqrt(U64 x)
{
	U64 l = 0, r = x;
	while (l < r) {
		U64 m = l + (r - l)/2;
		if (m*m > x)
			r = m;
		else
			l = m + 1;
	}
	return l - 1;
}
