#include <string.h>
#include <math.h>

#include "types.h"
#include "print.h"
#include "ntime.h"
#include "prof.h"
#include "mlib.h"
#include "alloc.h"

typedef struct {
	U64 min;
	U64 max;
	U64 stdev2; /* stdev*stdev */
	U64 avg;
	U64 n;
} Stat;

#define FMTSTAT(t) "min=", FMTU((t)->min),\
	", max=", FMTU((t)->max),\
	", avg=", FMTU((t)->avg),\
	", stdev=", FMTU(sqrtf((t)->stdev2))

static void statadd(Stat *s, U64 x)
{
	if (x < s->min || !s->n)
		s->min = x;
	if (x > s->max || !s->n)
		s->max = x;
	s->n += 1;
	U64 newavg = (s->avg*s->n + (x - s->avg))/s->n;
	s->stdev2 = (s->stdev2*(s->n - 1) + (x - s->avg)*(x - newavg))/s->n;
	s->avg = newavg;
}

#define MAXNAME 256

typedef struct {
	char name[MAXNAME+1];
	Stat time;
} Section;

typedef struct {
	U64 startns;
	Section *s;
} Entry;

typedef struct {
	Section *ss;
	U16 scount, scap;
	Entry *es;
	U16 edepth, ecap;
} Profiler;

/* NOTE: the profiler exists throughout the program execution,
 * so we don't bother with freeing up it's resources. */
static Profiler defpr;

static void profpush(Profiler *p, Section *s, U64 t)
{
	if (p->edepth >= p->ecap) {
		p->ecap = p->ecap*2 + 1;
		p->es = memreallocarray(p->es, p->ecap, sizeof(p->es[0]));
	}
	Entry *e = &p->es[p->edepth];
	e->s = s;
	e->startns = t;
	p->edepth += 1;
}

static Section *profaddsection(Profiler *p, const char *name)
{
	if (p->scount >= p->scap) {
		p->scap = p->scap*2 + 1;
		p->ss = memreallocarray(p->ss, p->scap, sizeof(p->ss[0]));
	}
	Section *s = &p->ss[p->scount];
	strncpy(s->name, name, MAXNAME);
	s->name[MAXNAME] = '\0';
	p->scount += 1;
	return s;
}

static Entry *profpop(Profiler *p)
{
	if (p->edepth) {
		p->edepth -= 1;
		return &p->es[p->edepth];
	}
	return 0;
}

void _profbegin(const char *name)
{
	Section *s = 0;
	for (int i = 0; i < defpr.scount; i++) {
		if (!strncmp(defpr.ss[i].name, name, MAXNAME)) {
			s = &defpr.ss[i];
			break;
		}
	}
	if (!s)
		s = profaddsection(&defpr, name);
	profpush(&defpr, s, timens());
}

void _profend(void)
{
	Entry *e = profpop(&defpr);
	if (!e)
		return;
	statadd(&e->s->time, timens() - e->startns);
}

void _profdump(void)
{
	for (int i = 0; i < defpr.scount; i++) {
		Section *s = &defpr.ss[i];
		println("prof: ", s->name, ": ", FMTSTAT(&s->time));
	}
}
