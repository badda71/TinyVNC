/*
 * TinyVNC - A VNC client for Nintendo 3DS
 *
 * uibottom.c - functions for handling 3DS bottom screen
 *
 * Copyright 2020 Sebastian Weber
 */
 
#include <stdlib.h>
#include <png.h>
#include <SDL/SDL.h>
#include <3ds.h>
#include <citro3d.h>
#include <rfb/rfbclient.h>
#include "uibottom.h"
#include "utilities.h"

#define ENTER //log_citra("enter %s",__func__);
#define DEF_TXT_COL COL_WHITE
#define DEF_BCK_COL COL_BLACK

uikbd_key uikbd_keypos[] = {
	//  x,  y,   w,   h,            key,      shftkey,stky, repeat, name
	// toggle kb button
	{ 284,-15,  36,  15,              2,            0,   0,      0, "ToggleKB"},
	// 1st Row
	{   0,  0,  20,  20,      XK_Escape,            0,   0,      1, "ESC"},
	{  20,  0,  20,  20,          XK_F1,            0,   0,      1, "F1"},
	{  40,  0,  20,  20,          XK_F2,            0,   0,      1, "F2"},
	{  60,  0,  20,  20,          XK_F3,            0,   0,      1, "F3"},
	{  80,  0,  20,  20,          XK_F4,            0,   0,      1, "F4"},
	{ 100,  0,  20,  20,          XK_F5,            0,   0,      1, "F5"},
	{ 120,  0,  20,  20,          XK_F6,            0,   0,      1, "F6"},
	{ 140,  0,  20,  20,          XK_F7,            0,   0,      1, "F7"},
	{ 160,  0,  20,  20,          XK_F8,            0,   0,      1, "F8"},
	{ 180,  0,  20,  20,          XK_F9,            0,   0,      1, "F9"},
	{ 200,  0,  20,  20,         XK_F10,            0,   0,      1, "F10"},
	{ 220,  0,  20,  20,         XK_F11,            0,   0,      1, "F11"},
	{ 240,  0,  21,  20,         XK_F12,            0,   0,      1, "F12"},
	{ 299,  0,  21,  20,      XK_Insert,            0,   0,      1, "Insert"},
	// 2nd Row
	{   0, 20,  20,  20,       XK_grave,XK_asciitilde,   0,      1, "~"},
	{  20, 20,  20,  20,           XK_1,    XK_exclam,   0,      1, "1"},
	{  40, 20,  20,  20,           XK_2,        XK_at,   0,      1, "2"},
	{  60, 20,  20,  20,           XK_3,XK_numbersign,   0,      1, "3"},
	{  80, 20,  20,  20,           XK_4,    XK_dollar,   0,      1, "4"},
	{ 100, 20,  20,  20,           XK_5,   XK_percent,   0,      1, "5"},
	{ 120, 20,  20,  20,           XK_6,XK_asciicircum,  0,      1, "6"},
	{ 140, 20,  20,  20,           XK_7, XK_ampersand,   0,      1, "7"},
	{ 160, 20,  20,  20,           XK_8,  XK_asterisk,   0,      1, "8"},
	{ 180, 20,  20,  20,           XK_9, XK_parenleft,   0,      1, "9"},
	{ 200, 20,  20,  20,           XK_0,XK_parenright,   0,      1, "0"},
	{ 220, 20,  20,  20,       XK_minus,XK_underscore,   0,      1, "-"},
	{ 240, 20,  20,  20,       XK_equal,      XK_plus,   0,      1, "="},
	{ 260, 20,  31,  20,   XK_BackSpace,            0,   0,      1, "Backspace"},
	{ 299, 20,  21,  20,      XK_Delete,            0,   0,      1, "Delete"},
	// 3rd Row
	{   0, 40,  30,  20,         XK_Tab,            0,   0,      1, "TAB"},
	{  30, 40,  20,  20,           XK_q,         XK_Q,   0,      1, "Q"},
	{  50, 40,  20,  20,           XK_w,         XK_W,   0,      1, "W"},
	{  70, 40,  20,  20,           XK_e,         XK_E,   0,      1, "E"},
	{  90, 40,  20,  20,           XK_r,         XK_R,   0,      1, "R"},
	{ 110, 40,  20,  20,           XK_t,         XK_T,   0,      1, "T"},
	{ 130, 40,  20,  20,           XK_y,         XK_Y,   0,      1, "Y"},
	{ 150, 40,  20,  20,           XK_u,         XK_U,   0,      1, "U"},
	{ 170, 40,  20,  20,           XK_i,         XK_I,   0,      1, "I"},
	{ 190, 40,  20,  20,           XK_o,         XK_O,   0,      1, "O"},
	{ 210, 40,  20,  20,           XK_p,         XK_P,   0,      1, "P"},
	{ 230, 40,  20,  20, XK_bracketleft, XK_braceleft,   0,      1, "["},
	{ 250, 40,  20,  20,XK_bracketright,XK_braceright,   0,      1, "]"},
	{ 270, 40,  20,  20,   XK_backslash,       XK_bar,   0,      1, "\\"},
	{ 299, 40,  21,  20,        XK_Home,            0,   0,      1, "Home"},
	// 4th Row
	{   0, 60,  40,  20,     XK_Shift_L,            0,   1,      0, "CAPS"},
	{  40, 60,  20,  20,           XK_a,         XK_A,   0,      1, "A"},
	{  60, 60,  20,  20,           XK_s,         XK_S,   0,      1, "S"},
	{  80, 60,  20,  20,           XK_d,         XK_D,   0,      1, "D"},
	{ 100, 60,  20,  20,           XK_f,         XK_F,   0,      1, "F"},
	{ 120, 60,  20,  20,           XK_g,         XK_G,   0,      1, "G"},
	{ 140, 60,  20,  20,           XK_h,         XK_H,   0,      1, "H"},
	{ 160, 60,  20,  20,           XK_j,         XK_J,   0,      1, "J"},
	{ 180, 60,  20,  20,           XK_k,         XK_K,   0,      1, "K"},
	{ 200, 60,  20,  20,           XK_l,         XK_L,   0,      1, "L"},
	{ 220, 60,  20,  20,   XK_semicolon,     XK_colon,   0,      1, ";"},
	{ 240, 60,  20,  20,  XK_apostrophe,  XK_quotedbl,   0,      1, "'"},
	{ 260, 60,  31,  20,      XK_Return,            0,   0,      1, "ENTER"},
	{ 299, 60,  21,  20,         XK_End,            0,   0,      1, "End"},
	// 5th Row
	{   0, 80,  50,  20,     XK_Shift_L,            0,   1,      0, "LSHIFT"},
	{  50, 80,  20,  20,           XK_z,         XK_Z,   0,      1, "Z"},
	{  70, 80,  20,  20,           XK_x,         XK_X,   0,      1, "X"},
	{  90, 80,  20,  20,           XK_c,         XK_C,   0,      1, "C"},
	{ 110, 80,  20,  20,           XK_v,         XK_V,   0,      1, "V"},
	{ 130, 80,  20,  20,           XK_b,         XK_B,   0,      1, "B"},
	{ 150, 80,  20,  20,           XK_n,         XK_N,   0,      1, "N"},
	{ 170, 80,  20,  20,           XK_m,         XK_M,   0,      1, "M"},
	{ 190, 80,  20,  20,       XK_comma,      XK_less,   0,      1, ","},
	{ 210, 80,  20,  20,      XK_period,   XK_greater,   0,      1, "."},
	{ 230, 80,  20,  20,       XK_slash,  XK_question,   0,      1, "/"},
	{ 250, 80,  20,  20,          XK_Up,            0,   0,      1, "C_UP"},
	{ 270, 80,  21,  20,     XK_Shift_R,            0,   1,      0, "RSHIFT"},
	{ 299, 80,  21,  20,     XK_Page_Up,            0,   0,      1, "PG_UP"},
	// 6th row
	{   0,100,  20,  20,   XK_Control_L,            0,   2,      0, "LCTRL"},
	{  20,100,  20,  20,     XK_Super_L,            0,   4,      0, "WIN"},
	{  40,100,  20,  20,       XK_Alt_L,            0,   8,      0, "LALT"},
	{  60,100, 130,  20,       XK_space,            0,   0,      1, "Space"},
	{ 190,100,  20,  20,       XK_Alt_R,            0,   8,      0, "RALT"},
	{ 210,100,  20,  20,   XK_Control_R,            0,   2,      0, "RCTRL"},
	{ 230,100,  20,  20,        XK_Left,            0,   0,      1, "C_LEFT"},
	{ 250,100,  20,  20,        XK_Down,            0,   0,      1, "C_DOWN"},
	{ 270,100,  21,  20,       XK_Right,            0,   0,      1, "C_RIGHT"},
	{ 299,100,  21,  20,   XK_Page_Down,            0,   0,      1, "PG_DOWN"},
	// Finish
	{   0,  0,   0,   0,             -1,            0,   0,      0, ""}
};

extern void log_citra(const char *format, ...);

// data definitions
typedef struct {
	unsigned w;
	unsigned h;
	float fw,fh;
	C3D_Tex tex;
} DS3_Image;

// exposed variables
uib_action uib_must_redraw = UIB_NO;

// static sprites
//static DS3_Image background_spr;
static DS3_Image kbd_spr;
static DS3_Image twistyup_spr;
static DS3_Image twistydn_spr;
static DS3_Image keymask_spr;

// dynamic sprites
static DS3_Image menu_spr;
static DS3_Image whitepixel_spr;
static DS3_Image blackpixel_spr;
static DS3_Image uibvnc_spr;

// SDL Surfaces
SDL_Surface *menu_img=NULL;
SDL_Surface *chars_img=NULL;

// static variables
static u8* uibvnc_buffer = NULL;
static Handle repaintRequired;
static int uib_isinit=0;
static int kb_y_pos = 0;
static int kb_activekey=-1;
static int sticky=0;
static unsigned char keysPressed[256];
static int kb_enabled = 0;
static int uib_x=0, uib_y=0;
static SDL_Color txt_col = DEF_TXT_COL;
static SDL_Color bck_col = DEF_BCK_COL;
static int top_scrollbars = 0;
static int sb_pos_hx, sb_pos_hw, sb_pos_vy, sb_pos_vh;
static int bottom_lcd_on=1;
static int uibvnc_scaling=1;


// static functions
// ================

/* 8-times unrolled loop */
#define DUFFS_LOOP(pixel_copy_increment, width)			\
{ int n = (width+7)/8;							\
	switch (width & 7) {						\
	case 0: do {	pixel_copy_increment;				\
	case 7:		pixel_copy_increment;				\
	case 6:		pixel_copy_increment;				\
	case 5:		pixel_copy_increment;				\
	case 4:		pixel_copy_increment;				\
	case 3:		pixel_copy_increment;				\
	case 2:		pixel_copy_increment;				\
	case 1:		pixel_copy_increment;				\
		} while ( --n > 0 );					\
	}								\
}

static void bufferSetMask(u8* buffer, int width, int height, int pitch)
{
	int skip = (pitch - width) * 4;

	while ( height-- ) {
		DUFFS_LOOP(
		{
			*buffer = 0xFF;
			buffer+=4;
		},
		width);
		buffer += skip;
	}
}

// sprite handling funtions
extern C3D_RenderTarget* VideoSurface2;
extern void SDL_RequestCall(void(*callback)(void*), void *param);

#define CLEAR_COLOR 0x000000FF
// Used to convert textures to 3DS tiled format
// Note: vertical flip flag set so 0,0 is top left of texture
#define TEXTURE_TRANSFER_FLAGS \
	(GX_TRANSFER_FLIP_VERT(1) | GX_TRANSFER_OUT_TILED(1) | GX_TRANSFER_RAW_COPY(0) | \
	GX_TRANSFER_IN_FORMAT(GX_TRANSFER_FMT_RGBA8) | GX_TRANSFER_OUT_FORMAT(GX_TRANSFER_FMT_RGBA8) | \
	GX_TRANSFER_SCALING(GX_TRANSFER_SCALE_NO))

#define TEX_MIN_SIZE 64

// key repeat functions for the simulated key repeat
static int keydown = 0;
static u32 key_ts = 0;
static int repeat_state = 0;

void make_key_event_norepeat(SDL_Event *e, int sym, int pressed)
{
	if (pressed) {
		e->type = SDL_KEYDOWN;
		e->key.state = SDL_PRESSED;
	} else {
		e->type = SDL_KEYUP;
		e->key.state = SDL_RELEASED;
	}
	e->key.keysym.sym = e->key.keysym.unicode = sym;
	e->key.keysym.mod = e->key.keysym.scancode = 0;
}

void make_key_event(SDL_Event *e, int sym, int pressed)
{
	if (pressed) {
		keydown = sym;
		key_ts = SDL_GetTicks();
		repeat_state = 0;
	} else {
		keydown = repeat_state = 0;
	}
	make_key_event_norepeat(e, sym, pressed);
}

void push_key_event(int sym, int pressed) {
	SDL_Event e;
	make_key_event(&e, sym, pressed);
	SDL_PushEvent(&e);
}

void push_key_event_norepeat(int sym, int pressed) {
	SDL_Event e;
	make_key_event_norepeat(&e, sym, pressed);
	SDL_PushEvent(&e);
}

void checkKeyRepeat() {
	int send=0;
	if (!keydown) return;
	u32 ts=SDL_GetTicks();
	switch (repeat_state) {
	case 0:
		if (ts-key_ts > SDL_DEFAULT_REPEAT_DELAY) {
			++repeat_state;
			send=-1;
			key_ts=ts;
		}
		break;
	case 1:
		if (ts - key_ts > SDL_DEFAULT_REPEAT_INTERVAL / 2) {
			++repeat_state;
			send=1;
			key_ts=ts;
		}
		break;
	case 2:
		if (ts - key_ts > SDL_DEFAULT_REPEAT_INTERVAL / 2) {
			--repeat_state;
			send=-1;
			key_ts=ts;
		}
		break;
	}
	extern rfbClient* cl;
	if (send) {
		if (send == -1) {
			// do not send keyup events for repeating when RFB connection is active
			if (!cl) push_key_event_norepeat(keydown, 0);
		} else {
			push_key_event_norepeat(keydown, 1);
		}
	}
}

static unsigned int mynext_pow2(unsigned int v)
{
	v--;
	v |= v >> 1;
	v |= v >> 2;
	v |= v >> 4;
	v |= v >> 8;
	v |= v >> 16;
	v++;
	return v >= TEX_MIN_SIZE ? v : TEX_MIN_SIZE;
}

#define B2T(x) (int)(((x)*400.0f)/320.0f+0.5f)

//---------------------------------------------------------------------------------
static void  drawImage( DS3_Image *img, int x, int y, int w, int h, int deg) {
//---------------------------------------------------------------------------------
	if (!img) return;
	if (!w) w=img->w;
	if (!h) h=img->h;
	float fw = (float)w;
	float fx = (float)x;

	int x1,x2,x3,x4,y1,y2,y3,y4;
	if (deg) {
		// rotated draw
		float fh = (float)h;
		float fy = (float)y;
		float rad = ((float)deg) * M_PI / 180.0f;
		float c = cosf(rad);
		float s = sinf(rad);
		float cx = fx + fw / 2.0f;
		float cy = fy + fh / 2.0f;
		x1 = B2T(c*(fx-cx)-s*(fy-cy)+cx);
		y1 = (int)(s*(fx-cx)+c*(fy-cy)+cy+0.5f);
		x2 = B2T(c*(fx-cx)-s*(fy+fh-cy)+cx);
		y2 = (int)(s*(fx-cx)+c*(fy+fh-cy)+cy+0.5f);
		x3 = B2T(c*(fx+fw-cx)-s*(fy-cy)+cx);
		y3 = (int)(s*(fx+fw-cx)+c*(fy-cy)+cy+0.5f);
		x4 = B2T(c*(fx+fw-cx)-s*(fy+fh-cy)+cx);
		y4 = (int)(s*(fx+fw-cx)+c*(fy+fh-cy)+cy+0.5f);
	} else {
		x1 = B2T(fx);
		y1 = y;
		x2 = B2T(fx);
		y2 = y + h;
		x3 = B2T(fx + fw);
		y3 = y;
		x4 = B2T(fx + fw);
		y4 = y + h;
	}

	C3D_TexBind(0, &(img->tex));
	// Draw a textured quad directly
	C3D_ImmDrawBegin(GPU_TRIANGLE_STRIP);
		C3D_ImmSendAttrib( x1, y1, 0.5f, 0.0f);	// v0 = position
		C3D_ImmSendAttrib( 0.0f, 0.0f, 0.0f, 0.0f);	// v1 = texcoord0

		C3D_ImmSendAttrib( x2, y2, 0.5f, 0.0f);
		C3D_ImmSendAttrib( 0.0f, img->fh, 0.0f, 0.0f);

		C3D_ImmSendAttrib( x3, y3, 0.5f, 0.0f);		// v0 = position
		C3D_ImmSendAttrib( img->fw, 0.0f, 0.0f, 0.0f);

		C3D_ImmSendAttrib( x4, y4, 0.5f, 0.0f);		// v0 = position
		C3D_ImmSendAttrib( img->fw, img->fh, 0.0f, 0.0f);
	C3D_ImmDrawEnd();
}

static void makeTexture(C3D_Tex *tex, const u8 *mygpusrc, unsigned hw, unsigned hh) {
	// init texture
	C3D_TexDelete(tex);
	C3D_TexInit(tex, hw, hh, GSP_RGBA8_OES);
	C3D_TexSetFilter(tex, GPU_NEAREST, GPU_NEAREST);

	// Convert image to 3DS tiled texture format
	GSPGPU_FlushDataCache(mygpusrc, hw*hh*4);
	C3D_SyncDisplayTransfer ((u32*)mygpusrc, GX_BUFFER_DIM(hw,hh), (u32*)(tex->data), GX_BUFFER_DIM(hw,hh), TEXTURE_TRANSFER_FLAGS);
	GSPGPU_FlushDataCache(tex->data, hw*hh*4);
}

static void makeImage(DS3_Image *img, const u8 *pixels, unsigned w, unsigned h, int noconv) {
	img->w=w;
	img->h=h;
	unsigned hw=mynext_pow2(w);
	img->fw=(float)(w)/hw;
	unsigned hh=mynext_pow2(h);
	img->fh=(float)(h)/hh;
	if (noconv) { // pixels are already in a transferable format (buffer in linear RAM, ABGR pixel format, pow2-dimensions)
		makeTexture(&(img->tex), pixels, hw, hh);
	} else {
		// GX_DisplayTransfer needs input buffer in linear RAM
		u8 *gpusrc = (u8*)linearAlloc(hh*hw*4);
		// copy to linear buffer, convert from RGBA to ABGR
		const u8* src=pixels; u8 *dst;
		for(unsigned y = 0; y < h; y++) {
			dst=gpusrc+y*hw*4;
			for (unsigned x=0; x<w; x++) {
				int r = *src++;
				int g = *src++;
				int b = *src++;
				int a = *src++;

				*dst++ = a;
				*dst++ = b;
				*dst++ = g;
				*dst++ = r;
			}
		}
		makeTexture(&(img->tex), gpusrc, hw, hh);
		linearFree(gpusrc);
	}
	return;
}

SDL_Surface *myIMG_Load(char *fname)
{
	SDL_Surface *s;
	int paletted = 0;
	png_image image = {0};
	image.version = PNG_IMAGE_VERSION;
	if (!png_image_begin_read_from_file(&image, fname))
		return NULL;
	if (image.format & PNG_FORMAT_FLAG_COLORMAP) paletted=1;
	image.format = paletted ? PNG_FORMAT_RGBA_COLORMAP : PNG_FORMAT_RGBA;
	if (paletted)
		s = SDL_CreateRGBSurface(SDL_SWSURFACE,image.width, image.height, 8, 0, 0, 0, 0xff000000);
	else
		s = SDL_CreateRGBSurface(SDL_SWSURFACE,image.width, image.height, 32, 0x000000ff, 0x0000ff00, 0x00ff0000, 0xff000000);
	if (!s) {
		png_image_free(&image);
		return NULL;
	}
	if (!png_image_finish_read(&image, NULL, s->pixels, 0, paletted ? s->format->palette->colors: NULL)) {
		SDL_FreeSurface(s);
		return NULL;
	}
	return s;
}

static int loadImage(DS3_Image *img, char *fname) {
	SDL_Surface *s=myIMG_Load(fname);
	if (!s) return -1;

	makeImage(img, (u8*)s->pixels, s->w,s->h,0);
	SDL_FreeSurface(s);
	return 0;
}

// bottom handling functions
// =========================
static inline void requestRepaint() {
	svcSignalEvent(repaintRequired);
}

int uibvnc_w=320;
int uibvnc_h=240;
int uibvnc_x=0;
int uibvnc_y=0;

static void uib_repaint(void *param) {
	ENTER
	
	// paint the scrollbars
	if (top_scrollbars) {
		if (top_scrollbars & 1) drawImage(&blackpixel_spr, 0, 240-SCROLLBAR_WIDTH, 320, SCROLLBAR_WIDTH, 0);
		if (top_scrollbars & 2) drawImage(&blackpixel_spr, 320-SCROLLBAR_WIDTH, 0, SCROLLBAR_WIDTH, 240, 0);
		if (top_scrollbars & 1) drawImage(&whitepixel_spr, sb_pos_hx, 240-SCROLLBAR_WIDTH, sb_pos_hw, SCROLLBAR_WIDTH, 0);
		if (top_scrollbars & 2) drawImage(&whitepixel_spr, 320-SCROLLBAR_WIDTH, sb_pos_vy, SCROLLBAR_WIDTH, sb_pos_vh, 0);
	}

	if (svcWaitSynchronization(repaintRequired, 0)) return;
	svcClearEvent(repaintRequired);

	// Render the scene
	C3D_RenderTargetClear(VideoSurface2, C3D_CLEAR_ALL, CLEAR_COLOR, 0);
	C3D_FrameDrawOn(VideoSurface2);

	// bottom VNC screen
	if (uibvnc_buffer) {
		drawImage(&uibvnc_spr, uibvnc_x, uibvnc_y, uibvnc_w, uibvnc_h, 0);
		// scrollbars
		if (uibvnc_w > 320) {
			drawImage(&blackpixel_spr, 0, 240-SCROLLBAR_WIDTH, 320, SCROLLBAR_WIDTH, 0);
			drawImage(&whitepixel_spr, (-uibvnc_x*320)/uibvnc_w, 240-SCROLLBAR_WIDTH, (320*320)/uibvnc_w, SCROLLBAR_WIDTH, 0);
		}
		if (uibvnc_h > 240) {
			drawImage(&blackpixel_spr, 320-SCROLLBAR_WIDTH, 0, SCROLLBAR_WIDTH, 240, 0);
			drawImage(&whitepixel_spr, 320-SCROLLBAR_WIDTH, (-uibvnc_y*240)/uibvnc_h, SCROLLBAR_WIDTH, (240*240)/uibvnc_h, 0);
		}
	} else { 
		// menu
		int y = kb_enabled ? MIN(0,-240 + kb_y_pos + (29-uib_y) * 8) : 0;
		drawImage(&menu_spr, 0, y, 0, 0, 0);
	}

	if (kb_enabled) {
		// keyboard
		drawImage(&kbd_spr, 0, kb_y_pos, 0, 0, 0);
		// keyboard twisty
		DS3_Image *tw = kb_y_pos > (240 - kbd_spr.h) ? &(twistyup_spr):&(twistydn_spr);
		drawImage(tw, 320 - tw->w, kb_y_pos - tw->h, 0, 0, 0);
		// keys pressed
		uikbd_key *k;
		for (int i=0;uikbd_keypos[i].key!=-1; ++i) {
			k=&(uikbd_keypos[i]);
			if (keysPressed[i]==0) continue;
			drawImage(&(keymask_spr),k->x,k->y+kb_y_pos,k->w,k->h,0);
		}
	}
}

// shutdown bottom
static void uib_shutdown() {
	if (!uib_isinit) return;

	SDL_RequestCall(NULL, NULL);
	svcCloseHandle(repaintRequired);
	
	if (menu_img) {
		u8 *p = menu_img->pixels;
		SDL_FreeSurface(menu_img);
		linearFree(p);
		menu_img = NULL;
	}
	uib_isinit = 0;
}

static void keypress_recalc() {
	int state,key;

	memset(keysPressed,0,sizeof(keysPressed));
	for (key = 0; uikbd_keypos[key].key!=-1; ++key) {
		state=0;
		if (uikbd_keypos[key].sticky) {
			if (uikbd_keypos[key].sticky & sticky) state=1;
		} else {
			if (key == kb_activekey) state=1;
		}
		keysPressed[key]=state;
	}
}

// threadworker-related vars / functions
#define MAXPENDING 10

static SDL_Thread *worker=NULL;
static SDL_sem *worker_sem=NULL;
static int (*worker_fn[MAXPENDING])(void *);
static void *worker_data[MAXPENDING];

static int worker_thread(void *data) {
	static int p2=0;
	while( 1 ) {
		SDL_SemWait(worker_sem);
		(worker_fn[p2])(worker_data[p2]);
		worker_fn[p2]=NULL;
		p2=(p2+1)%MAXPENDING;
	}
	return 0;
}

static void worker_init() {
	worker_sem = SDL_CreateSemaphore(0);
	worker = SDL_CreateThread(worker_thread,NULL);
	memset(worker_fn, 0, sizeof(worker_fn));
	memset(worker_data, 0, sizeof(worker_data));
}

static int start_worker(int (*fn)(void *), void *data) {
	static int p1=0;
	if (!worker_sem) worker_init();
	if (worker_fn[p1] != NULL) return -1;
	worker_fn[p1]=fn;
	worker_data[p1]=data;
	SDL_SemPost(worker_sem);	
	p1=(p1+1)%MAXPENDING;
	return 0;
}

// animation / keyboard toggle related functions
static void *alloc_copy(void *p, size_t s) {
	void *d=malloc(s);
	memcpy(d,p,s);
	return d;
}

typedef struct animation {
	int *var;
	int from;
	int to;
} animation;

typedef struct animation_set {
	int steps;
	int delay;
	int nr;
	void (*callback)(void *);
	void *callback_data;
	void (*callback2)(void *);
	void *callback2_data;
	animation anim[];
} animation_set;

static int animate(void *data){
	animation_set *a=(animation_set*)data;
	int steps = a->steps != 0 ? a->steps : 15 ;
	int delay = a->delay != 0 ? a->delay : 16 ; // 1/60 sec, one 3ds frame
	for (int s=0; s <= steps; s++) {
		for (int i=0; i < a->nr; i++) {
			*(a->anim[i].var)=
				a->anim[i].from +
				(((a->anim[i].to - a->anim[i].from) * s ) / steps);
		}
		if (a->callback) (a->callback)(a->callback_data);
		if (s != steps) SDL_Delay(delay);
	}
	if (a->callback2) (a->callback2)(a->callback2_data);
	free(data);
	return 0;
}

static void anim_callback(void *param) {
	requestRepaint();
}

void uib_enable_keyboard(int enable) {
	kb_enabled=enable;
	requestRepaint();
}

static void toggle_keyboard() {
	int y1=240-kbd_spr.h;

	start_worker(animate, alloc_copy(&((int[]){
		0, 0, 1, // steps, delay, nr
		(int)anim_callback, 0, // callback, callback_data
		0,0, // callback2, callback2_data
		(int)(&kb_y_pos), kb_y_pos < 240 ? y1 : 240, kb_y_pos < 240 ? 240 : y1
	}), 10*sizeof(int)));
}

void uib_printtext(SDL_Surface *s, const char *str, int xo, int yo, int w, int h, SDL_Color tcol, SDL_Color bcol) {
	if (str==NULL) return;
	int maxw=w/8;
	int maxh=h/8;
	char *buf=malloc(maxw*maxh);
	memset(buf,255,maxw*maxh);
	int x=0,y=0,cx=0,cy=0;
	char *t,c;
	for (int i=0; str[i] != 0 && y < maxh; ++i) {
		c=(str[i] & 0x7f)-32;
		if (c<0) c=0;
		if (c==0 && x==0) continue;
		if ((c==0 || c==92)  && (((t=strpbrk(str+i+1," |,"))==NULL && y < maxh) || t-str-i > maxw-x)) { // wrap on new word before end of line
			if (c==92) buf[x+y*maxw]=c & 0x7F;
			x=0;++y; continue;}
		buf[x+y*maxw]=c & 0x7F;
		if (cx<x) cx=x;
		if (cy<y) cy=y;
		++x;
		if (x>=maxw) { //end of line, next line
			x=0;++y;
			if ((t=strchr(str+i+1,' '))!=NULL && t-str-i-2<(maxw/2)) i=t-str; // wrap or cut?
		}
	}
	int xof = xo+(w-(cx+1)*8)/2;
	int yof = yo+(h-(cy+1)*8)/2;
	SDL_SetPalette(chars_img, SDL_LOGPAL, &tcol, 1, 1);
	SDL_SetPalette(chars_img, SDL_LOGPAL, &bcol, 0, 1);
	if (bcol.unused != 0)
		SDL_FillRect(s, &(SDL_Rect){xof, yof, w, h}, SDL_MapRGBA(s->format, bcol.r, bcol.g, bcol.b, bcol.unused));
	for (int i=0; i<maxw*maxh; i++) {
		c=buf[i];
		if (c==255) continue;
		SDL_BlitSurface(
			chars_img,
			&(SDL_Rect){.x=(c&0x0f)*8, .y=(c>>4)*8, .w=8, .h=8},
			s,
			&(SDL_Rect){.x=xof+(i % maxw)*8, .y=yof+(i/maxw)*8});
	}
	SDL_SetPalette(chars_img, SDL_LOGPAL, &COL_WHITE, 1, 1);
	SDL_SetPalette(chars_img, SDL_LOGPAL, &COL_BLACK, 0, 1);
	free(buf);
}

void uib_printstring(SDL_Surface *s, const char *str, int x, int y, int maxchars, str_alignment align, SDL_Color tcol, SDL_Color bcol) {
	int xof, w;
	if (str==NULL || str[0]==0) return;
	w=strlen(str);
	if (maxchars > 0 && w > maxchars) w=maxchars;

	switch (align) {
		case ALIGN_CENTER:
			xof=x-(w*8/2);
			break;
		case ALIGN_RIGHT:
			xof=x-w*8;
			break;
		default:
			xof=x;
	}

	int c;
	SDL_SetPalette(chars_img, SDL_LOGPAL, &bcol, 0, 1);
	SDL_SetPalette(chars_img, SDL_LOGPAL, &tcol, 1, 1);
	if (bcol.unused != 0)
		SDL_FillRect(s, &(SDL_Rect){xof, y, w*8, 8}, SDL_MapRGBA(s->format, bcol.r, bcol.g, bcol.b, bcol.unused));
	for (int i=0; i < w; i++) {
		c=str[i];
		SDL_BlitSurface(
			chars_img,
			&(SDL_Rect){.x=(c&0x0f)*8, .y=(c>>4)*8, .w=8, .h=8},
			s,
			&(SDL_Rect){.x = xof+i*8, .y = y});
	}
	SDL_SetPalette(chars_img, SDL_LOGPAL, &COL_WHITE, 1, 1);
	SDL_SetPalette(chars_img, SDL_LOGPAL, &COL_BLACK, 0, 1);
}

void uib_set_position(int x, int y) {
	uib_x = x;
	uib_y = y;
}

void uib_set_colors(SDL_Color text, SDL_Color background) {
	txt_col = text;
	bck_col = background;
}

void uib_invert_colors() {
	SDL_Color i = txt_col;
	txt_col = bck_col;
	bck_col = i;
}

void uib_reset_colors() {
	txt_col = DEF_TXT_COL;
	bck_col = DEF_BCK_COL;
}

void uib_clear() {
	SDL_FillRect(menu_img, NULL, 0x000000ff);
	uib_set_position(0,0);
}

static void uib_scrollup(int lines) {
	if (lines) {
		int offset = lines * 8 * menu_img->pitch;
		int size = menu_img->h * menu_img->pitch - offset;
		memmove(menu_img->pixels, menu_img->pixels + offset, size);
		memset(menu_img->pixels + size, 0, offset);
	}
}

static void uib_nextline() {
	uib_x = 0; ++uib_y;
	if (uib_y >= 30) {
		uib_scrollup(uib_y - 29);
		uib_y=29;
	}
}

int uib_vprintf(char *format, va_list arg) {
	int l=vsnprintf(NULL, 0, format, arg);
	if (!l) return 0;
	char *p=(char*)malloc(l+1);
	vsnprintf(p, l+1, format, arg);
	// split the string by newlines
	char *line = p, *end_line;
	int to_print, len;
	while (line && line[0]) {
		end_line = strchr(line, '\n');
		if (!end_line) end_line = line + strlen(line);
		to_print = len = end_line - line;
		while (to_print > 0) {
			if (uib_x>=40) uib_nextline();
			int i = MIN(to_print, 40-uib_x);
			uib_printstring(menu_img, line + len - to_print, uib_x*8, uib_y*8, i, ALIGN_LEFT, txt_col, bck_col);
			to_print -= i;
			uib_x += i;
		}
		// take care of newlines
		while (*end_line == '\n') {
			uib_nextline();
			++end_line;
		}
		line = end_line;
	}
	free(p);
	return l;
}

int uib_printf(char *format, ...) {
	va_list argptr;
	va_start(argptr, format);
	int r=uib_vprintf(format, argptr);
	va_end(argptr);
	return r;
}

// exposed functions
void uib_init() {
	ENTER

	if (uib_isinit) return;
	uib_isinit=1;

	chars_img=myIMG_Load("romfs:/chars.png");
	SDL_SetColorKey(chars_img, SDL_SRCCOLORKEY, 0x00000000);

	// init menusurface
	u8* menu_gpusrc = (u8*)linearAlloc(512*256*4);
	menu_img = SDL_CreateRGBSurfaceFrom(menu_gpusrc, 320, 240, 32, 512*4, 0xff000000,0x00ff0000,0x0000ff00,0x000000ff);
	SDL_FillRect(menu_img, NULL, 0x000000ff);

	// pre-load sprites
	loadImage(&kbd_spr,			"romfs:/kbd.png");
	loadImage(&twistyup_spr,	"romfs:/twistyup.png");
	loadImage(&twistydn_spr,	"romfs:/twistydn.png");
	makeImage(&keymask_spr, (const u8[]){0x00, 0x00, 0x00, 0x80},1,1,0);

	makeImage(&whitepixel_spr, (const u8[]){0xff, 0xff, 0xff, 0xff},1,1,0);
	makeImage(&blackpixel_spr, (const u8[]){0x00, 0x00, 0x00, 0xff},1,1,0);

	// other stuff
	kb_y_pos = 240; // keboard hidden at first

	svcCreateEvent(&repaintRequired, RESET_ONESHOT);
	SDL_RequestCall(uib_repaint, NULL);

	uib_update(UIB_REPAINT);
	atexit(uib_shutdown);
}

void uib_update(int what)
{
	// init if needed
	if (!uib_isinit) uib_init();
	uib_must_redraw |= what;
	if (uib_must_redraw) {
		if (uib_must_redraw & UIB_RECALC_KEYPRESS) {
			keypress_recalc();
		}
		if (uib_must_redraw & UIB_RECALC_MENU) {
			makeImage(&menu_spr, menu_img->pixels, menu_img->w, menu_img->h, 1);
		}
		if (uib_must_redraw & UIB_RECALC_VNC) {
			bufferSetMask(uibvnc_buffer, uibvnc_spr.w, uibvnc_spr.h, mynext_pow2(uibvnc_spr.w));
			makeImage(&uibvnc_spr, uibvnc_buffer, uibvnc_spr.w, uibvnc_spr.h, 1);
		}
		uib_must_redraw = UIB_NO;
		requestRepaint();
	}
}

// *****************************
// lots of stuff below inspired from synaptics touchpad driver
// https://github.com/freedesktop/xorg-xf86-input-synaptics
// ******************************

enum TapState {
    TS_START,                   // No tap/drag in progress
    TS_1,                       // After first touch
    TS_MOVE,                    // Pointer movement enabled 
    TS_2A,                      // After first release 
    TS_2B,                      // After second/third/... release 
    TS_SINGLETAP,               // After timeout after first release 
    TS_3,                       // After second touch 
    TS_DRAG,                    // Pointer drag enabled 
    TS_4,                       // After release when "locked drags" enabled 
    TS_5,                       // After touch when "locked drags" enabled 
    TS_CLICKPAD_MOVE,           // After left button press on a clickpad 
	TS_MOUSEDOWN,
	TS_MOUSEUP,
};

static int mouse_state = 0;
static int mainMenu_max_tap_time=250;
static int mainMenu_click_time=100;
static int mainMenu_single_tap_timeout=250;
static int mainMenu_max_double_tap_time=250;
static int mainMenu_locked_drag_timeout=5000;
static int mainMenu_tap_and_drag_gesture=1;
static int mainMenu_locked_drags=0;

static int get_timeout(enum TapState s)
{
	switch (s) {
    case TS_1:
    case TS_3:
    case TS_5:
        return mainMenu_max_tap_time;
    case TS_SINGLETAP:
        return mainMenu_click_time;
    case TS_2A:
        return mainMenu_single_tap_timeout;
    case TS_2B:
        return mainMenu_max_double_tap_time;
    case TS_4:
        return mainMenu_locked_drag_timeout;
	default:
		break;
    }
	return 0;
}

int tap_lastx=0;
int tap_lasty=0;

static void set_tap_state(enum TapState s, SDL_Event *e)
{
    int x=-1;
	switch (s) {
    case TS_START:
    case TS_1:
    case TS_2A:
    case TS_2B:
	case TS_MOUSEUP:
        x = 0;
        break;
    case TS_3:
    case TS_SINGLETAP:
	case TS_MOUSEDOWN:
        x = 1;
        break;
    default:
        break;
    }
    if (x != -1 && x != mouse_state) {
		SDL_Event ev = {0};
		if (x == 0) {
			ev.type = SDL_MOUSEBUTTONUP;
			ev.button.state = SDL_RELEASED;
		} else {
			ev.type = SDL_MOUSEBUTTONDOWN;
			ev.button.state = SDL_PRESSED;
		}
		ev.button.which = 1;
		ev.button.button = SDL_BUTTON_LEFT;

		ev.button.x = e?e->button.x:tap_lastx;
		ev.button.y = e?e->button.y:tap_lasty;

		SDL_PushEvent(&ev);
		mouse_state = x;
	}
}

#define SETSTATE(x)								\
    {status = x;								\
	timeout = get_timeout(x);					\
	if (timeout) timeout += SDL_GetTicks();		\
	set_tap_state(x,e);							\
	is_timeout=0;}

int uib_handle_tap_processing(SDL_Event *e) {
	static Uint32 timeout = 0;
	static enum TapState status = TS_START;
	static int status2 = 0;
	int is_timeout = 0;

	if (!e) {
		switch (status2) {
		case 2:
			set_tap_state(TS_MOUSEDOWN, NULL);
			--status2;
			break;
		case 1:
			set_tap_state(TS_MOUSEUP, NULL);
			--status2;
			break;
		}
	} else {
		tap_lastx = e->type == SDL_MOUSEMOTION ? e->motion.x : e->button.x;
		tap_lasty = e->type == SDL_MOUSEMOTION ? e->motion.y : e->button.y;
	}

	if (timeout && SDL_GetTicks()>=timeout) {
		is_timeout = 1;
		timeout = 0;
	}
	if (!e && !is_timeout) return 1;

 restart:
    switch (status) {
    case TS_START: // No tap/drag in progress
        if (e && e->type==SDL_MOUSEBUTTONDOWN)
            SETSTATE(TS_1)
        break;
    case TS_1: // After first touch
        if (is_timeout) {
            SETSTATE(TS_MOVE)
            goto restart;
        }
        else if (e && e->type==SDL_MOUSEBUTTONUP) {
            SETSTATE(TS_2A)
        }
        break;
    case TS_MOVE: // Pointer movement enabled 
        if (e && e->type==SDL_MOUSEBUTTONUP) {
            SETSTATE(TS_START)
        }
        break;
    case TS_2A: // After first release 
        if (e && e->type==SDL_MOUSEBUTTONDOWN)
            SETSTATE(TS_3)
        else if (is_timeout)
            SETSTATE(TS_SINGLETAP)
        break;
    case TS_2B: // After second/third/... release 
        if (e && e->type==SDL_MOUSEBUTTONDOWN) {
            SETSTATE(TS_3)
        }
        else if (is_timeout) {
            SETSTATE(TS_START)
            status2=2;
        }
        break;
    case TS_SINGLETAP: // After timeout after first release 
        if (e && e->type==SDL_MOUSEBUTTONDOWN)
            SETSTATE(TS_1)
        else if (is_timeout)
            SETSTATE(TS_START)
        break;
    case TS_3: // After second touch 
        if (is_timeout) {
            if (mainMenu_tap_and_drag_gesture) {
                SETSTATE(TS_DRAG)
            }
            else {
                SETSTATE(TS_1)
            }
            goto restart;
        }
        else if (e && e->type==SDL_MOUSEBUTTONUP) {
            SETSTATE(TS_2B)
        }
        break;
    case TS_DRAG:
        if (e && e->type==SDL_MOUSEBUTTONUP) {
            if (mainMenu_locked_drags) {
                SETSTATE(TS_4)
            }
            else {
                SETSTATE(TS_START)
            }
        }
        break;
    case TS_4:
        if (is_timeout) {
            SETSTATE(TS_START)
            goto restart;
        }
        if (e && e->type==SDL_MOUSEBUTTONDOWN)
            SETSTATE(TS_5)
        break;
    case TS_5:
        if (is_timeout) {
            SETSTATE(TS_DRAG)
            goto restart;
        }
        else if (e && e->type==SDL_MOUSEBUTTONUP) {
            SETSTATE(TS_START)
        }
        break;
	default:
		break;
    }
	return 1;
}

// *************************
// Bottom screen event handling routine
// *************************

#define mylog2 __builtin_ctz

int uib_handle_event(SDL_Event *e, int taphandling) {
	static int sticky2keysym[4]={-1,-1,-1,-1};

	extern SDL_Surface *sdl;
	int i,x,y;
	static int process_touchpad = 0;

	if (e->type == SDL_KEYDOWN && e->key.keysym.sym == 2) {
		toggle_keyboard();
		return 1;
	}
	if (e->type != SDL_MOUSEMOTION &&
		e->type != SDL_MOUSEBUTTONUP &&
		e->type != SDL_MOUSEBUTTONDOWN) return 0;

	if (e->type == SDL_MOUSEMOTION) {
		return (e->motion.state || (taphandling & 2))? 0 : 1;
	} else {
		if (e->button.which == 1) return 0; // events generated by taphandling are not handled here
	}

	// check if we should do touchpad or virtual keyboard processing
	if (process_touchpad) {
		if (e->type == SDL_MOUSEBUTTONUP) process_touchpad=0;
		return (taphandling&1) ? uib_handle_tap_processing(e) : 0;
	}
	if (!kb_enabled && e->type == SDL_MOUSEBUTTONDOWN) {
		process_touchpad = 1;
		return (taphandling&1) ? uib_handle_tap_processing(e) : 0;
	}
	switch (e->type) {
		case SDL_MOUSEBUTTONUP:
			if (kb_activekey==-1) return 1; // did not get the button down, so ignore button up
			i=kb_activekey;
			kb_activekey=-1;
			break;
		case SDL_MOUSEBUTTONDOWN:
			x = (int)(((((double)e->button.x)*320.0f)/((double)sdl->w))+0.5f);
			y = (int)(((((double)e->button.y)*240.0f)/((double)sdl->h))+0.5f);
			for (i = 0; uikbd_keypos[i].key != -1 ; ++i) {
				// keyboard button
				if (x >= uikbd_keypos[i].x &&
					x <  uikbd_keypos[i].x + uikbd_keypos[i].w &&
					y >= uikbd_keypos[i].y + kb_y_pos &&
					y <  uikbd_keypos[i].y + uikbd_keypos[i].h + kb_y_pos) break;
			}
			if (uikbd_keypos[i].key == -1) {
				// touch outside of keyboard - process touchpad
				if  (y < kb_y_pos) {
					process_touchpad = 1;
					return (taphandling&1) ? uib_handle_tap_processing(e) : 0;
				}
				return 1;
			}
			if (i==kb_activekey) return 1; // ignore button down on an already pressed key
			kb_activekey=i;
			break;
		default:
			return 1;
	}

	// sticky key press
	if (uikbd_keypos[i].sticky>0) {
		if (e->button.type == SDL_MOUSEBUTTONDOWN) {
			int pos=mylog2(uikbd_keypos[i].sticky & 0x0F);
			sticky = sticky ^ uikbd_keypos[i].sticky;
			if (sticky & uikbd_keypos[i].sticky) {
				push_key_event_norepeat((sticky2keysym[pos] = uikbd_keypos[i].key), 1);
			} else {
				push_key_event_norepeat(sticky2keysym[pos] == -1 ? uikbd_keypos[i].key : sticky2keysym[pos], 0);
				sticky2keysym[pos] = -1;
			}
		}
	} else {
		// normal key press
		if ((sticky & 1) && uikbd_keypos[i].shftkey) {
			push_key_event_norepeat(sticky2keysym[0], 0);
			if (uikbd_keypos[i].repeat)
				push_key_event(uikbd_keypos[i].shftkey, e->button.type == SDL_MOUSEBUTTONDOWN ? 1 : 0);
			else
				push_key_event_norepeat(uikbd_keypos[i].shftkey, e->button.type == SDL_MOUSEBUTTONDOWN ? 1 : 0);
			push_key_event_norepeat(sticky2keysym[0], 1);
		} else {
			if (uikbd_keypos[i].repeat)
				push_key_event(uikbd_keypos[i].key, e->button.type == SDL_MOUSEBUTTONDOWN ? 1 : 0);
			else
				push_key_event_norepeat(uikbd_keypos[i].key, e->button.type == SDL_MOUSEBUTTONDOWN ? 1 : 0);
		}
	}
	uib_update(UIB_RECALC_KEYPRESS);
	return 1;
}

void uib_show_scrollbars(int x, int y, int w, int h)
{
	static int width=400;
	static int height=240;
	if (w) width = w;
	if (h) height = h;
	top_scrollbars = (width>400?1:0) + (height>240?2:0);
	if (top_scrollbars) {
		if (width>400) {
			sb_pos_hx=(-x*320)/width;
			sb_pos_hw=(400*320)/width;
		}
		if (height>240) {
			sb_pos_vy=(-y*240)/height;
			sb_pos_vh=(240*240)/height;
		}
	}
	requestRepaint();
}

int uib_getBacklight() {
	return bottom_lcd_on;
}

void uib_setBacklight (int on) {
	if (on == bottom_lcd_on) return;
	gspLcdInit();
	bottom_lcd_on=on;
	if (on) {
		GSPLCD_PowerOnBacklight(GSPLCD_SCREEN_BOTTOM);
	} else {
		GSPLCD_PowerOffBacklight(GSPLCD_SCREEN_BOTTOM);
	}
	gspLcdExit();
}

void uibvnc_cleanup() {
	if (uibvnc_buffer) {
		linearFree(uibvnc_buffer);
		uibvnc_buffer=NULL;
	}
}

rfbBool uibvnc_resize(rfbClient* client) {

//log_citra("enter %s, %p, %d, %d",__func__, client, client->width, client->height);
	if (client->width > 1024 || client->height > 1024) {
		rfbClientErr("bottom resize: screen size >1024px!");
		return FALSE;
	}

	uibvnc_spr.w=client->width;
	uibvnc_spr.h=client->height;

	client->updateRect.x = client->updateRect.y = 0;
	client->updateRect.w = uibvnc_spr.w;
	client->updateRect.h = uibvnc_spr.h;

	/* (re)create the buffer used as the client's framebuffer */
	uibvnc_cleanup();

	unsigned hw=mynext_pow2(uibvnc_spr.w);
	unsigned hh=mynext_pow2(uibvnc_spr.h);

	// alloc buffer in linear RAM, ABGR pixel format, pow2-dimensions
	uibvnc_buffer = (u8*)linearAlloc(hh*hw*4);
	if(!uibvnc_buffer) {
		rfbClientErr("bottom resize: alloc failed");
		return FALSE;
	}
	memset(uibvnc_buffer, 255, hh*hw*4);
	uib_update(UIB_RECALC_VNC);

	client->width = hw;
	client->frameBuffer=uibvnc_buffer;

	client->format.bitsPerPixel=32;
	client->format.redShift=24;
	client->format.greenShift=16;
	client->format.blueShift=8;

	client->format.redMax = client->format.greenMax = client->format.blueMax = 255;
	SetFormatAndEncodings(client);

	if (uibvnc_scaling) {
		int scale1024 = MIN (1024, MIN(320*1024 / uibvnc_spr.w, 240*1024 / uibvnc_spr.h)); // no upscaling (scale max 1024)
		uibvnc_w = uibvnc_spr.w * scale1024 / 1024;
		uibvnc_h = uibvnc_spr.h * scale1024 / 1024;
		uibvnc_x = (320 - uibvnc_w) / 2;
		uibvnc_y = (240 - uibvnc_h) / 2;
	} else {
		extern int x,y;
		int w = uibvnc_spr.w > 320 ? 318 : 320;
		int h = uibvnc_spr.h > 240 ? 238 : 240;
		// center screen around mouse cursor if not scaling
		uibvnc_w = uibvnc_spr.w;
		uibvnc_h = uibvnc_spr.h;
		uibvnc_x = uibvnc_w > w ? LIMIT(-x + w / 2, -uibvnc_w + w, 0) : ( w - uibvnc_w ) / 2;
		uibvnc_y = uibvnc_h > h ? LIMIT(-y + h / 2, -uibvnc_h + h, 0) : ( h - uibvnc_h ) / 2;
	}
	return TRUE;
}

void uibvnc_setScaling(int scaling) {
	uibvnc_scaling=scaling;
}
