void _fdprint(int fd, ...);

#define FMTB(x)   0, 'b', (U)(x) /* Binary */
#define FMTX(x)   0, 'h', (U)(x) /* Hexidecimal */
#define FMTU(x)   0, 'u', (U)(x) /* Unsigned integers */
#define FMTI(x)   0, 'i', (U)(x) /* Signed integers */
#define FMTEND()  0, 0           /* End of arguments, isn't supposed to be used explicitly */

#define print(...) _fdprint(1, __VA_ARGS__, FMTEND())
#define println(...) print(__VA_ARGS__, "\n")
#define eprint(...) _fdprint(2, __VA_ARGS__, FMTEND())
#define eprintln(...) eprint(__VA_ARGS__, "\n")
