/*
 * TODO: research more on the topic of signed/unsigned conversions
 * on integers of different sizes and stuff. And maybe add overflow
 * checks during integer parsing.
 */

#define _INTFMT(type) (ISUNSIGNED(type)<<8 | sizeof(type))

#define FMTEND (U)0, 0 /* End of arguments, isn't supposed to be used explicitly */

#define OD(v)  (U)0, _INTFMT(typeof(v)), 10, (U64)v
#define OH(v)  (U)0, _INTFMT(typeof(v)), 16, (U64)v
#define OB(v)  (U)0, _INTFMT(typeof(v)), 2,  (U64)v

void _fdprint(int fd, ...);

#define I(v)   (U)0, _INTFMT(typeof(*v)), (U)v

#define print(...) _fdprint(1, __VA_ARGS__, FMTEND)
#define println(...) print(__VA_ARGS__, "\n")
#define eprint(...) _fdprint(2, __VA_ARGS__, FMTEND)
#define eprintln(...) eprint(__VA_ARGS__, "\n")

int _fdinputln(int fd, ...);

#define inputln(...) _fdinputln(0, __VA_ARGS__, FMTEND)