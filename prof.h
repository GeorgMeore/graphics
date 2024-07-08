void profbegin(const char *name);
void profend(void);
void profdump(void);

#ifdef PROFENABLED
#define PROFBEGIN(...) profbegin(__VA_ARGS__)
#define PROFEND(...)   profend(__VA_ARGS__)
#define PROFDUMP(...)  profdump(__VA_ARGS__)
#else
#define PROFBEGIN(...)
#define PROFEND(...)
#define PROFDUMP(...)
#endif
