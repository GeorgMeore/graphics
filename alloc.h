#define KIB 1024
#define MIB (1024 * KIB)
#define GIB (1024 * MIB)

typedef struct Zone Zone;

/* TODO: maybe an arena should have several predefined zone sizes?
 * (like a list of 16K zones, a list of 16M and maybe a list of 1G) */
typedef struct {
	Zone *head;
	Zone *tail;
} Arena;

Arena arena(void);
void *aralloc(Arena *a, uW size);
void *aralloca(Arena *a, uW size, uW align);
void arclear(Arena *a);

void *memalloca(uW size, uW align);
void *memalloc(uW size);
void memfree(void *p);
