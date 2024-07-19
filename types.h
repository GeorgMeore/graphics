/* TODO: maybe some ifdefs? Or at least tests that verify the size assumtions. */
typedef unsigned char  u8;
typedef signed char    s8;
typedef unsigned short u16;
typedef unsigned       s16;
typedef unsigned       u32;
typedef int            s32;
typedef unsigned long  u64;
typedef long           s64;
typedef u64            uW; /* the machine address size */

#define CARD(t) (2<<(sizeof(t)*8))
