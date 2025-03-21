#define IOBUFSIZE 4096

typedef struct {
	int fd;
	U8  bytes[IOBUFSIZE];
	int i, count;
	int error;
} IOBuffer;

extern IOBuffer *bin;
extern IOBuffer *bout;
extern IOBuffer *berr;

int bread(IOBuffer *b);
int bpeek(IOBuffer *b);
int bwrite(IOBuffer *b, U8 v);
int bflush(IOBuffer *b);

#define _INTFMT(type) (ISUNSIGNED(type)<<8 | sizeof(type))

#define FMTEND (U)0, 0 /* End of arguments, isn't supposed to be used explicitly */

#define OD(v) (U)0, _INTFMT(typeof(v)), 10, (U64)(v)
#define OH(v) (U)0, _INTFMT(typeof(v)), 16, (U64)(v)
#define OB(v) (U)0, _INTFMT(typeof(v)), 2,  (U64)(v)

int _bprint(IOBuffer *b, ...);

#define bprint(b, ...) _bprint(b, __VA_ARGS__, FMTEND)
#define bprintln(b, ...) bprint(b, __VA_ARGS__, "\n")

#define print(...) bprint(bout, __VA_ARGS__)
#define println(...) bprintln(bout, __VA_ARGS__)
#define eprint(...) bprint(berr, __VA_ARGS__)
#define eprintln(...) bprintln(berr, __VA_ARGS__)

#define ID(v)  (U)0, _INTFMT(typeof(*(v))), (U)(v)
#define IWS    "", (U)-1
#define IWS1   "", (U)1

int _binput(IOBuffer *b, ...);

#define binput(b, ...) _binput(b, __VA_ARGS__, FMTEND)
#define binputln(b, ...) binput(b, __VA_ARGS__, "\n")

#define input(...) binput(bin, __VA_ARGS__)
#define inputln(...) binputln(bin, __VA_ARGS__)
