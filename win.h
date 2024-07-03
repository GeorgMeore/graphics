extern Image fb;

void winopen(u16 w, u16 h, const char *title);
void winpoll(void);
void winclose(void);
int keyisdown(u8 k);
