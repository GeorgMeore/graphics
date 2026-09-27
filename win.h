typedef enum {
	KeyGrave     = (U64)1 << 0,
	Key0         = (U64)1 << 1,
	Key1         = (U64)1 << 2,
	Key2         = (U64)1 << 3,
	Key3         = (U64)1 << 4,
	Key4         = (U64)1 << 5,
	Key5         = (U64)1 << 6,
	Key6         = (U64)1 << 7,
	Key7         = (U64)1 << 8,
	Key8         = (U64)1 << 9,
	Key9         = (U64)1 << 10,
	KeyMinus     = (U64)1 << 11,
	KeyEqual     = (U64)1 << 12,
	KeyA         = (U64)1 << 13,
	KeyB         = (U64)1 << 14,
	KeyC         = (U64)1 << 15,
	KeyD         = (U64)1 << 16,
	KeyE         = (U64)1 << 17,
	KeyF         = (U64)1 << 18,
	KeyG         = (U64)1 << 19,
	KeyH         = (U64)1 << 20,
	KeyI         = (U64)1 << 21,
	KeyJ         = (U64)1 << 22,
	KeyK         = (U64)1 << 23,
	KeyL         = (U64)1 << 24,
	KeyM         = (U64)1 << 25,
	KeyN         = (U64)1 << 26,
	KeyO         = (U64)1 << 27,
	KeyP         = (U64)1 << 28,
	KeyQ         = (U64)1 << 29,
	KeyR         = (U64)1 << 30,
	KeyS         = (U64)1 << 31,
	KeyT         = (U64)1 << 32,
	KeyU         = (U64)1 << 33,
	KeyV         = (U64)1 << 34,
	KeyW         = (U64)1 << 35,
	KeyX         = (U64)1 << 36,
	KeyY         = (U64)1 << 37,
	KeyZ         = (U64)1 << 38,
	KeyLBracket  = (U64)1 << 39,
	KeyRBracket  = (U64)1 << 40,
	KeyBSlash    = (U64)1 << 41,
	KeySemicolon = (U64)1 << 42,
	KeyDQuote    = (U64)1 << 43,
	KeyComa      = (U64)1 << 44,
	KeyDot       = (U64)1 << 45,
	KeySlash     = (U64)1 << 46,
	KeyTab       = (U64)1 << 47,
	KeyCaps      = (U64)1 << 48,
	KeyLShift    = (U64)1 << 49,
	KeyLCtrl     = (U64)1 << 50,
	KeyLWin      = (U64)1 << 51,
	KeyLAlt      = (U64)1 << 52,
	KeySpace     = (U64)1 << 53,
	KeyRAlt      = (U64)1 << 54,
	KeyRWin      = (U64)1 << 55,
	KeyRCtrl     = (U64)1 << 56,
	KeyRShift    = (U64)1 << 57,
	KeyEnter     = (U64)1 << 58,
	KeyBackspace = (U64)1 << 59,
} Key;

typedef enum {
	BtnLeft   = 1 << 0,
	BtnRight  = 1 << 1,
	BtnMiddle = 1 << 2,
	BtnUp     = 1 << 3,
	BtnDown   = 1 << 4,
} Btn;

void   winopen(U16 w, U16 h, const char *title, U16 fps);
Image* frame(void);
void   flush(void);
U64    lastframetime(void);
OK     keyisdown(Key k);
OK     keywaspressed(Key k);
OK     btnisdown(Btn b);
OK     btnwaspressed(Btn b);
void   mouselock(OK on);
I      mousex(void);
I      mousey(void);
