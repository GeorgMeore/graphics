#include <sys/mman.h>

#include "mlib.h"
#include "types.h"
#include "alloc.h"

#define PAGESIZE 4096

static void *pagemap(uW size)
{
	void *p = mmap(0, size, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
	if (p == MAP_FAILED)
		return 0;
	return p;
}

/* TODO: use crc or something for metadata corruption detection */
struct Zone {
	uW free;  /* in bytes */
	void *mem;
	Zone *next;
};

/*
 * The layout of a zone in memory:
 *                        Unallocated memory (Zone.free bytes)
 *                          .---------------.
 *  _________________ _____.v____.....______v
 * |                 |     .                |
 * | Zone descriptor |     .                |
 * |      (Zone)     |     .                |
 * '_________________'_____._____.....______'
 *          |              ^
 *          `--------------'
 * The pointer to the start of the free memory (Zone.mem)
 *
 *
 * The layout of an arena in memory:
 *   ______ ______
 *  |      |      |
 *  | head | tail | (Arena)
 *  '______'______'
 *      |     `--------------------------------------.
 *  ____V________    _________________           ____V_____
 * |             |  |                 |         |          |
 * |    Zone 1   |  |     Zone 2      |  .....  |  Zone n  |
 * '_____________'  '_________________'  ^   |  '__________'
 *   `--------------^ `------------------'   `--^
 */

Arena arena(void)
{
	Arena a = {};
	return a;
}

static Zone *addzone(Arena *a, uW zsize)
{
	uW allocsize = ALIGNUP(zsize + sizeof(Zone), PAGESIZE);
	Zone *z = pagemap(allocsize);
	if (!z)
		return 0;
	/* NOTE: alignment likely increased the capacity, recalculate */
	z->free = allocsize - sizeof(Zone);
	z->mem = z + 1;
	z->next = 0;
	if (a->tail) {
		a->tail->next = z;
	} else {
		a->head = z;
		a->tail = z;
	}
	return z;
}

void *aralloca(Arena *a, uW size, uW align)
{
	uW asize = (size + align*2);
	Zone *z = a->head;
	while (z) {
		if (z->free >= asize)
			break;
		z = z->next;
	}
	if (!z) {
		/* NOTE: Why multiply? Well, the idea is that we expect
		 * the user to maybe allocate some more objects of the same size.
		 * Why 16? I just think it kind of makes sense: for small objects (<< PAGESIZE)
		 * this doesn't matter since we round up to the page size anyway,
		 * for large objects (~PAGESIZE) I suspect that 16 should be enough in most cases. */
		z = addzone(a, asize * 16);
		if (!z)
			return 0;
	}
	z->free -= asize;
	z->mem += asize;
	return (void *)ALIGNUP((uW)z->mem, align);
}

void *aralloc(Arena *a, uW size)
{
	return aralloca(a, size, sizeof(uW));
}

void arclear(Arena *a)
{
	for (Zone *z = a->head; z; z = z->next) {
		z->free += (uW)z->mem - (uW)(z + 1);
		z->mem = z + 1;
	}
}

typedef struct Segment Segment;
typedef struct Chunk Chunk;

/* TODO: use crc or something for metadata corruption detection */
struct Segment {
	uW size : sizeof(uW)*8 - 1; /* in sizeof(Segment), for convenience */
	uW free : 1;
	Segment *next;
	Segment *prev;
	Chunk *header;
};

#define SEGRIGHT(l) ((l) + (l)->size + 1)
#define SEGLEFT(r) ((r) - (r)->size - 1)

static void seginit(Segment *s, uW size, Chunk *header)
{
	s->size = size;
	Segment *r = SEGRIGHT(s);
	r->size = size;
	s->header = r->header = header;
}

static Segment *segsplit(Segment *s, uW size)
{
	uW oldsize = s->size;
	Chunk *header = s->header;
	Segment *o = s + size + 2;
	seginit(s, size, header);
	seginit(o, oldsize - size - 2, header);
	return o;
}

static Segment *segmerge(Segment *s1, Segment *s2)
{
	if (s1 > s2)
		SWAP(s1, s2);
	seginit(s1, s1->size + s2->size + 2, s1->header);
	return s1;
}

/* TODO: use crc or something for metadata corruption detection */
struct Chunk {
	uW size; /* in sizeof(Segment) */
	Segment *free;
	Segment *busy;
	Chunk *next;
};

#define FIRSTSEG(c) ((Segment *)((c) + 1))
#define LASTSEG(c) (FIRSTSEG(c) + (c)->size - 1)

static void seglink(Segment *s, uW free)
{
	Segment *r = SEGRIGHT(s);
	Segment **q = free ? &s->header->free : &s->header->busy;
	s->free = r->free = BOOL(free);
	s->next = r->next = *q;
	s->prev = r->prev = 0;
	if (*q)
		(*q)->prev = s;
	(*q) = s;
}

static void segunlink(Segment *s)
{
	Segment **q = s->free ? &s->header->free : &s->header->busy;
	Segment *p = s->prev;
	Segment *n = s->next;
	if (p)
		p->next = n;
	else
		*q = n;
	if (n)
		n->prev = p;
}

static Segment *segladjacent(Segment *s)
{
	if (s == FIRSTSEG(s->header))
		return 0;
	return SEGLEFT(s - 1);
}

static Segment *segradjacent(Segment *s)
{
	if (SEGRIGHT(s) == LASTSEG(s->header))
		return 0;
	return SEGRIGHT(s) + 1;
}

/* TODO: implement a set of cache allocators for small allocations (< ~128 bytes)
 * TODO: implement a page allocator for huge ones (> ~16 MIB) */
typedef struct {
	Chunk *chunks;
} Heap;

static Chunk *addchunk(Heap *h, uW segcount)
{
	uW allocsize = ALIGNUP(segcount*sizeof(Segment) + sizeof(Chunk), PAGESIZE);
	Chunk *c = pagemap(allocsize);
	if (!c)
		return 0;
	/* NOTE: alignment likely increased the capacity, recalculate */
	c->size = (allocsize - sizeof(Chunk))/sizeof(Segment);
	c->next = h->chunks;
	c->busy = 0;
	c->free = 0;
	/* NOTE: the chunk is initialized, but not linked */
	seginit(FIRSTSEG(c), c->size - 2, c);
	h->chunks = c;
	return c;
}

static Heap h; /* TODO: if I'll do multithreading, this should become a thread-local */

/*
 * The layout of a segment in memory:
 *
 *          .-- Two copies of the segment descriptor --.
 *  ________V________ _____ _ _.______.....___ ________V_________
 * |                 |     | | .              |                  |
 * | Left descriptor |     | | .              | Right descriptor |
 * |    (Segment)    |     | | .              |    (Segment)     |
 * '_________________'_____'_'_.______.....___'__________________'
 * ^                        |  ^
 *  `---- Back pointer -----'  `---- Return pointer
 *   (The back pointer is located at the closest word-byte aligned memory
 *    slot to the left of the return pointer)
 *
 * The layout of a heap in memory:
 *     .--------.
 *     | chunks | (Heap)
 *     '--------'
 *         |                    Note that segments also form a doubly-linked list
 *   .-----|-------------.
 *   '  ___v____________ v____________ _____________ _    __ _____________        ________________ _
 *   ' |                |             |             |       |             |      |                |
 *   ' | Chunk header 1 |             |             |       |             |      |                |
 *   ' |    (Chunk)     |             |             |       |             |      |                |
 *   '-|---- free       |  Segment 1  |  Segment 2  |  ...  |  Segment n  |      | Chunk header 2 | ...
 * .---|-----next       |             |             |       |             |      |                |
 * ' .-|---- busy       |             |             |       |             |      |                |
 * ' \ '________________'_____________'_____________'_    __'_____________'      '________________'_
 * '  `--------------------------------^                                          ^
 * '------------------------------------------------------------------------------'
 */

#define BACKPTR(a) ((void **)(ALIGNDOWN((uW)a, sizeof(uW)) - sizeof(uW)))

void *segaddr(Segment *s, uW align)
{
	uW addr = ALIGNUP((uW)(s + 1) + sizeof(uW), align);
	*BACKPTR(addr) = s;
	return (void *)addr;
}

void *memalloca(uW size, uW align)
{
	uW asize = DIVCEIL(size + align*2 + sizeof(uW), sizeof(Segment));
	Segment *s = 0;
	for (Chunk *c = h.chunks; c && !s; c = c->next) {
		for (s = c->free; s; s = s->next) {
			if (s->size >= asize)
				break;
		}
	}
	if (s) {
		segunlink(s);
	} else {
		/* NOTE: preallocation logic is the same as in aralloca */
		Chunk *c = addchunk(&h, asize * 16);
		if (!c)
			return 0;
		s = FIRSTSEG(c);
	}
	if (s->size - asize > 4) {
		Segment *o = segsplit(s, asize);
		seglink(o, 1);
	}
	seglink(s, 0);
	return segaddr(s, align);
}

void *memalloc(uW size)
{
	return memalloca(size, sizeof(uW));
}

void memfree(void *p)
{
	if (!p)
		return;
	Segment *s = *BACKPTR(p);
	segunlink(s);
	for (;;) {
		Segment *n = segladjacent(s);
		if (!n)
			n = segradjacent(s);
		if (!n || !n->free)
			break;
		segunlink(n);
		s = segmerge(s, n); /* heap defragmentation */
	}
	seglink(s, 1);
}

static void *memtryextend(void *p, uW size, uW align)
{
	if (!p)
		return 0;
	Segment *s = *BACKPTR(p);
	uW asize = DIVCEIL(size + align*2 + sizeof(uW), sizeof(Segment));
	if (s->size >= asize)
		return segaddr(s, align);
	Segment *r = segradjacent(s);
	if (!r || !r->free)
		return 0;
	segunlink(s);
	segunlink(r);
	/* NOTE: a->size + b->size + 2 == segmerge(a, b)->size */
	if ((r->size + s->size + 2) - asize > 4) {
		Segment *o = segsplit(r, asize - s->size - 2);
		seglink(o, 1);
	}
	s = segmerge(s, r);
	seglink(s, 0);
	return segaddr(s, align);
}

static void memtransfer(void *dst, void *src)
{
	Segment *s = *BACKPTR(src);
	Segment *d = *BACKPTR(dst);
	u8 *sc = src, *dc = dst;
	while (sc < (u8 *)SEGRIGHT(s) && dc < (u8 *)SEGRIGHT(d)) {
		*dc = *sc;
		sc += 1;
		dc += 1;
	}
}

void *memrealloca(void *p, uW size, uW align)
{
	if (!p)
		return memalloca(size, align);
	void *o = memtryextend(p, size, align);
	if (o)
		return o;
	o = memalloca(size, align);
	memtransfer(o, p);
	memfree(p);
	return o;
}

void *memrealloc(void *p, uW size)
{
	return memrealloca(p, size, sizeof(uW));
}

void *memallocarray(uW n, uW size)
{
	return memalloc(n*size);
}

void *memreallocarray(void *p, uW n, uW size)
{
	return memrealloc(p, n*size);
}
