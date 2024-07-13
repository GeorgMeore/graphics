typedef enum {
	FU,   /* Unsigned integers */
	FS,   /* Signed integers */
	FEND, /* End of format, isn't supposed to be used explicitly */
} Format;

void _fdprint(int fd, ...);

#define FU(x)   0, FU, (u64)(x)
#define FS(x)   0, FS, (s64)(x)
#define FEND()  0, FEND

#define print(...) _fdprint(1, __VA_ARGS__, FEND())
#define println(...) print(__VA_ARGS__, "\n")
#define eprint(...) _fdprint(2, __VA_ARGS__, FEND())
#define eprintln(...) eprint(__VA_ARGS__, "\n")
