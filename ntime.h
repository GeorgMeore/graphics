#define USEC 1000
#define MSEC (1000*USEC)
#define SEC  (1000*MSEC)

u64  timens(void);
void sleepns(u64 t);
