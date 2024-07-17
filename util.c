#include <time.h>
#include <unistd.h>

#include "types.h"
#include "util.h"
#include "print.h"

u64 timens(void)
{
	struct timespec ts;
	clock_gettime(CLOCK_REALTIME, &ts);
	return ts.tv_sec*1000000000 + ts.tv_nsec;
}

void sleepns(u64 t)
{
	usleep(t / 1000);
}

void panic(char *msg)
{
	eprintln("panic: ", msg);
	_exit(1);
}
