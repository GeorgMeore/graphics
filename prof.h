void profbegin(const char *name);
void profend(void);

#ifdef PROFENABLED
#define PROFBEGIN(name) profbegin(name)
#define PROFEND() profend()
#else
#define PROFBEGIN(name)
#define PROFEND()
#endif
