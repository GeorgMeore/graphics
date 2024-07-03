#include <stdio.h>
#include <time.h>

#include "prof.h"
#include "imath.h"

/* TODO: something more smart than simple stdout printing */

static const char *curr;
static struct timespec start;
static struct timespec end;

void profbegin(const char *name)
{
	if (curr) {
		fprintf(stderr, "prof: error: nested profiling sections are not allowed (%s is active)\n", curr);
		return;
	}
	curr = name;
	clock_gettime(CLOCK_REALTIME, &start);
}

#define NSECS(ts) ((ts).tv_sec*1000000000 + (ts).tv_nsec)

void profend(void)
{
	if (!curr) {
		fprintf(stderr, "prof: error: no current profiling section\n");
		return;
	}
	clock_gettime(CLOCK_REALTIME, &end);
	float msecs = (NSECS(end) - NSECS(start))/1000000.0;
	fprintf(stderr, "prof: %s took %f ms (%f op/sec)\n", curr, msecs, 1000.0/msecs);
	curr = NULL;
}
