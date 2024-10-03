#include "mlib.h"
#include "types.h"
#include "fmt.h"

int main(void)
{
	U8 op;
	I x, y;
	while (ETRACE(inputln(ID(&op), "\tx=", ID(&x), "\ty=", ID(&y)), OD)) {
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
