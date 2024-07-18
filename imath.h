#define SIGN(a) (((a) > 0) + ((a) == 0) - ((a) <= 0))
#define ABS(a) ((a) * SIGN(a))
#define DIVCEIL(n, d) ((ABS(n) + ABS(d) - 1) / d * SIGN(n))
#define DIVROUND(n, d) ((ABS(n) + ABS(d)/2) / d * SIGN(n))
#define SQUARE(n) ((n) * (n))
#define MOD(a, b) (((a % b) + b) % b)
#define MIN(a, b) ((a) > (b) ? (b) : (a))
#define MAX(a, b) ((a) < (b) ? (b) : (a))
#define CLAMP(a, l, r) (MIN(MAX(l, a), r))
#define ALIGN(n, m) (DIVCEIL(n, m) * (m))
