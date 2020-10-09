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

// exposed definitions
typedef enum {
	UIB_NO = 0,
	UIB_REPAINT = 1
} uib_action;

typedef struct {
	int x,y,w,h,key,sticky,flags;
	const char *name;
} uikbd_key;

typedef enum {
	ALIGN_LEFT,
	ALIGN_RIGHT,
	ALIGN_CENTER
} str_alignment;

// exposed functions
extern void uib_printtext(SDL_Surface *s, const char *str, int xo, int yo, int w, int h, SDL_Color tcol, SDL_Color bcol);
extern void uib_printstring(SDL_Surface *s, const char *str, int x, int y, int maxchars, str_alignment align, SDL_Color tcol, SDL_Color bcol);
extern int uib_printf(char *format, ...);
extern int uib_vprintf(char *format, va_list arg);
extern void uib_set_position(int x, int y);
extern void uib_set_colors(SDL_Color text, SDL_Color background);
extern void uib_reset_colors();
extern void uib_invert_colors();
extern void uib_clear();

extern void uib_update(void);
extern int uib_handle_event(SDL_Event *);
extern void uib_init();
extern int uib_handle_tap_processing(SDL_Event *e);

// exposed variables
extern uikbd_key uikbd_keypos[];
extern uib_action uib_must_redraw;
#endif
