#define IOBUFSIZE 16384

typedef struct {
	int fd;
	U8  bytes[IOBUFSIZE];
	U64 i, count, pos;
	U8  error, mode;
} IOBuffer;

extern IOBuffer *bin;
extern IOBuffer *bout;
extern IOBuffer *berr;

OK bopen(IOBuffer *b, const char *path, U8 mode);
OK bclose(IOBuffer *b);

I  bread(IOBuffer *b);
I  bpeek(IOBuffer *b);
OK bseek(IOBuffer *b, U64 byte);
OK bwrite(IOBuffer *b, U8 v);
OK bflush(IOBuffer *b);

#define _INTFMT(type) ((U)(ISUNSIGNED(type)<<8 | sizeof(type)))

#define _FMTEND (U)0, (U)0, (U)0 /* End of arguments, isn't supposed to be used explicitly */

#define OD(v) (U)0, _INTFMT(typeof(v)), (U)10, (U64)(v)
#define OH(v) (U)0, _INTFMT(typeof(v)), (U)16, (U64)(v)
#define OB(v) (U)0, _INTFMT(typeof(v)), (U)2,  (U64)(v)

OK _bprint(IOBuffer *b, ...);

#define bprint(b, ...) _bprint(b, __VA_ARGS__, _FMTEND)
#define bprintln(b, ...) bprint(b, __VA_ARGS__, "\n")

#define print(...) bprint(bout, __VA_ARGS__)
#define println(...) bprintln(bout, __VA_ARGS__)
#define eprint(...) bprint(berr, __VA_ARGS__)
#define eprintln(...) bprintln(berr, __VA_ARGS__)

#define ID(v)  (U)0, _INTFMT(typeof(*(v))), (U)(v)
#define IWS    (U)0, (U)0, (U)-1
#define IWS1   (U)0, (U)0, (U)1

OK _binput(IOBuffer *b, ...);

#define binput(b, ...) _binput(b, __VA_ARGS__, _FMTEND)
#define binputln(b, ...) binput(b, __VA_ARGS__, "\n")

#define input(...) binput(bin, __VA_ARGS__)
#define inputln(...) binputln(bin, __VA_ARGS__)

#define ETRACE(x, fmt) ({\
	typeof(x) _tmp = (x);\
	println("trace: ", #x, " = ", fmt(_tmp));\
	_tmp;\
})
