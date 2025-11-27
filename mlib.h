#define STRING(x) #x
#define VALSTRING(x) STRING(x)
#define LOCATION __FILE__ ":" VALSTRING(__LINE__)

#define SWAP(x, y) ({\
	typeof(x) _tmp = (x);\
	(x) = (y);\
	(y) = _tmp;\
})

#define BOOL(v) (!!(v)) /* non-zero -> 1 */

/* NOTE: this should have been (a > 0) - (a < 0), but gcc
 * nags when you try to use this macro with an unsigned type */
#define SIGN(a) (((a) > 0) + ((a) == 0) - ((a) <= 0))

#define ABS(a) ((a) * SIGN(a))
#define DIVCEIL(n, d) ((ABS(n) + ABS(d) - 1) / (d) * SIGN(n))
#define DIVROUND(n, d) ((ABS(n) + ABS(d)/2) / (d) * SIGN(n))
#define SQUARE(n) ((n) * (n))
#define MOD(a, b) ((((a) % (b)) + (b)) % (b))
#define MIN(a, b) ({\
	typeof(a) _av = (a);\
	typeof(b) _bv = (b);\
	((_av) > (_bv) ? (_bv) : (_av));\
})
#define MAX(a, b) ({\
	typeof(a) _av = (a);\
	typeof(b) _bv = (b);\
	((_av) < (_bv) ? (_bv) : (_av));\
})
#define MIN3(a, b, c) MIN(a, MIN(b, c))
#define MAX3(a, b, c) MAX(a, MAX(b, c))
#define CLAMP(x, l, r) (MIN(MAX(l, x), r))
#define ALIGNUP(n, m) (DIVCEIL(n, m) * (m))
#define ALIGNDOWN(n, m) ((n) / (m) * (m))
#define LP2(n) ((n) ^ ((n) & ((n) - 1))) /* lowest power of 2 that evenly divides n */

#define MASK(s, l) (((U64)-1 >> (64-(l))) << (s))
