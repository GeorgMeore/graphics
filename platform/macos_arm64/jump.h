typedef struct {
	U64 r[12+8+1]; /* x19-x30, d8-d15, sp */
} Jump;

extern U64  save(Jump *j);
extern void jump(Jump *j, U64 v);
