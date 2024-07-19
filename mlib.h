#define STRING(x) #x
#define VALSTRING(x) STRING(x)
#define LOCATION __FILE__ ":" VALSTRING(__LINE__)

#define SWAP(x, y) ({ typeof(x) tmp; tmp = (x); (x) = (y); (y) = tmp; })

#define BOOL(v) (!!v) /* non-zero -> 1 */

/* NOTE: shis should have been (a > 0) - (a < 0), but gcc
 * nags when you try to use this macro with an unsigned type */
#define SIGN(a) (((a) > 0) + ((a) == 0) - ((a) <= 0))
#define ABS(a) ((a) * SIGN(a))
#define DIVCEIL(n, d) ((ABS(n) + ABS(d) - 1) / d * SIGN(n))
#define DIVROUND(n, d) ((ABS(n) + ABS(d)/2) / d * SIGN(n))
#define SQUARE(n) ((n) * (n))
#define MOD(a, b) (((a % b) + b) % b)
#define MIN(a, b) ((a) > (b) ? (b) : (a))
#define MAX(a, b) ((a) < (b) ? (b) : (a))
#define CLAMP(a, l, r) (MIN(MAX(l, a), r))
#define ALIGNUP(n, m) (DIVCEIL(n, m) * (m))
#define ALIGNDOWN(n, m) ((n) / (m) * (m))
