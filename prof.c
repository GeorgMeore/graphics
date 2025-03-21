#include "types.h"
#include "math.h"
#include "io.h"
#include "ntime.h"
#include "prof.h"
#include "alloc.h"

typedef struct {
	U64 min;
	U64 max;
	U64 stdev2; /* stdev*stdev */
	U64 avg;
	U64 n;
} Stat;

#define FMTSTAT(t) "min=", OD((t)->min),\
	", max=", OD((t)->max),\
	", avg=", OD((t)->avg),\
	", stdev=", OD(isqrt((t)->stdev2))

static void statadd(Stat *s, U64 x)
{
	if (x < s->min)
		s->min = x;
	if (x > s->max)
		s->max = x;
	s->n += 1;
	U64 newavg = (s->avg*s->n + (x - s->avg))/s->n;
	s->stdev2 = (s->stdev2*(s->n - 1) + (x - s->avg)*(x - newavg))/s->n;
	s->avg = newavg;
}

static void statreset(Stat *s)
{
	s->n = 0;
	s->avg = 0;
	s->min = MAXVAL(U64);
	s->max = 0;
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
		p->es = memrealloc(p->es, p->ecap*sizeof(p->es[0]));
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
		p->ss = memrealloc(p->ss, p->scap*sizeof(p->ss[0]));
	}
	Section *s = &p->ss[p->scount];
	for (int i = 0; i < MAXNAME && name[i]; i++)
		s->name[i] = name[i];
	statreset(&s->time);
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

static Section *proffindsection(Profiler *p, const char *name)
{
	for (int i = 0; i < p->scount; i++) {
		for (int j = 0; p->ss[i].name[j] == name[j]; j++)
			if (j >= MAXNAME || !name[j])
				return &p->ss[i];
	}
	return 0;
}

void _profbegin(const char *name)
{
	Section *s = proffindsection(&defpr, name);
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

void _profreset(const char *name)
{
	Section *s = proffindsection(&defpr, name);
	if (s)
		statreset(&s->time);
}
