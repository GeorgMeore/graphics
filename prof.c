#include <stdio.h>

#include "types.h"
#include "util.h"
#include "prof.h"
#include "imath.h"

/* TODO: something more smart than simple stdout printing */

static const char *curr;
static u64 start;
static u64 end;

void profbegin(const char *name)
{
	if (curr) {
		fprintf(stderr, "prof: error: nested profiling sections are not allowed (%s is active)\n", curr);
		return;
	}
	curr = name;
	start = timens();
}

void profend(void)
{
	if (!curr) {
		fprintf(stderr, "prof: error: no current profiling section\n");
		return;
	}
	end = timens();
	float msecs = (end - start)/1000000.0;
	fprintf(stderr, "prof: %s took %f ms (%f op/sec)\n", curr, msecs, 1000.0/msecs);
	curr = 0;
}
