#define SWAP(x, y) ({ typeof(x) tmp; tmp = (x); (x) = (y); (y) = tmp; })

u64 timens(void);
void sleepns(u64 t);
