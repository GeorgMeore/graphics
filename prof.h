void _profbegin(const char *name);
void _profend(void);
void _profdump(void);
void _profreset(const char *name);

#ifdef PROFENABLED
#define profbegin(...) _profbegin(__VA_ARGS__)
#define profend(...)   _profend(__VA_ARGS__)
#define profdump(...)  _profdump(__VA_ARGS__)
#define profreset(...)  _profreset(__VA_ARGS__)
#else
#define profbegin(...)
#define profend(...)
#define profdump(...)
#define profreset(...)
#endif
