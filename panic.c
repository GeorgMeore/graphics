#include <unistd.h>

#include "types.h"
#include "io.h"

void panic(char *msg)
{
	eprintln("panic: ", msg);
	_exit(1);
}
