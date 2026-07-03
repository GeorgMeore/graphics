typedef struct {
	U64 r[3+4+1]; /* rbx, rsp, rbp, r12-r15, rip */
} Jump;

extern U64  save(Jump *j);
extern void jump(Jump *j, U64 v);
