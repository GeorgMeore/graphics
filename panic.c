#include <unistd.h>

#include "print.h"

void panic(char *msg)
{
	eprintln("panic: ", msg);
	_exit(1);
}
