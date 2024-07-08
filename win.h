void   winopen(u16 w, u16 h, const char *title, u16 fps);
void   winclose(void);
Image* framebegin(void);
void   frameend(void);
int    keyisdown(u8 k);
int    btnisdown(u8 k);
