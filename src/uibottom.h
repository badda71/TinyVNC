/*
 * TinyVNC - A VNC client for Nintendo 3DS
 *
 * uibottom.h - functions for handling 3DS bottom screen
 *
 * Copyright 2020 Sebastian Weber
 */


#ifndef UAE_UIBOTTOM_H
#define UAE_UIBOTTOM_H

// some colors for writing text
#define COL_WHITE (SDL_Color){0xff, 0xff, 0xff, 0xff}
#define COL_BLACK (SDL_Color){0, 0, 0, 0xff}
#define COL_RED (SDL_Color){0xff, 0, 0, 0xff}
#define COL_GREEN (SDL_Color){0, 0xff, 0, 0xff}
#define COL_BLUE (SDL_Color){0, 0, 0xff, 0xff}
#define COL_MAKE(x,y,z) (SDL_Color){x, y, z, 0xff}

#define MIN(a,b) \
	({ __typeof__ (a) _a = (a); \
		__typeof__ (b) _b = (b); \
		_a < _b ? _a : _b; })

#define MAX(a,b) \
	({ __typeof__ (a) _a = (a); \
		__typeof__ (b) _b = (b); \
		_a > _b ? _a : _b; })

#define LIMIT(a,l1,l2) \
	({ __typeof__ (a) _a = (a); \
		__typeof__ (l1) _l1 = (l1); \
		__typeof__ (l2) _l2 = (l2); \
		_a < _l1 ? _l1 : (_a > _l2 ? _l2 : _a); })

// exposed definitions
typedef enum {
	UIB_NO = 0,
	UIB_REPAINT = 1,
	UIB_RECALC_KEYPRESS = 2,
	UIB_RECALC_MENU = 4,
	UIB_RECALC_VNC = 8
} uib_action;

typedef struct {
	int x,y,w,h,key,shftkey,sticky,repeat;
	const char *name;
} uikbd_key;

typedef enum {
	ALIGN_LEFT,
	ALIGN_RIGHT,
	ALIGN_CENTER
} str_alignment;

// exposed functions
extern void make_key_event_norepeat(SDL_Event *e, int sym, int pressed);
extern void make_key_event(SDL_Event *e, int sym, int pressed);
extern void push_key_event(int sym, int pressed);
extern void push_key_event_norepeat(int sym, int pressed);
extern void checkKeyRepeat();

extern void uib_printtext(SDL_Surface *s, const char *str, int xo, int yo, int w, int h, SDL_Color tcol, SDL_Color bcol);
extern void uib_printstring(SDL_Surface *s, const char *str, int x, int y, int maxchars, str_alignment align, SDL_Color tcol, SDL_Color bcol);
extern int uib_printf(char *format, ...);
extern int uib_vprintf(char *format, va_list arg);
extern void uib_set_position(int x, int y);
extern void uib_set_colors(SDL_Color text, SDL_Color background);
extern void uib_reset_colors();
extern void uib_invert_colors();
extern void uib_clear();
extern SDL_Surface *myIMG_Load(char *fname);


extern void uib_update(int what);
extern int uib_handle_event(SDL_Event *, int taphandling);
extern void uib_init();
extern int uib_handle_tap_processing(SDL_Event *e);
extern void uib_enable_keyboard(int enable);
extern void uib_show_scrollbars(int x, int y, int w, int h);
#define SCROLLBAR_WIDTH 2
extern int uib_getBacklight();
extern void uib_setBacklight (int on);

extern rfbBool uibvnc_resize(rfbClient*);
extern void uibvnc_cleanup();
extern void uibvnc_dirty();

// exposed variables
extern uikbd_key uikbd_keypos[];
extern uib_action uib_must_redraw;
#endif