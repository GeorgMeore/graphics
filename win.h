void   winopen(u16 w, u16 h, const char *title, u16 fps);
void   winclose(void);
Image* framebegin(void);
void   frameend(void);
u64    lastframetime(void);
int    keyisdown(u8 k);
int    keywaspressed(u8 k);
int    btnisdown(u8 b);
int    btnwaspressed(u8 b);
void   mouselock(int on);
int    mousex(void);
int    mousey(void);
