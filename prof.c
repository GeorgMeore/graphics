#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "types.h"
#include "util.h"
#include "prof.h"
#include "imath.h"

typedef struct {
	u64 min;
	u64 max;
	u64 stdev2; /* stdev*stdev */
	u64 avg;
	u64 n;
} Stat;

static void statadd(Stat *s, u64 x)
{
	if (x < s->min || !s->n)
		s->min = x;
	if (x > s->max || !s->n)
		s->max = x;
	s->n += 1;
	u64 newavg = (s->avg*s->n + (x - s->avg))/s->n;
	s->stdev2 = (s->stdev2*(s->n - 1) + (x - s->avg)*(x - newavg))/s->n;
	s->avg = newavg;
}

#define MAXNAME 256

typedef struct {
	char name[MAXNAME+1];
	Stat time;
} Section;

typedef struct {
	u64 startns;
	Section *s;
} Entry;

typedef struct {
	Section *ss;
	u16 scount, scap;
	Entry *es;
	u16 edepth, ecap;
} Profiler;

/* NOTE: the profiler exists throughout the program execution,
 * so we don't bother with freeing up it's resources. */
static Profiler p;

static void profpush(Section *s, u64 t)
{
	if (p.edepth >= p.ecap) {
		p.ecap = p.ecap*2 + 1;
		p.es = reallocarray(p.es, p.ecap, sizeof(p.es[0]));
	}
	Entry *e = &p.es[p.edepth];
	e->s = s;
	e->startns = t;
	p.edepth += 1;
}

static Entry *profpop(void)
{
	if (p.edepth) {
		p.edepth -= 1;
		return &p.es[p.edepth];
	}
	return NULL;
}

void profbegin(const char *name)
{
	Section *s = NULL;
	for (int i = 0; i < p.scount; i++) {
		if (!strncmp(p.ss[i].name, name, MAXNAME)) {
			s = &p.ss[i];
			break;
		}
	}
	if (!s) {
		if (p.scount >= p.scap) {
			p.scap = p.scap*2 + 1;
			p.ss = reallocarray(p.ss, p.scap, sizeof(p.ss[0]));
		}
		s = &p.ss[p.scount];
		strncpy(s->name, name, MAXNAME);
		s->name[MAXNAME] = '\0';
		p.scount += 1;
	}
	profpush(s, timens());
}

void profend(void)
{
	Entry *e = profpop();
	if (!e)
		return;
	statadd(&e->s->time, timens() - e->startns);
}

void profdump(void)
{
	for (int i = 0; i < p.scount; i++) {
		Section *s = &p.ss[i];
		Stat *t = &s->time;
		printf("prof: %s: min=%lu, max=%lu, avg=%lu, stdev=%f\n", s->name, t->min, t->max, t->avg, sqrtf(t->stdev2));
	}
}
