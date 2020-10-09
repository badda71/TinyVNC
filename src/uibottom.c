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
#include "uibottom.h"

#define ENTER //log_citra("enter %s",__func__);

/*
uikbd_key uikbd_keypos[] = {
	//  x,  y,   w,   h,         key,stky, flgs, name
	// toggle kb button
	{   0,-15,  36,  15,         255,   0,  0,  "ToggleKB"},
	// 1st Row
	{   0,  0,  32,  16,      AK_ESC,   0,  0,  "ESC"},
	{  32,  0,  16,  16,       AK_F1,   0,  0,  "F1"},
	{  48,  0,  16,  16,       AK_F2,   0,  0,  "F2"},
	{  64,  0,  16,  16,       AK_F3,   0,  0,  "F3"},
	{  80,  0,  16,  16,       AK_F4,   0,  0,  "F4"},
	{  96,  0,  16,  16,       AK_F5,   0,  0,  "F5"},
	{ 112,  0,  16,  16,       AK_F6,   0,  0,  "F6"},
	{ 128,  0,  16,  16,       AK_F7,   0,  0,  "F7"},
	{ 144,  0,  16,  16,       AK_F8,   0,  0,  "F8"},
	{ 160,  0,  16,  16,       AK_F9,   0,  0,  "F9"},
	{ 176,  0,  16,  16,      AK_F10,   0,  0,  "F10"},
	{ 192,  0,  32,  16,      AK_DEL,   0,  0,  "DEL"},
	{ 224,  0,  32,  16,     AK_HELP,   0,  0,  "HELP"},
	{ 256,  0,  16,  16, AK_NPLPAREN,   0,  0,  "NP ("},
	{ 272,  0,  16,  16, AK_NPRPAREN,   0,  0,  "NP )"},
	{ 288,  0,  16,  16,    AK_NPDIV,   0,  0,  "NP /"},
	{ 304,  0,  16,  16,    AK_NPMUL,   0,  0,  "NP *"},
	// 2nd Row
	{   0, 16,  32,  16,AK_BACKQUOTE,   0,  0,  "`"},
	{  32, 16,  16,  16,        AK_1,   0,  0,  "1"},
	{  48, 16,  16,  16,        AK_2,   0,  0,  "2"},
	{  64, 16,  16,  16,        AK_3,   0,  0,  "3"},
	{  80, 16,  16,  16,        AK_4,   0,  0,  "4"},
	{  96, 16,  16,  16,        AK_5,   0,  0,  "5"},
	{ 112, 16,  16,  16,        AK_6,   0,  0,  "6"},
	{ 128, 16,  16,  16,        AK_7,   0,  0,  "7"},
	{ 144, 16,  16,  16,        AK_8,   0,  0,  "8"},
	{ 160, 16,  16,  16,        AK_9,   0,  0,  "9"},
	{ 176, 16,  16,  16,        AK_0,   0,  0,  "0"},
	{ 192, 16,  16,  16,    AK_MINUS,   0,  0,  "-"},
	{ 208, 16,  16,  16,    AK_EQUAL,   0,  0,  "="},
	{ 224, 16,  16,  16,AK_BACKSLASH,   0,  0,  "\\"},
	{ 240, 16,  16,  16,       AK_BS,   0,  0,  "Backspace"},
	{ 256, 16,  16,  16,      AK_NP7,   0,  0,  "NP 7"},
	{ 272, 16,  16,  16,      AK_NP8,   0,  0,  "NP 8"},
	{ 288, 16,  16,  16,      AK_NP9,   0,  0,  "NP 9"},
	{ 304, 16,  16,  16,    AK_NPSUB,   0,  0,  "NP -"},
	// 3rd Row
	{   0, 32,  32,  16,      AK_TAB,   0,  0,  "TAB"},
	{  32, 32,  16,  16,        AK_Q,   0,  0,  "Q"},
	{  48, 32,  16,  16,        AK_W,   0,  0,  "W"},
	{  64, 32,  16,  16,        AK_E,   0,  0,  "E"},
	{  80, 32,  16,  16,        AK_R,   0,  0,  "R"},
	{  96, 32,  16,  16,        AK_T,   0,  0,  "T"},
	{ 112, 32,  16,  16,        AK_Y,   0,  0,  "Y"},
	{ 128, 32,  16,  16,        AK_U,   0,  0,  "U"},
	{ 144, 32,  16,  16,        AK_I,   0,  0,  "I"},
	{ 160, 32,  16,  16,        AK_O,   0,  0,  "O"},
	{ 176, 32,  16,  16,        AK_P,   0,  0,  "P"},
	{ 192, 32,  16,  16, AK_LBRACKET,   0,  0,  "["},
	{ 208, 32,  16,  16, AK_RBRACKET,   0,  0,  "]"},
	{ 224, 32,  32,  32,      AK_RET,   0,  0,  "RETURN"},
	{ 256, 32,  16,  16,      AK_NP4,   0,  0,  "NP 4"},
	{ 272, 32,  16,  16,      AK_NP5,   0,  0,  "NP 5"},
	{ 288, 32,  16,  16,      AK_NP6,   0,  0,  "NP 6"},
	{ 304, 32,  16,  16,    AK_NPADD,   0,  0,  "NP +"},
	// 4th Row
	{   0, 48,  32,  16,     AK_CTRL,   2,  0,  "CTRL"},
	{  32, 48,  16,  16,        AK_A,   0,  0,  "A"},
	{  48, 48,  16,  16,        AK_S,   0,  0,  "S"},
	{  64, 48,  16,  16,        AK_D,   0,  0,  "D"},
	{  80, 48,  16,  16,        AK_F,   0,  0,  "F"},
	{  96, 48,  16,  16,        AK_G,   0,  0,  "G"},
	{ 112, 48,  16,  16,        AK_H,   0,  0,  "H"},
	{ 128, 48,  16,  16,        AK_J,   0,  0,  "J"},
	{ 144, 48,  16,  16,        AK_K,   0,  0,  "K"},
	{ 160, 48,  16,  16,        AK_L,   0,  0,  "L"},
	{ 176, 48,  16,  16,AK_SEMICOLON,   0,  0,  ";"},
	{ 192, 48,  16,  16,    AK_QUOTE,   0,  0,  "'"},
	{ 256, 48,  16,  16,      AK_NP1,   0,  0,  "NP 1"},
	{ 272, 48,  16,  16,      AK_NP2,   0,  0,  "NP 2"},
	{ 288, 48,  16,  16,      AK_NP3,   0,  0,  "NP 3"},
	{ 304, 48,  16,  48,      AK_ENT,   0,  0,  "ENTER"},
	// 5th Row
	{   0, 64,  48,  16,      AK_LSH,   1,  0,  "LSHIFT"},
	{  48, 64,  16,  16,        AK_Z,   0,  0,  "Z"},
	{  64, 64,  16,  16,        AK_X,   0,  0,  "X"},
	{  80, 64,  16,  16,        AK_C,   0,  0,  "C"},
	{  96, 64,  16,  16,        AK_V,   0,  0,  "V"},
	{ 112, 64,  16,  16,        AK_B,   0,  0,  "B"},
	{ 128, 64,  16,  16,        AK_N,   0,  0,  "N"},
	{ 144, 64,  16,  16,        AK_M,   0,  0,  "M"},
	{ 160, 64,  16,  16,    AK_COMMA,   0,  0,  ","},
	{ 176, 64,  16,  16,   AK_PERIOD,   0,  0,  "."},
	{ 192, 64,  16,  16,    AK_SLASH,   0,  0,  "/"},
	{ 208, 64,  32,  16,      AK_RSH,   1,  0,  "RSHIFT"},
	{ 240, 64,  16,  16,       AK_UP,   0,  0,  "C_UP"},
	{ 256, 64,  32,  16,      AK_NP0,   0,  0,  "NP 0"},
	{ 288, 64,  16,  16,    AK_NPDEL,   0,  0,  "NP ."},
	// 6th row
	{   0, 80,  32,  17,     AK_LALT,   4,  0,  "LALT"},
	{  32, 80,  16,  17,     AK_LAMI,   8,  0,  "LAMI"},
	{  48, 80, 144,  17,      AK_SPC,   0,  0,  "SPACE"},
	{ 192, 80,  16,  17,     AK_RAMI,   8,  0,  "RAMI"},
	{ 208, 80,  16,  17,     AK_RALT,   4,  0,  "RALT"},
	{ 224, 80,  16,  17,       AK_LF,   0,  0,  "C_LEFT"},
	{ 240, 80,  16,  17,       AK_DN,   0,  0,  "C_DOWN"},
	{ 256, 80,  16,  17,       AK_RT,   0,  0,  "C_RIGHT"},
	// Finish
	{   0,  0,   0,   0,          -1,   0,  0,  ""}
};
*/
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

// dynamic sprites
static DS3_Image menu_spr;

// SDL Surfaces
SDL_Surface *menu_img=NULL;
SDL_Surface *chars_img=NULL;

// static variables
static Handle repaintRequired;
static int uib_isinit=0;
/*
static int kb_y_pos = 0;
static int set_kb_y_pos = -10000;
static int kb_activekey=-1;
static int sticky=0;
static unsigned char keysPressed[256];
*/
// static functions
// ================

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
//	s32 i;
//	svcWaitSynchronization(privateSem1, U64_MAX);
	// init texture
	C3D_TexDelete(tex);
	C3D_TexInit(tex, hw, hh, GPU_RGBA8);
	C3D_TexSetFilter(tex, GPU_NEAREST, GPU_NEAREST);

	// Convert image to 3DS tiled texture format
	GSPGPU_FlushDataCache(mygpusrc, hw*hh*4);
	C3D_SyncDisplayTransfer ((u32*)mygpusrc, GX_BUFFER_DIM(hw,hh), (u32*)(tex->data), GX_BUFFER_DIM(hw,hh), TEXTURE_TRANSFER_FLAGS);
	GSPGPU_FlushDataCache(tex->data, hw*hh*4);
//	svcReleaseSemaphore(&i, privateSem1, 1);
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

static SDL_Surface *myIMG_Load(char *fname, int paletted)
{
	SDL_Surface *s;
	png_image image = {0};
	image.version = PNG_IMAGE_VERSION;
	if (!png_image_begin_read_from_file(&image, fname))
		return NULL;
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
	SDL_Surface *s=myIMG_Load(fname, 0);
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

static void uib_repaint(void *param) {
	ENTER
//	s32 c;
//	int i;
	
	if (svcWaitSynchronization(repaintRequired, 0)) return;
	svcClearEvent(repaintRequired);
//	svcWaitSynchronization(privateSem1, U64_MAX);
/*
	if (set_kb_y_pos != -10000) {
		kb_y_pos=set_kb_y_pos;
		set_kb_y_pos=-10000;
	}
*/
	// Render the scene

	C3D_RenderTargetClear(VideoSurface2, C3D_CLEAR_ALL, CLEAR_COLOR, 0);
	C3D_FrameDrawOn(VideoSurface2);

	// background
//	drawImage(&background_spr, 0, 0, 0, 0, 0);

	// menu
	drawImage(&menu_spr, 0, 0, 0, 0, 0);

/*
	// keyboard
	DS3_Image *kb=(sticky & 1) == 1 ? &(kbd2_spr):&(kbd1_spr);
	drawImage(kb, 0, kb_y_pos, 0, 0, 0);
	// keyboard twisty
	DS3_Image *tw = kb_y_pos > (240 - kb->h) ? &(twistyup_spr):&(twistydn_spr);
	drawImage(tw, 0, kb_y_pos - tw->h, 0, 0, 0);
	// keys pressed
	uikbd_key *k;
	for (i=0;uikbd_keypos[i].key!=-1; ++i) {
		k=&(uikbd_keypos[i]);
		if (k->flags==1 || keysPressed[i]==0) continue;
		drawImage(&(keymask_spr),k->x,k->y+kb_y_pos,k->w,k->h,0);
	}
*/
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
/*
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

static void toggle_keyboard() {
	int y1=240-kbd1_spr.h;

	start_worker(animate, alloc_copy(&((int[]){
		0, 0, 1, // steps, delay, nr
		(int)anim_callback, 0, // callback, callback_data
		0,0, // callback2, callback2_data
		(int)(&set_kb_y_pos), kb_y_pos < 240 ? y1 : 240, kb_y_pos < 240 ? 240 : y1
	}), 10*sizeof(int)));
}
*/

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

#define DEF_TXT_COL COL_WHITE
#define DEF_BCK_COL COL_BLACK

static int uib_x=0, uib_y=0;
static SDL_Color txt_col = DEF_TXT_COL;
static SDL_Color bck_col = DEF_BCK_COL;

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

	chars_img=myIMG_Load("romfs:/chars.png", 1);
	SDL_SetColorKey(chars_img, SDL_SRCCOLORKEY, 0x00000000);

	// init menusurface
	u8* menu_gpusrc = (u8*)linearAlloc(512*256*4);
	menu_img = SDL_CreateRGBSurfaceFrom(menu_gpusrc, 320, 240, 32, 512*4, 0xff000000,0x00ff0000,0x0000ff00,0x000000ff);
	SDL_FillRect(menu_img, NULL, 0x000000ff);

	// pre-load sprites
//	loadImage(&background_spr,	"romfs:/bg_bot.png");
//	makeImage(&keymask_spr, (const u8[]){0x00, 0x00, 0x00, 0x80},1,1,0);

	// other stuff
//	kb_y_pos = 240 - kbd1_spr.h;

	svcCreateEvent(&repaintRequired, RESET_ONESHOT);
	SDL_RequestCall(uib_repaint, NULL);

	uib_must_redraw |= UIB_REPAINT;
	uib_update();
	atexit(uib_shutdown);
}

void uib_update(void)
{
	// init if needed
	if (!uib_isinit) uib_init();

	makeImage(&menu_spr, menu_img->pixels, menu_img->w, menu_img->h, 1);
	requestRepaint();
}


/*
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

static int get_timeout(TapState s)
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
    }
	return 0;
}

static int mouse_state = 0;
static int mainMenu_max_tap_time=250;
static int mainMenu_click_time=100;
static int mainMenu_single_tap_timeout=250;
static int mainMenu_max_double_tap_time=250;
static int mainMenu_locked_drag_timeout=5000;
static int mainMenu_tap_and_drag_gesture=1;
static int mainMenu_locked_drags=0;

static void set_tap_state(TapState s, SDL_Event *e)
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
		if (e) {
			ev.button.x = e->button.x;
			ev.button.y = e->button.y;
		}
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
	static TapState status = TS_START;
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
    }
	return 1;
}

// *************************
// Bottom screen event handling routine
// *************************

#define log2 __builtin_ctz

int uib_handle_event(SDL_Event *e) {
	static int sticky2keysym[4]={-1,-1,-1,-1};

	extern SDL_Surface *prSDLScreen;
	static SDL_Event sdl_e;
	int i,x,y;
	static int process_touchpad = 0;


	if (e->type == SDL_QUIT) {
		storeConfig();
		do_leave_program();
		exit(0);
	}

	if (e->type == SDL_KEYDOWN) {
		if (e->key.keysym.sym == 255) {
			toggle_keyboard();
			return 1;
		}
		if (e->key.keysym.sym == DS_TOUCH) return 1;
	}
	if (e->type != SDL_MOUSEMOTION &&
		e->type != SDL_MOUSEBUTTONUP &&
		e->type != SDL_MOUSEBUTTONDOWN) return 0;

	if (e->type == SDL_MOUSEMOTION) {
		return e->motion.state ? 0 : 1;
	} else {
		if (e->button.which == 1) return 0;
	}

	// check if we should do touchpad or virtual keyboard processing
	if (process_touchpad) {
		if (e->type == SDL_MOUSEBUTTONUP) process_touchpad=0;
		return uib_handle_tap_processing(e);
	}

	switch (e->type) {
		case SDL_MOUSEBUTTONUP:
			if (kb_activekey==-1) return 1; // did not get the button down, so ignore button up
			i=kb_activekey;
			kb_activekey=-1;
			break;
		case SDL_MOUSEBUTTONDOWN:
			x = (int)(((((double)e->button.x)*320.0f)/((double)prSDLScreen->w))+0.5f);
			y = e->button.y;
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
					return uib_handle_tap_processing(e);
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
			int pos=log2(uikbd_keypos[i].sticky & 0x0F);
			sticky = sticky ^ uikbd_keypos[i].sticky;
			if (sticky & uikbd_keypos[i].sticky) {
				sdl_e.type = SDL_KEYDOWN;
				sticky2keysym[pos] = sdl_e.key.keysym.sym = uikbd_keypos[i].key;
				SDL_PushEvent(&sdl_e);
			} else {
				sdl_e.type = SDL_KEYUP;
				sdl_e.key.keysym.sym = sticky2keysym[pos] == -1 ? uikbd_keypos[i].key : sticky2keysym[pos];
				sticky2keysym[pos] = -1;
				SDL_PushEvent(&sdl_e);
			}
		}
	} else {
		// normal key press
		sdl_e.type = e->button.type == SDL_MOUSEBUTTONDOWN ? SDL_KEYDOWN : SDL_KEYUP;
		sdl_e.key.keysym.sym = uikbd_keypos[i].key;
		SDL_PushEvent(&sdl_e);
	}
	uib_must_redraw |= UIB_RECALC_KEYPRESS;
	return 1;
}
*/