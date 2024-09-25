#include "mlib.h"
#include "types.h"
#include "fmt.h"

int main(void)
{
	U8 op;
	I x, y;
	while (ETRACE(inputln(I(&op), "\tx=", I(&x), "\ty=", I(&y)), OD)) {
		switch (op) {
		case '+':
			println("x + y = ", OD(x + y));
			break;
		case '-':
			println("x - y = ", OD(x - y));
			break;
		}
	}
	return 0;
}
