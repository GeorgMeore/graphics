/* NOTE: Two's complement integer representation is assumed. */

/* TODO: ifdefs for other platforms. */
typedef unsigned char  U8;
typedef signed char    I8;
typedef unsigned short U16;
typedef short          I16;
typedef unsigned       U32;
typedef int            I32;
typedef unsigned long  U64;
typedef long           I64;
typedef U64            U; /* the machine address size */
typedef I64            I; /* the machine address size */

#define ISUNSIGNED(t) ((t)(-1) > 0)
#define ISSIGNED(t) (!ISUNSIGNED(t))
#define NBITS(t) (sizeof(t)*8)
#define MINVAL(t) ((t)((U64)ISSIGNED(t)<<(NBITS(t) - 1)))
#define MAXVAL(t) ((t)~MINVAL(t))

typedef float  F32;
typedef double F64;

#define ISFLOAT(t) ((t)1.1 != (t)1)
