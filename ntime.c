#include <time.h>
#include <unistd.h>

#include "types.h"
#include "time.h"

U64 timens(void)
{
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return ts.tv_sec*1000000000 + ts.tv_nsec;
}

void sleepns(U64 t)
{
	struct timespec ts = {t / 1000000000, t % 1000000000};
	clock_nanosleep(CLOCK_MONOTONIC, 0, &ts, 0);
}
