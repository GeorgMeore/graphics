#include <time.h>
#include <unistd.h>

#include "types.h"
#include "time.h"

U64 timens(void)
{
	struct timespec ts;
	clock_gettime(CLOCK_REALTIME, &ts);
	return ts.tv_sec*1000000000 + ts.tv_nsec;
}

void sleepns(U64 t)
{
	usleep(t / 1000);
}
