#include <unistd.h>

#include "types.h"
#include "fmt.h"

void panic(char *msg)
{
	eprintln("panic: ", msg);
	_exit(1);
}
