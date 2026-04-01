#define KIB ((U64)1024)
#define MIB (1024*KIB)
#define GIB (1024*MIB)

/* NOTE: alloc functions without 'a' suffix return word-aligned pointers */
/* TODO: maybe use natural alignment instead? */

/* TODO: use crc or something for metadata corruption detection */
typedef struct Zone Zone;
struct Zone {
	U free;  /* in bytes */
	void *mem;
	Zone *next;
};

/* TODO: maybe an arena should have several predefined zone sizes?
 * (like a list of 16K zones, a list of 16M and maybe a list of 1G) */
typedef struct {
	Zone *head;
	Zone *tail;
} Arena;

void *aralloc(Arena *a, U size);
void *aralloca(Arena *a, U size, U align);
void arclear(Arena *a);
void arfree(Arena *a);

/* TODO: I actually have a really cool idea, I can try to integrate
 * reference counting into the allocator implementation and introduce, say,
 * memref function that would increase the reference count, while memfree
 * would decrease it */
void *memalloca(U size, U align);
void *memalloc(U size);
void *memrealloca(void *p, U size, U align);
void *memrealloc(void *p, U size);
void memfree(void *p);
