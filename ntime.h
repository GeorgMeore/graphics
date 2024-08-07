#define USEC 1000
#define MSEC (1000*USEC)
#define SEC  (1000*MSEC)

U64  timens(void);
void sleepns(U64 t);
