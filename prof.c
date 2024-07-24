#include <string.h>
#include <math.h>

#include "types.h"
#include "print.h"
#include "util.h"
#include "prof.h"
#include "mlib.h"
#include "alloc.h"

typedef struct {
	u64 min;
	u64 max;
	u64 stdev2; /* stdev*stdev */
	u64 avg;
	u64 n;
} Stat;

#define FMTSTAT(t) "min=", FMTU((t)->min),\
	", max=", FMTU((t)->max),\
	", avg=", FMTU((t)->avg),\
	", stdev=", FMTU(sqrtf((t)->stdev2))

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
static Profiler defpr;

static void profpush(Section *s, u64 t)
{
	if (defpr.edepth >= defpr.ecap) {
		defpr.ecap = defpr.ecap*2 + 1;
		defpr.es = memreallocarray(defpr.es, defpr.ecap, sizeof(defpr.es[0]));
	}
	Entry *e = &defpr.es[defpr.edepth];
	e->s = s;
	e->startns = t;
	defpr.edepth += 1;
}

static Entry *profpop(void)
{
	if (defpr.edepth) {
		defpr.edepth -= 1;
		return &defpr.es[defpr.edepth];
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
	if (!s) {
		if (defpr.scount >= defpr.scap) {
			defpr.scap = defpr.scap*2 + 1;
			defpr.ss = memreallocarray(defpr.ss, defpr.scap, sizeof(defpr.ss[0]));
		}
		s = &defpr.ss[defpr.scount];
		strncpy(s->name, name, MAXNAME);
		s->name[MAXNAME] = '\0';
		defpr.scount += 1;
	}
	profpush(s, timens());
}

void _profend(void)
{
	Entry *e = profpop();
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
