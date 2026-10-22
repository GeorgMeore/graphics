#include <stdio.h>

#include "types.h"
#include "math.h"

void itos(U64 v, U8 *d, U8 base, U16 *n)
{
	if (!v) {
		d[0] = '0';
		*n = 1;
		return;
	}
	U c;
	for (c = 0; v; c++) {
		/* hacky hacky hex digit calculation */
		d[c] = '0' + v%base%10 + v%base/10*('a' - '0');
		v /= base;
	}
	*n = c;
	for (U16 i = 0; i < c/2; i++)
		SWAP(d[i], d[c-1-i]);
}

void mul2(U8 *d, U16 *n, I16 *pow)
{
	U16 shift = 0;
	if (d[0] >= '5') {
		*pow += 1;
		shift = 1;
	}
	U8 c = 0;
	for (I i = *n-1; i >= 0; i--) {
		U8 x = c + (d[i] - '0')*2;
		d[i+shift] = '0' + x%10;
		c = x / 10;
	}
	if (shift) {
		d[0] = '1';
		*n += 1;
	}
}

void div2(U8 *d, U16 *n, I16 *pow)
{
	if (d[*n-1] % 2) {
		d[*n] = '0';
		*n += 1;
	}
	U16 shift = 0;
	U8 c = 0;
	if (d[0] < '2') {
		shift = 1;
		c = d[0] - '0';
		*n -= 1;
		*pow -= 1;
	}
	for (I i = 0; i < *n; i++) {
		U8 x = c*10 + (d[i+shift] - '0');
		d[i] = '0' + x/2;
		c = x % 2;
	}
}

F64 scan(U8 *d, U16 n, I16 pow)
{
	U8 tmp[1000];
	for (I i = 0; i < n; i++) {
		tmp[i] = d[i];
	}
	d = tmp;
	I16 e = -52;
	for (; pow < 0; e--)
		mul2(d, &n, &pow);
	for (; pow > 0 || d[0] > '1'; e++)
		div2(d, &n, &pow);
	I64 m = 0;
	/* TODO: round at the end? */
	for (I i = 0; i < 53; i++) {
		m = (m << 1) | (d[0] - '0');
		d[0] = '0';
		mul2(d, &n, &pow);
	}
	printf("scan: e = %d, m = %ld\n", e, m);
	return fpack(m, e);
}

void print(F64 x)
{
	I64 m;
	I16 e;
	funpack(x, &m, &e);
	if (fpack(m, e) != x)
		printf("fucking fuck\n");
	printf("e = %d, m = %ld\n", e, m);
	U8 digits[1000]; /* more than enough for any number */
	U16 n;
	itos(iabs(m), digits, 10, &n);
	I16 pow = n - 1;
	for (; e > 0; e--)
		mul2(digits, &n, &pow);
	for (; e < 0; e++)
		div2(digits, &n, &pow);
	for (I i = 1; i < n; i++) {
		/* TODO: round first */
		if (scan(digits, i, pow) == x) {
			n = i;
			printf("goyda\n");
			break;
		}
	}
	if (m < 0)
		printf("-");
	printf("%c.%.*s", digits[0], n - 1, digits + 1);
	if (pow)
		printf("e%d", pow);
	printf("\n");
	scan(digits, n, pow);
}

int main()
{
	print(15.0 / 128.0);
	print(1.3);
	print(1.7);
	print(1.3e100);
	return 0;
}
