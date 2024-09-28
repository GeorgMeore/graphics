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

#define ISUNSIGNED(t) ((t)0 - 1 > 0)
#define CARD(t) (2<<(sizeof(t)*8))
