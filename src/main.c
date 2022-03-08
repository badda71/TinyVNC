/*
 * TinyVNC - A VNC client for Nintendo 3DS
 *
 * main.c - Main functionalities
 *
 * Copyright 2020 Sebastian Weber
 */

#include <malloc.h>
#include <stdio.h>
#include <sys/stat.h>
#include <errno.h>
#include <3ds.h>
#include <SDL/SDL.h>
#include <rfb/rfbclient.h>
#include <arpa/inet.h>
#include "streamclient.h"
#include "uibottom.h"
#include "utilities.h"
#include "vjoy-udp-feeder-client.h"
#include "dsu-server.h"

#define SOC_ALIGN       0x1000
#define SOC_BUFFERSIZE  0x100000
#define NUMCONF 25

#define HEADERCOL COL_MAKE(0x47, 0x80, 0x82)

typedef struct {
	char name[128];
	char host[128];
	int port;
	int audioport;
	char audiopath[128];
	char user[128];
	char pass[128];
	int scaling;
	int vncoff;
	int port2;
	int enablevnc2;
	int scaling2;
	int eventtarget; // 0: events are sent to top, 1: events are sent to bottom
	int notaphandling;
	int enableaudio;
	int hidelog;
	int backoff; // bottom backlight off?
	int hidekb;
	int ctr_vnc_keys;
	int ctr_vnc_touch;
	int ctr_udp_enable;
	int ctr_udp_port;
	int ctr_udp_motion;
	int ctr_dsu_enable;
	int ctr_dsu_port;
} vnc_config;

static vnc_config default_config = {
	.name = "",
	.host = "",
	.port = SERVER_PORT_OFFSET,
	.audioport = 80,
	.audiopath = "/",
	.user = "",
	.pass = "",
	.scaling = 1,
	.vncoff = 0,
	.port2 = SERVER_PORT_OFFSET+1,
	.enablevnc2 = 0,
	.scaling2 = 1,
	.eventtarget = 1,
	.notaphandling = 1,
	.enableaudio = 0,
	.hidelog = 0,
	.backoff = 0,
	.hidekb = 0,
	.ctr_vnc_keys = 1,
	.ctr_vnc_touch = 1,
	.ctr_udp_enable = 0,
	.ctr_udp_port = 1608,
	.ctr_udp_motion = 0,
	.ctr_dsu_enable = 0,
	.ctr_dsu_port = 26760
};

typedef struct {
	char name[128];
	char host[128];
	int port;
	int audioport;
	char audiopath[128];
	char user[128];
	char pass[128];
	int scaling;
} vnc_config_1_0;

typedef struct {
	char name[128];
	char host[128];
	int port;
	char user[128];
	char pass[128];
} vnc_config_0_9;

struct { int sdl; int rfb; } buttonMapping[]={
	{1, rfbButton1Mask},
	{2, rfbButton2Mask},
	{3, rfbButton3Mask},
	{4, rfbButton4Mask},
	{5, rfbButton5Mask},
	{0,0}
};

const char *config_filename = "/3ds/TinyVNC/vnc.cfg";
#define BUFSIZE 1024
static vnc_config conf[NUMCONF] = {0};
static int cpy = -1;
static u32 *SOC_buffer = NULL;
static int viewOnly=0, buttonMask=0;
/* client's pointer position */
int x=0,y=0;
rfbClient* cl;
rfbClient* cl2;
static SDL_Surface *bgimg;
SDL_Surface* sdl=NULL;
static int sdl_pos_x, sdl_pos_y;
static vnc_config config;
static int have_scrollbars=0;
aptHookCookie cookie;

extern void SDL_SetVideoPosition(int x, int y);
extern void SDL_ResetVideoPosition();

static void vwrite_log(const char *format, va_list arg, int channel)
{
	int i=vsnprintf(NULL, 0, format, arg);
	if (i) {
		char *buf = malloc(i+1);
		vsnprintf(buf, i+1, format, arg);
		while (i && buf[i-1]=='\n') buf[--i]=0; // strip trailing newlines
		if (channel & 2) svcOutputDebugString(buf, i);
		if (channel & 1 && !config.hidelog) {
			uib_printf("%s\n",buf);
			uib_update(UIB_RECALC_MENU);
			SDL_Flip(sdl);
		}
		free(buf);
	}
}

void log_citra(const char *format, ...) {
    va_list argptr;
    va_start(argptr, format);
	vwrite_log(format, argptr, 2);
    va_end(argptr);
}

void log_msg(const char *format, ...)
{
    va_list argptr;
    va_start(argptr, format);
	vwrite_log(format, argptr, 3);
    va_end(argptr);
}

void log_color(SDL_Color front, SDL_Color back, const char *format, ...)
{
	va_list argptr;
    va_start(argptr, format);
	uib_set_colors(front, back);
	vwrite_log(format, argptr, 3);
	uib_reset_colors();
    va_end(argptr);
}

void log_err(const char *format, ...)
{
	va_list argptr;
    va_start(argptr, format);
	uib_set_colors(COL_RED, COL_BLACK);
	vwrite_log(format, argptr, 3);
	uib_reset_colors();
    va_end(argptr);
}

static rfbBool resize(rfbClient* client) {
	int width=client->width;
	int height=client->height;
	int depth=client->format.bitsPerPixel;

	if (width > 1024 || height > 1024) {
		rfbClientErr("resize: screen size >1024px!");
		return FALSE;
	}

	client->updateRect.x = client->updateRect.y = 0;
	client->updateRect.w = width;
	client->updateRect.h = height;

	/* (re)create the surface used as the client's framebuffer */
	int flags = SDL_TOPSCR;
	if (config.scaling) {
		SDL_ResetVideoPosition();
		if (width > 400 || height > 240) {
			flags |= ((width * 1024) / 400 > (height * 1024) / 240)? SDL_FITWIDTH : SDL_FITHEIGHT;
		}
		uib_show_scrollbars(0,0,400,240);
	} else {
		have_scrollbars = (width>400?1:0) + (height>240?2:0);
		int w = have_scrollbars & 2 ? 398 : 400;
		int h = have_scrollbars & 1 ? 238 : 240;
		// center screen around mouse cursor if not scaling
		sdl_pos_x = width > w ? LIMIT(-x+w/2, -width+w, 0) : (w - width) / 2;
		sdl_pos_y = height > h ? LIMIT( -y+h/2, -height+h, 0) : (h - height) / 2;
		SDL_SetVideoPosition(sdl_pos_x, sdl_pos_y);
		uib_show_scrollbars(sdl_pos_x, sdl_pos_y, width, height);
	}

	sdl = SDL_SetVideoMode(width, height, depth, flags);
	SDL_ShowCursor(SDL_DISABLE);

	if(!sdl) {
		rfbClientErr("resize: error creating surface: %s", SDL_GetError());
		return FALSE;
	}
	SDL_FillRect(sdl,NULL, 0x00000000);
	SDL_Flip(sdl);

	client->width = sdl->pitch / (depth / 8);
	client->frameBuffer=sdl->pixels;

	client->format.bitsPerPixel=depth;
	client->format.redShift=sdl->format->Rshift;
	client->format.greenShift=sdl->format->Gshift;
	client->format.blueShift=sdl->format->Bshift;

	client->format.redMax=sdl->format->Rmask>>client->format.redShift;
	client->format.greenMax=sdl->format->Gmask>>client->format.greenShift;
	client->format.blueMax=sdl->format->Bmask>>client->format.blueShift;
	SetFormatAndEncodings(client);

	return TRUE;
}


static void cleanup()
{
  if(cl)
    rfbClientCleanup(cl);
  cl = NULL;
  if (cl2)
    rfbClientCleanup(cl2);
  cl2 = NULL;
  uibvnc_cleanup();
}

// function for translating the joystick events to keyboard events (analog to old SDL version)
#define THRESHOLD 16384
static void map_joy_to_key(SDL_Event *e)
{
	static int old_status = 0;
	static int old_dp_status = 0;
	static int meta = 0;
	int i1,i2, status;

	// key mappings =================================================================
	// values as per https://libvnc.github.io/doc/html/keysym_8h_source.html
	// 1 = meta button
	// 2 = toggle keyboard
	// 3 = disconnect
	// 4 = toggle scaling
	// 5 = toggle bottom screen backlight
	// 6 = toggle touch/button events target (top or bottom)
	// 16-20 = mouse button 1-5 (16=left, 17=middle, 18=right, 19=wheelup, 20=wheeldown)

	static int buttonkeys[10]={
		XK_Escape,	// START
		XK_a,	// A
		XK_b,	// B
		XK_x,	// X
		XK_y,	// Y
		XK_q,	// L
		XK_w,	// R
		1,		// SELECT (= meta)
		XK_1,	// ZL
		XK_2	// ZR
	};

	static int buttonkeys_meta[10]={
		3,		// START
		XK_A,	// A
		XK_B,	// B
		XK_C,	// X
		XK_Y,	// Y
		XK_Q,	// L
		XK_W,	// R
		1,		// SELECT (= meta)
		XK_3,	// ZL
		XK_4	// ZR
	};

	static int axiskeys[8]={
		XK_Right,	// C-PAD RIGHT
		XK_Left,	// C-PAD LEFT
		XK_Down,	// C-PAD DOWN
		XK_Up,	// C-PAD UP
		XK_l, // C-STICK RIGHT
		XK_j, // C-STICK LEFT
		XK_k, // C-STICK DOWN
		XK_i // C-STICK UP
	};
	static int axiskeys_meta[8]={
		XK_Right,	// C-PAD RIGHT
		XK_Left,	// C-PAD LEFT
		XK_Down,	// C-PAD DOWN
		XK_Up,	// C-PAD UP
		XK_L, // C-STICK RIGHT
		XK_J, // C-STICK LEFT
		XK_K, // C-STICK DOWN
		XK_I // C-STICK UP
	};

	static int hatkeys[4]={
		XK_t,	// D-PAD UP
		XK_h,	// D-PAD RIGHT
		XK_g,	// D-PAD DOWN
		XK_f	// D-PAD LEFT
	};
	static int hatkeys_meta[4]={
		XK_T,	// D-PAD UP
		XK_H,	// D-PAD RIGHT
		XK_G,	// D-PAD DOWN
		XK_F	// D-PAD LEFT
	};

	switch (e->type) {
	case SDL_JOYHATMOTION:
		status = e->jhat.value;
		if (status != old_dp_status) {
			for (i1 = 0; i1 < 4; ++i1) {
				i2 = 1 << i1;
				if ((status & i2) != (old_dp_status & i2))
					push_key_event(meta?hatkeys_meta[i1]:hatkeys[i1], status & i2);
			}
			old_dp_status = status;
		}
		break;
	case SDL_JOYBUTTONDOWN:
	case SDL_JOYBUTTONUP:
		i1 = e->type == SDL_JOYBUTTONDOWN ? 1 : 0;
		if (buttonkeys[e->jbutton.button] == 1) {
			meta = i1;
		} else {
			make_key_event(e,
				meta?buttonkeys_meta[e->jbutton.button]:buttonkeys[e->jbutton.button],
				i1);
		}
		break;
	case SDL_JOYAXISMOTION:
		i1 = 3 << (e->jaxis.axis * 2);
		i2 = (e->jaxis.value > THRESHOLD ? 1 : (e->jaxis.value < -THRESHOLD ? 2 : 0)) << (e->jaxis.axis * 2);
		status = (old_status & (~i1)) | i2;
		if (old_status != status) {
			for (i1 = 0; i1 < 8; ++i1) {
				i2 = 1 << i1;
				if ((status & i2) != (old_status & i2))
					push_key_event(meta?axiskeys_meta[i1]:axiskeys[i1], status & i2);
			}
			old_status = status;
		}
		break;
	default:
		break;
	}
}

static void safeexit();

static void record_mousebutton_event(int which, int pressed) {
	for (int i = 0; buttonMapping[i].sdl; i++) {
		if (which == buttonMapping[i].sdl) {
			which = buttonMapping[i].rfb;
			if (pressed)
				buttonMask |= which;
			else
				buttonMask &= ~which;
			break;
		}
	}
}

static SDL_Event scheduled = {0};
static void schedule_event(SDL_Event e) {
	scheduled = e;
}

static void push_scheduled_event() {
	if (scheduled.type) {
		SDL_PushEvent(&scheduled);
		scheduled.type = 0;
	}
}

extern int uibvnc_w, uibvnc_h, uibvnc_x, uibvnc_y;
#define SCROLL_SIZE 20 // size of the scrolling arean in px
#define SCROLL_SPEED 200 // max scrolling speed in px/sec

// event handler while VNC is running
static rfbBool handleSDLEvent(SDL_Event *e)
{
	// pointer positions
	static double xf=0.0,yf=0.0;
	
	int s;
	map_joy_to_key(e);

	switch(e->type) {
	case SDL_MOUSEBUTTONUP:
	case SDL_MOUSEBUTTONDOWN:
	case SDL_MOUSEMOTION:
	{
		if (viewOnly)
			break;

		rfbClient *tcl = (cl2 && config.eventtarget)?cl2:cl;

		if (tcl != cl) { // bottom screen always uses direct coodinates, not relative
			// get and translate the positions
			int x1=(e->type == SDL_MOUSEMOTION ? e->motion.x : e->button.x) * 320 / cl->updateRect.w;
			int y1=(e->type == SDL_MOUSEMOTION ? e->motion.y : e->button.y) * 240 / cl->updateRect.h;
			x = ((x1 - uibvnc_x) * tcl->updateRect.w) / uibvnc_w;
			y = ((y1 - uibvnc_y) * tcl->updateRect.h) / uibvnc_h;
			xf=(double)x;
			yf=(double)y;

			// handle bottom screen scrolling - only if we are not scaling of course
			if (!config.scaling2) {
				static int scrolling = 0;
				static double xposf, yposf;

				double dxf=0.0, dyf=0.0;
				if (uibvnc_w > 320) {
					if (x1 >= 320 - SCROLL_SIZE && uibvnc_x > 320-uibvnc_w)
						dxf = -(double)(((x1 - 319 + SCROLL_SIZE) * SCROLL_SPEED) / SCROLL_SIZE) / 50.0;
					if (x1 < SCROLL_SIZE && uibvnc_x < 0)
						dxf = (double)(((-x1 + SCROLL_SIZE) * SCROLL_SPEED) / SCROLL_SIZE) / 50.0;
				}
				if (uibvnc_h > 240) {
					if (y1 >= 240 - SCROLL_SIZE && uibvnc_y > 240-uibvnc_h)
						dyf = -(double)(((y1 - 239 + SCROLL_SIZE) * SCROLL_SPEED) / SCROLL_SIZE) / 50.0;
					if (y1 < SCROLL_SIZE && uibvnc_y < 0)
						dyf = (double)(((-y1 + SCROLL_SIZE) * SCROLL_SPEED) / SCROLL_SIZE) / 50.0;
				}
				if (dxf != 0.0 || dyf != 0.0) {
					if (scrolling == 0) {
						xposf = (double)uibvnc_x;
						yposf = (double)uibvnc_y;
						if (e->type == SDL_MOUSEBUTTONDOWN || (e->type == SDL_MOUSEMOTION && e->motion.which != 2))
							scrolling = 1;
					}
					if (dxf != 0.0) {
						xposf += dxf;
						uibvnc_x = (int)xposf;
						int w = uibvnc_h>240 ? 318 : 320;
						if (uibvnc_x > 0) uibvnc_x = 0;
						if (uibvnc_x < w - uibvnc_w) uibvnc_x =  w - uibvnc_w;
					}
					if (dyf != 0.0) {
						yposf += dyf;
						uibvnc_y = (int)yposf;
						int h = uibvnc_w>320 ? 238 : 240;
						if (uibvnc_y > 0) uibvnc_y = 0;
						if (uibvnc_y < h - uibvnc_h) uibvnc_y = h - uibvnc_h;
					}
					if (scrolling) {
						u32 kHeld = hidKeysHeld();
						if (kHeld & KEY_TOUCH) {
//log_citra("pushing event, dx: %f, dy: %f, newxf: %f, newyf: %f, x: %d, y: %d", dxf, dyf, xposf, yposf, uibvnc_x, uibvnc_y);
							schedule_event((SDL_Event){
								.motion.type = SDL_MOUSEMOTION,
								.motion.state = 1,
								.motion.which = 2, // identify as autoscrolling
								.motion.x = (x1 * cl->updateRect.w) / 320,
								.motion.y = (y1 * cl->updateRect.h) / 240
							});
						}
					}
				} else {
//log_citra("scrolling off");
					scrolling = 0;
				}
			}
		}

		if (e->type == SDL_MOUSEMOTION) {
			if (tcl == cl) {
				double xrel = (double)e->motion.xrel * (config.scaling?1.0:(400.0 / (double)cl->updateRect.w));
				double yrel = (double)e->motion.yrel * (config.scaling?1.0:(240.0 / (double)cl->updateRect.h));
				xf += xrel;
				if (xf < 0.0) xf=0.0;
				if (xf > (double)tcl->updateRect.w) xf=(double)tcl->updateRect.w;
				yf += yrel;
				if (yf < 0.0) yf=0.0;
				if (yf > (double)tcl->updateRect.h) yf = (double)tcl->updateRect.h;
				x=(int)xf; y=(int)yf;

				// if not scaling and pointer is outside display area, scroll the display
				if (!config.scaling) {
					int w = have_scrollbars & 2 ? 398 : 400;
					int h = have_scrollbars & 1 ? 238 : 240;
					if (x < -sdl_pos_x)		sdl_pos_x = -x;
					if (x > -sdl_pos_x + w)	sdl_pos_x = -x + w;
					if (y < -sdl_pos_y)		sdl_pos_y = -y;
					if (y > -sdl_pos_y + h)	sdl_pos_y = -y + h;
					SDL_SetVideoPosition(sdl_pos_x, sdl_pos_y);
					uib_show_scrollbars(sdl_pos_x, sdl_pos_y, 0, 0);
				}
			}
		}
		else {
			record_mousebutton_event(e->button.button, e->type == SDL_MOUSEBUTTONDOWN?1:0);
		}
		//log_citra("pointer event: x %d, y %d, mask %p",x, y, buttonMask);
		if (config.ctr_vnc_touch) SendPointerEvent(tcl, x, y, buttonMask);
		buttonMask &= ~(rfbButton4Mask | rfbButton5Mask); // clear wheel up and wheel down state
		break;
	}
	case SDL_KEYUP:
	case SDL_KEYDOWN:
		s =  e->key.keysym.sym;
		if (s == 3)
			return 0; // disconnect
		if (viewOnly) break;
		if (s>15 && s<21) {
			// mouse button 1-5: 16-20
			record_mousebutton_event(s-15, e->type == SDL_KEYDOWN?1:0);
			if (config.ctr_vnc_touch) SendPointerEvent((cl2 && config.eventtarget)?cl2:cl, x, y, buttonMask);
			buttonMask &= ~(rfbButton4Mask | rfbButton5Mask); // clear wheel up and wheel down state
		} else if (s == 4) {
			if (e->type == SDL_KEYDOWN) {
				// toggle top screen scaling
				config.scaling = !config.scaling;
				// resize the SDL screen
				resize(cl);
				SendFramebufferUpdateRequest(cl, 0, 0, cl->width, cl->height, FALSE);
			}
		} else if (s == 5) {
			if (e->type == SDL_KEYDOWN) {
				// toggle bottom screen backlight
				uib_setBacklight(!uib_getBacklight());
			}
		} else if (s == 6) {
			if (cl2 && e->type == SDL_KEYDOWN) {
				// toggle event target
				config.eventtarget = !config.eventtarget;
			}
		} else {
			if (config.ctr_vnc_keys) SendKeyEvent((cl2 && config.eventtarget)?cl2:cl, s, e->type == SDL_KEYDOWN ? TRUE : FALSE);
		}
		break;
	case SDL_QUIT:
		safeexit();
		break;
	default:
		break;
	}
	return TRUE;
}

//static void get_selection(rfbClient *cl, const char *text, int len){}
char *user1, *pass1;

static char* get_password(rfbClient* cl) {
	char* p=calloc(1,strlen(pass1)+1);
	strcpy(p, pass1);
	return p;
}

static rfbCredential* get_credential(rfbClient* cl, int credentialType) {
	rfbCredential *c = malloc(sizeof(rfbCredential));
	c->userCredential.username = malloc(RFB_BUF_SIZE);
	c->userCredential.password = malloc(RFB_BUF_SIZE);

	if(credentialType != rfbCredentialTypeUser) {
	    rfbClientErr("Something else than username and password required for authentication");
	    return NULL;
	}

	rfbClientLog("Username and password required for authentication!");
	snprintf(c->userCredential.username, RFB_BUF_SIZE, "%s", user1);
	snprintf(c->userCredential.password, RFB_BUF_SIZE, "%s", pass1);
	return c;
}

static int mkpath(const char* file_path1, int complete) {
	char *file_path=strdup(file_path1);
	char* p;

	for (p=strchr(file_path+1, '/'); p; p=strchr(p+1, '/')) {
		*p='\0';
		if (mkdir(file_path, 0644)==-1) {
			if (errno!=EEXIST) { *p='/'; goto mkpath_err; }
		}
		*p='/';
	}
	if (complete) {
		mkdir(file_path, 0644);
	}
	free(file_path);
	return 0;
mkpath_err:
	free(file_path);
	return 1;
}

static void printlist(int num) {
	char buf[41];
	
	uib_clear();
	uib_set_colors(COL_BLACK, HEADERCOL);
	
	uib_printf(	" %-38.38s ", "TinyVNC v" VERSION " by badda71");
	if (cpy!=-1) {
		snprintf(buf,41,"L:paste '%s'                              ",conf[cpy].name);
		uib_set_position(0,28);
		uib_printf("%s", buf);
	}
	uib_set_position(0,29);
	uib_printf(	"A:select B:quit X:delete R:copy Y:edit  ");
	uib_reset_colors();
	for (int i=0;i<NUMCONF; ++i) {
		uib_set_position(0,i+2);
		if (i==num) uib_invert_colors();
		uib_printf("%-40.40s", conf[i].host[0]?conf[i].name:"-");
		if (i==num) uib_reset_colors();
	}
	uib_update(UIB_RECALC_MENU);
}

static void saveconfig() {
	FILE *f;
	if((f=fopen(config_filename, "w"))!=NULL) {
		fwrite((void*)conf, sizeof(vnc_config), NUMCONF, f);
		fclose(f);
	}
}

static void checkset(char *name, char *nhost, int nport, char *nuser, char *ohost, int oport, char *ouser) {
	int upd=0;
	char buf[128], buf2[128];
	if (name[0]) {
		snprintf(buf, sizeof(buf), "%s%s%s%s%s",
			ouser, ouser[0]?"@":"",
			ohost, oport != SERVER_PORT_OFFSET?":":"", oport != SERVER_PORT_OFFSET?itoa(oport, buf2, 10):"");
		if (strcmp(name, buf)==0) upd=1;
	} else upd=1;
	if (upd) {
		snprintf(name, 128, "%s%s%s%s%s",
			nuser, nuser[0]?"@":"",
			nhost, nport != SERVER_PORT_OFFSET?":":"", nport != SERVER_PORT_OFFSET?itoa(nport, buf2, 10):"");
	}
}

enum {
	EDITCONF_NAME = 0,
	EDITCONF_HOST,
	EDITCONF_PORT,
	EDITCONF_USER,
	EDITCONF_PASS,
	EDITCONF_SCALING,
	EDITCONF_VNCOFF,
	EDITCONF_ENABLEVNC2,
	EDITCONF_PORT2,
	EDITCONF_SCALING2,
	EDITCONF_EVENTTARGET,
	EDITCONF_NOTAPHANDLING,
	EDITCONF_HIDELOG,
	EDITCONF_BACKOFF,
	EDITCONF_HIDEKB,
	EDITCONF_ENABLEAUDIO,
	EDITCONF_AUDIOPORT,
	EDITCONF_AUDIOPATH,
	EDITCONF_CTRVNCKEYS,
	EDITCONF_CTRVNCTOUCH,
	EDITCONF_CTRUDPENABLE,
	EDITCONF_CTRUDPPORT,
	EDITCONF_CTRUDPMOTION,
	EDITCONF_CTRDSUENABLE,
	EDITCONF_CTRDSUPORT,
	EDITCONF_END
};

static int editconfig(vnc_config *c) {
	int sel = EDITCONF_HOST;
	int page = 0;
	const char *pass = "****************************************";
	char input[128];
	int ret = 0;
	static SwkbdState swkbd;
	SDL_Event e;
	SwkbdButton button;
	char *msg=NULL;
	int showmsg = 0, msg_state=0;
	struct in_addr sin_addr;
	char myip[INET_ADDRSTRLEN];
	sin_addr.s_addr = gethostid();
	inet_ntop( AF_INET, &sin_addr, myip, INET_ADDRSTRLEN );

	vnc_config nc = *c;
	if (nc.host[0]==0) nc = default_config;

	int upd = 1;
	while (ret==0) {
		// blink message
		int old_state=showmsg;
		if (msg) {
			if (msg_state > 32) showmsg = 1;
			else {
				++msg_state;
				showmsg= (msg_state / 4) % 2 == 0 ? 1 : 0;
			}
		} else msg_state=0;
		if (upd || showmsg != old_state) {
			uib_clear();
			uib_set_colors(COL_BLACK, HEADERCOL);
			uib_printf(	" Edit VNC Session                       ");
			uib_reset_colors();

			int l=2;
			uib_set_position(0,l);
			uib_printf(	"Name: ");
			if (sel == EDITCONF_NAME) uib_invert_colors();
			uib_printf(	"%-34.34s", nc.name);
			if (sel == EDITCONF_NAME) uib_reset_colors();
			++l;

			uib_set_colors(COL_BLACK, page==0?COL_WHITE:COL_GRAY);
			uib_set_position(1,++l);
			uib_printf( "  VNC / UI  " );
			uib_set_colors(COL_BLACK, page==1?COL_WHITE:COL_GRAY);
			uib_set_position(14,l);
			uib_printf( " Audio/Ctrl " );
			uib_reset_colors();
			++l;

			if (page == 0) {
				uib_set_colors(HEADERCOL, COL_BLACK);
				uib_set_position(0,++l);
				uib_printf( "--------- Main VNC connection ----------");
				uib_reset_colors();
				uib_set_position(0,++l);
				uib_printf(	"Host: ");
				if (sel == EDITCONF_HOST) uib_invert_colors();
				uib_printf(	"%-34.34s", nc.host);
				if (sel == EDITCONF_HOST) uib_reset_colors();
				uib_set_position(0,++l);
				uib_printf(	"Port: ");
				if (sel == EDITCONF_PORT)uib_invert_colors();
				uib_printf(	"%-34d", nc.port);
				if (sel == EDITCONF_PORT) uib_reset_colors();
				uib_set_position(0,++l);
				uib_printf(	"Username: ");
				if (sel == EDITCONF_USER) uib_invert_colors();
				uib_printf(	"%-30.30s", nc.user);
				if (sel == EDITCONF_USER) uib_reset_colors();
				uib_set_position(0,++l);
				uib_printf(	"Password: ");
				if (sel == EDITCONF_PASS) uib_invert_colors();
				uib_printf(	"%*.*s%*s", strlen(nc.pass), strlen(nc.pass), pass, 30-strlen(nc.pass), "");
				if (sel == EDITCONF_PASS) uib_reset_colors();
				uib_set_position(0,++l);
				uib_printf(nc.scaling?"\x91 ":"\x90 ");
				if (sel == EDITCONF_SCALING) uib_invert_colors();
				uib_printf(	"Scale to fit screen");
				if (sel == EDITCONF_SCALING) uib_reset_colors();
				uib_set_position(0,++l);
				uib_printf(nc.vncoff?"\x91 ":"\x90 ");
				if (sel == EDITCONF_VNCOFF) uib_invert_colors();
				uib_printf(	"Disable VNC connection");
				if (sel == EDITCONF_VNCOFF) uib_reset_colors();
				++l;
				
				uib_set_colors(HEADERCOL, COL_BLACK);
				uib_set_position(0,++l);
				uib_printf(	"--------- 2nd VNC connection -----------" );
				uib_reset_colors();
				uib_set_position(0,++l);
				uib_printf(nc.enablevnc2?"\x91 ":"\x90 ");
				if (sel == EDITCONF_ENABLEVNC2) uib_invert_colors();
				uib_printf(	"Enable bottom screen VNC");
				if (sel == EDITCONF_ENABLEVNC2) uib_reset_colors();
				if (nc.enablevnc2) {
					uib_set_position(0,++l);
					uib_printf(	"Bottom screen port: ");
					if (sel == EDITCONF_PORT2)uib_invert_colors();
					uib_printf(	"%-20d", nc.port2);
					if (sel == EDITCONF_PORT2) uib_reset_colors();
					uib_set_position(0,++l);
					uib_printf(nc.scaling2?"\x91 ":"\x90 ");
					if (sel == EDITCONF_SCALING2) uib_invert_colors();
					uib_printf(	"Scale to fit bottom screen");
					if (sel == EDITCONF_SCALING2) uib_reset_colors();
					uib_set_position(0,++l);
					uib_printf(nc.eventtarget?"\x91 ":"\x90 ");
					if (sel == EDITCONF_EVENTTARGET) uib_invert_colors();
					uib_printf(	"Send touch/button events to bottom");
					if (sel == EDITCONF_EVENTTARGET) uib_reset_colors();
					if (nc.eventtarget) {
						uib_set_position(0,++l);
						uib_printf(nc.notaphandling?"\x91 ":"\x90 ");
						if (sel == EDITCONF_NOTAPHANDLING) uib_invert_colors();
						uib_printf(	"Disable tap gestures");
						if (sel == EDITCONF_NOTAPHANDLING) uib_reset_colors();
					} else ++l;
				} else l+=4;
				++l;

				uib_set_colors(HEADERCOL, COL_BLACK);
				uib_set_position(0,++l);
				uib_printf(	"--------- User Interface ---------------" );
				uib_reset_colors();
				uib_set_position(0,++l);
				uib_printf(nc.hidelog?"\x91 ":"\x90 ");
				if (sel == EDITCONF_HIDELOG) uib_invert_colors();
				uib_printf(	"Hide Log");
				if (sel == EDITCONF_HIDELOG) uib_reset_colors();
				uib_set_position(17,l);
				uib_printf(nc.backoff?"\x91 ":"\x90 ");
				if (sel == EDITCONF_BACKOFF) uib_invert_colors();
				uib_printf(	"Bottom backlight off");
				if (sel == EDITCONF_BACKOFF) uib_reset_colors();
				uib_set_position(0,++l);
				uib_printf(nc.hidekb?"\x91 ":"\x90 ");
				if (sel == EDITCONF_HIDEKB) uib_invert_colors();
				uib_printf(	"Hide Keyboard");
				if (sel == EDITCONF_HIDEKB) uib_reset_colors();
			}
			else if (page == 1) {
				uib_set_colors(HEADERCOL, COL_BLACK);
				uib_set_position(0,++l);
				uib_printf(	"--------- Audio Stream -----------------" );
				uib_reset_colors();
				uib_set_position(0,++l);
				uib_printf(nc.enableaudio?"\x91 ":"\x90 ");
				if (sel == EDITCONF_ENABLEAUDIO) uib_invert_colors();
				uib_printf(	"Enable audio stream");
				if (sel == EDITCONF_ENABLEAUDIO) uib_reset_colors();
				if (nc.enableaudio) {
					uib_set_position(0,++l);
					uib_printf(	"Audio Stream Port: ");
					if (sel == EDITCONF_AUDIOPORT) uib_invert_colors();
					uib_printf(	"%-21s", nc.audioport?itoa(nc.audioport,input,10):"");
					if (sel == EDITCONF_AUDIOPORT) uib_reset_colors();
					uib_set_position(0,++l);
					uib_printf(	"Audio Stream Path: ");
					if (sel == EDITCONF_AUDIOPATH) uib_invert_colors();
					uib_printf(	"%-21s", nc.audiopath);
					if (sel == EDITCONF_AUDIOPATH) uib_reset_colors();
				} else l+=2;
				++l;
				uib_set_colors(HEADERCOL, COL_BLACK);
				uib_set_position(0,++l);
				uib_printf(	"--------- Controller -------------------" );
				uib_reset_colors();
				uib_set_position(0,++l);
				uib_printf(nc.ctr_vnc_keys?"\x91 ":"\x90 ");
				if (sel == EDITCONF_CTRVNCKEYS) uib_invert_colors();
				uib_printf(	"Send keys to VNC connection" );
				if (sel == EDITCONF_CTRVNCKEYS) uib_reset_colors();
				uib_set_position(0,++l);
				uib_printf(nc.ctr_vnc_touch?"\x91 ":"\x90 ");
				if (sel == EDITCONF_CTRVNCTOUCH) uib_invert_colors();
				uib_printf(	"Send mouse to VNC connection" );
				if (sel == EDITCONF_CTRVNCTOUCH) uib_reset_colors();
				++l;
				uib_set_position(0,++l);
				uib_printf(nc.ctr_udp_enable?"\x91 ":"\x90 ");
				if (sel == EDITCONF_CTRUDPENABLE) uib_invert_colors();
				uib_printf(	"Enable vJoy-UDP-Feeder client" );
				if (sel == EDITCONF_CTRUDPENABLE) uib_reset_colors();
				if (nc.ctr_udp_enable) {
					uib_set_position(0,++l);
					uib_printf(	"vJoy-UDP-Feeder Port: ");
					if (sel == EDITCONF_CTRUDPPORT)uib_invert_colors();
					uib_printf(	"%-18d", nc.ctr_udp_port);
					if (sel == EDITCONF_CTRUDPPORT) uib_reset_colors();
					uib_set_position(0,++l);
					uib_printf(nc.ctr_udp_motion?"\x91 ":"\x90 ");
					if (sel == EDITCONF_CTRUDPMOTION) uib_invert_colors();
					uib_printf(	"Send motion data to vJoy-UDP-Feeder" );
					if (sel == EDITCONF_CTRUDPMOTION) uib_reset_colors();					
				} else l+=2;
				++l;
				uib_set_position(0,++l);
				uib_printf(nc.ctr_dsu_enable?"\x91 ":"\x90 ");
				if (sel == EDITCONF_CTRDSUENABLE) uib_invert_colors();
				uib_printf(	"Enable cemuhook UDP server" );
				if (sel == EDITCONF_CTRDSUENABLE) uib_reset_colors();
				if (nc.ctr_dsu_enable) {
					uib_set_position(0,++l);
					uib_printf(	"My IP: %s", myip);
					uib_set_position(0,++l);
					uib_printf(	"Cemuhook server port: ");
					if (sel == EDITCONF_CTRDSUPORT)uib_invert_colors();
					uib_printf(	"%-18d", nc.ctr_dsu_port);
					if (sel == EDITCONF_CTRDSUPORT) uib_reset_colors();
				} else l+=2;
			}
			if (msg && showmsg) {
				uib_invert_colors();
				uib_set_position(40-strlen(msg),28);
				uib_printf("%s",msg);
				uib_reset_colors();
			}
			uib_set_colors(COL_BLACK, HEADERCOL);
			uib_set_position(0,29);
			uib_printf(	"A:edit B:cancel Y:OK L/R:change page    ");
			uib_reset_colors();

			uib_update(UIB_RECALC_MENU);
		}
		SDL_Flip(sdl);
		while (SDL_PollEvent(&e)) {
			if (uib_handle_event(&e, 0)) continue;
			if (e.type == SDL_QUIT)
				safeexit();
			map_joy_to_key(&e);
			if (e.type == SDL_KEYDOWN) {
				switch (e.key.keysym.sym) {
				case XK_q:
				case XK_w:
					page = (page + 1) % 2;
					if (page == 0 && sel != EDITCONF_NAME)
						sel = EDITCONF_HOST;
					if (page == 1 && sel != EDITCONF_NAME)
						sel = EDITCONF_ENABLEAUDIO;
					upd = 1; break;
				case XK_b:
					ret=-1; break;
				case XK_y:
					// validate input
					if (!nc.host[0]) {
						msg = "Host name is required";
					} else ret=1;
					break;
				case XK_Down:
				case XK_Right:
				case XK_g:
				case XK_h:
				case XK_k:
				case XK_l:
					sel = (sel + 1) % EDITCONF_END;
					if (page == 0) {
						if (sel == EDITCONF_ENABLEAUDIO) sel = 0;
						if (!nc.enablevnc2 && sel==EDITCONF_PORT2) sel=EDITCONF_HIDELOG;
						if (!nc.eventtarget && sel==EDITCONF_NOTAPHANDLING) sel=EDITCONF_HIDELOG;
					} else if (page == 1) {
						if (sel != 0 && sel < EDITCONF_ENABLEAUDIO) sel=EDITCONF_ENABLEAUDIO;
						if (!nc.enableaudio && sel==EDITCONF_AUDIOPORT) sel=EDITCONF_CTRVNCKEYS;
						if (!nc.ctr_udp_enable && sel==EDITCONF_CTRUDPPORT) sel=EDITCONF_CTRDSUENABLE;
						if (!nc.ctr_dsu_enable && sel==EDITCONF_CTRDSUPORT) sel=0;
					}
					upd = 1;
					break;
				case XK_Up:
				case XK_Left:
				case XK_t:
				case XK_f:
				case XK_i:
				case XK_j:
					sel = (sel + EDITCONF_END - 1) % EDITCONF_END;
					if (page == 0) {
						if (sel >= EDITCONF_ENABLEAUDIO) sel = EDITCONF_ENABLEAUDIO-1;
						if (!nc.enablevnc2 && sel==EDITCONF_NOTAPHANDLING) sel=EDITCONF_ENABLEVNC2;
						if (!nc.eventtarget && sel==EDITCONF_NOTAPHANDLING) sel=EDITCONF_EVENTTARGET;
					} else if (page == 1) {
						if (sel < EDITCONF_ENABLEAUDIO) sel = 0;
						if (!nc.enableaudio && sel==EDITCONF_AUDIOPATH) sel=EDITCONF_ENABLEAUDIO;
						if (!nc.ctr_udp_enable && sel==EDITCONF_CTRUDPMOTION) sel=EDITCONF_CTRUDPENABLE;
						if (!nc.ctr_dsu_enable && sel==EDITCONF_CTRDSUPORT) sel=EDITCONF_CTRDSUENABLE;
					}
					upd = 1;
					break;
				case XK_a:
					upd = 1;
					switch (sel) {
					case EDITCONF_NAME: // entry name
						swkbdInit(&swkbd, SWKBD_TYPE_QWERTY, 2, sizeof(input)-1);
						swkbdSetHintText(&swkbd, "Entry Name");
						swkbdSetInitialText(&swkbd, nc.name);
						swkbdSetFeatures(&swkbd, SWKBD_DEFAULT_QWERTY);
						button = swkbdInputText(&swkbd, input, sizeof(input));
						if(button != SWKBD_BUTTON_LEFT)
							strcpy(nc.name, input);
						break;
					case EDITCONF_HOST: // hostname
						swkbdInit(&swkbd, SWKBD_TYPE_QWERTY, 2, sizeof(input)-1);
						swkbdSetHintText(&swkbd, "Host Name / IP address");
						swkbdSetInitialText(&swkbd, nc.host);
						swkbdSetFeatures(&swkbd, SWKBD_DEFAULT_QWERTY);
						button = swkbdInputText(&swkbd, input, sizeof(input));
						if(button != SWKBD_BUTTON_LEFT) {
							checkset(nc.name, input, nc.port, nc.user, nc.host, nc.port, nc.user);
							strcpy(nc.host, input);
						}
						break;
					case EDITCONF_PORT: // port
						swkbdInit(&swkbd, SWKBD_TYPE_NUMPAD, 2, 5);
						swkbdSetHintText(&swkbd, "Port");
						sprintf(input, "%d", nc.port);
						swkbdSetInitialText(&swkbd, input);
						//swkbdSetFeatures(&swkbd, SWKBD_DEFAULT_QWERTY);
						button = swkbdInputText(&swkbd, input, 6);
						if(button != SWKBD_BUTTON_LEFT) {
							int po = atoi(input);
							if (po <= 0) po=1;
							if (po > 0xffff) po=0xffff;
							checkset(nc.name, nc.host, po, nc.user, nc.host, nc.port, nc.user);
							nc.port = po;
						}
						break;
					case EDITCONF_USER: // username
						swkbdInit(&swkbd, SWKBD_TYPE_QWERTY, 2, sizeof(input)-1);
						swkbdSetHintText(&swkbd, "Username");
						swkbdSetInitialText(&swkbd, nc.user);
						swkbdSetFeatures(&swkbd, SWKBD_DEFAULT_QWERTY);
						button = swkbdInputText(&swkbd, input, sizeof(input));
						if(button != SWKBD_BUTTON_LEFT) {
							checkset(nc.name, nc.host, nc.port, input, nc.host, nc.port, nc.user);
							strcpy(nc.user, input);
						}
						break;
					case EDITCONF_PASS: // password
						swkbdInit(&swkbd, SWKBD_TYPE_WESTERN, 2, sizeof(input)-1);
						swkbdSetHintText(&swkbd, "Password");
						swkbdSetInitialText(&swkbd, nc.pass);
						swkbdSetFeatures(&swkbd, SWKBD_DEFAULT_QWERTY);
						swkbdSetPasswordMode(&swkbd, SWKBD_PASSWORD_HIDE_DELAY);
						button = swkbdInputText(&swkbd, input, sizeof(input));
						if(button != SWKBD_BUTTON_LEFT)
							strcpy(nc.pass, input);
						break;
					case EDITCONF_SCALING: // top screen scaling on/off
						nc.scaling = !nc.scaling;
						break;
					case EDITCONF_VNCOFF: // disable top screen vnc
						nc.vncoff = !nc.vncoff;
						break;
					case EDITCONF_ENABLEVNC2: // enable bottom screen vnc
						nc.enablevnc2 = !nc.enablevnc2;
						break;
					case EDITCONF_PORT2: // bottom screen port
						swkbdInit(&swkbd, SWKBD_TYPE_NUMPAD, 2, 5);
						swkbdSetHintText(&swkbd, "Bottom Screen Port");
						sprintf(input, "%d", nc.port2);
						swkbdSetInitialText(&swkbd, input);
						//swkbdSetFeatures(&swkbd, SWKBD_DEFAULT_QWERTY);
						button = swkbdInputText(&swkbd, input, 6);
						if(button != SWKBD_BUTTON_LEFT) {
							int po = atoi(input);
							if (po <= 0) po=1;
							if (po > 0xffff) po=0xffff;
							nc.port2 = po;
						}
						break;
					case EDITCONF_SCALING2: // bottom screen scaling on/off
						nc.scaling2 = !nc.scaling2;
						break;
					case EDITCONF_EVENTTARGET: // mouse to top/bottom
						nc.eventtarget = !nc.eventtarget;
						break;
					case EDITCONF_NOTAPHANDLING: // disable tap handling on touch screen
						nc.notaphandling = !nc.notaphandling;
						break;
					case EDITCONF_ENABLEAUDIO: // enable audio stream
						nc.enableaudio = !nc.enableaudio;
						break;
					case EDITCONF_AUDIOPORT: // audio port
						swkbdInit(&swkbd, SWKBD_TYPE_NUMPAD, 2, 5);
						swkbdSetHintText(&swkbd, "Audio Port");
						sprintf(input, "%d", nc.audioport);
						swkbdSetInitialText(&swkbd, input);
						//swkbdSetFeatures(&swkbd, SWKBD_DEFAULT_QWERTY);
						button = swkbdInputText(&swkbd, input, 6);
						if(button != SWKBD_BUTTON_LEFT) {
							int po = atoi(input);
							if (po <= 0) po=1;
							if (po > 0xffff) po=0xffff;
							nc.audioport = po;
						}
						break;
					case EDITCONF_AUDIOPATH: // audio path
						swkbdInit(&swkbd, SWKBD_TYPE_QWERTY, 2, sizeof(input)-1);
						swkbdSetHintText(&swkbd, "Audio Path");
						swkbdSetInitialText(&swkbd, nc.audiopath);
						swkbdSetFeatures(&swkbd, SWKBD_DEFAULT_QWERTY);
						button = swkbdInputText(&swkbd, input, sizeof(input));
						if(button != SWKBD_BUTTON_LEFT) {
							snprintf(nc.audiopath, sizeof(nc.audiopath), "%s%s", input[0]=='/'?"":"/", input);
						}
						break;
					case EDITCONF_HIDELOG: // hide log from bottom screen
						nc.hidelog = !nc.hidelog;
						break;
					case EDITCONF_BACKOFF: // turn off bottom screen backlight
						nc.backoff = !nc.backoff;
						break;
					case EDITCONF_HIDEKB: // hide on-screen keyboard  from bottom screen
						nc.hidekb = !nc.hidekb;
						break;
					case EDITCONF_CTRVNCKEYS:
						nc.ctr_vnc_keys = !nc.ctr_vnc_keys;
						break;
					case EDITCONF_CTRVNCTOUCH:
						nc.ctr_vnc_touch = !nc.ctr_vnc_touch;
						break;
					case EDITCONF_CTRUDPENABLE:
						nc.ctr_udp_enable = !nc.ctr_udp_enable;
						break;
					case EDITCONF_CTRUDPPORT:
						swkbdInit(&swkbd, SWKBD_TYPE_NUMPAD, 2, 5);
						swkbdSetHintText(&swkbd, "vJoy-UDP-Feeder Port");
						sprintf(input, "%d", nc.ctr_udp_port);
						swkbdSetInitialText(&swkbd, input);
						//swkbdSetFeatures(&swkbd, SWKBD_DEFAULT_QWERTY);
						button = swkbdInputText(&swkbd, input, 6);
						if(button != SWKBD_BUTTON_LEFT) {
							int po = atoi(input);
							if (po <= 0) po=1;
							if (po > 0xffff) po=0xffff;
							nc.ctr_udp_port = po;
						}
						break;
					case EDITCONF_CTRUDPMOTION:
						nc.ctr_udp_motion = !nc.ctr_udp_motion;
						break;
					case EDITCONF_CTRDSUENABLE:
						nc.ctr_dsu_enable = !nc.ctr_dsu_enable;
						break;
					case EDITCONF_CTRDSUPORT:
						swkbdInit(&swkbd, SWKBD_TYPE_NUMPAD, 2, 5);
						swkbdSetHintText(&swkbd, "Cemuhook Server Port");
						sprintf(input, "%d", nc.ctr_dsu_port);
						swkbdSetInitialText(&swkbd, input);
						//swkbdSetFeatures(&swkbd, SWKBD_DEFAULT_QWERTY);
						button = swkbdInputText(&swkbd, input, 6);
						if(button != SWKBD_BUTTON_LEFT) {
							int po = atoi(input);
							if (po <= 0) po=1;
							if (po > 0xffff) po=0xffff;
							nc.ctr_dsu_port = po;
						}
						break;
					}
					break;
				default:
					break;
				}
			}
		}
		checkKeyRepeat();
	}
	if (ret == 1) {
		memcpy(c, &nc, sizeof(nc));
		return 0;
	}
	return -1;
}

static void readconfig() {
	static int isinit = 0;
	if (!isinit) {
		// set default config
		for(int i=0; i<NUMCONF; ++i) conf[i] = default_config;		
		
		FILE *f;
		if((f=fopen(config_filename, "r"))!=NULL) {
			// check correct filesize
			fseek(f, 0L, SEEK_END);
			long int sz = ftell(f);
			fseek(f, 0L, SEEK_SET);
			if (sz == sizeof(vnc_config_0_9) * NUMCONF) {
				// read 0.9 config
				vnc_config_0_9 c[NUMCONF] = {0};
				fread((void*)c, sizeof(vnc_config_0_9), NUMCONF, f);
				for(int i=0; i<NUMCONF; ++i) {
					strcpy(conf[i].name, c[i].name);
					strcpy(conf[i].host, c[i].host);
					conf[i].port = c[i].port;
					strcpy(conf[i].user, c[i].user);
					strcpy(conf[i].pass, c[i].pass);
				}
			} else if (sz == sizeof(vnc_config_1_0) * NUMCONF) {
				// starting for first time after upgrade from 1.0, read 1.0 config
				vnc_config_1_0 c[NUMCONF] = {0};
				fread((void*)c, sizeof(vnc_config_1_0), NUMCONF, f);
				for(int i=0; i<NUMCONF; ++i) {
					strcpy(conf[i].name, c[i].name);
					strcpy(conf[i].host, c[i].host);
					conf[i].port = c[i].port;
					conf[i].audioport = c[i].audioport;
					strcpy(conf[i].audiopath, c[i].audiopath);
					strcpy(conf[i].user, c[i].user);
					strcpy(conf[i].pass, c[i].pass);
					conf[i].scaling = c[i].scaling;
				}
			} else {
				// read current config
				fread((void*)conf, sizeof(vnc_config), NUMCONF, f);
			}
			fclose(f);
		}
		isinit = 1;
	}
}

static int getconfig(vnc_config *c) {
	readconfig();

	SDL_Event e;
	int ret=-1;
	int i;
	static int sel=0;

	int upd = 1;

	// check which sentry to select or jump into edit mode right away
	if (!conf[sel].host[0]) {
		for (i = 0; i<NUMCONF; ++i) {
			if (conf[i].host[0]) break;
		}
		if (i<NUMCONF) sel=i;
		else editconfig(&(conf[sel]));
	}

	// event loop
	while (ret == -1) {
		if (upd) {
			printlist(sel);
			upd=0;
		}
		SDL_Flip(sdl);
		while (SDL_PollEvent(&e)) {
			if (uib_handle_event(&e, 0)) continue;
			if (e.type == SDL_QUIT)
				safeexit();
			map_joy_to_key(&e);
			if (e.type == SDL_KEYDOWN) {
				switch (e.key.keysym.sym) {
				case XK_b:
					ret=1; break;
				case XK_Down:
				case XK_g:
				case XK_k:
					sel = (sel+1) % NUMCONF;
					upd = 1;
					break;
				case XK_Up:
				case XK_t:
				case XK_i:
					sel = (sel+NUMCONF-1) % NUMCONF;
					upd = 1;
					break;
				case XK_x:
					conf[sel] = default_config;
					upd = 1;
					break;
				case XK_w:
					if (conf[sel].host[0])
						cpy = sel;
					else cpy = -1;
					upd = 1;
					break;
				case XK_q:
					if (cpy != -1)
						conf[sel]=conf[cpy];
					cpy = -1;
					upd = 1;
					break;
				case XK_a:
					if (conf[sel].host[0]) {
						*c = conf[sel];
						ret=0;
					} else {
						editconfig(&(conf[sel]));
					}
					upd = 1;
					break;
				case XK_y:
					editconfig(&(conf[sel]));
					upd = 1;
					break;
				default:
					break;
				}
			}
		}
		checkKeyRepeat();
	}
	cpy = -1;
	return (ret ? -1 : 0);
}

static void safeexit() {
	cleanup();
	saveconfig();
	socExit();
	SDL_Quit();
	uib_setBacklight(1);
	exit(0);
}

void aptHookFunc(APT_HookType hookType, void *param)
{

	static int old_state = 1;
	switch (hookType) {
		case APTHOOK_ONSUSPEND:
		case APTHOOK_ONSLEEP:
			old_state = uib_getBacklight();
			if (!old_state) uib_setBacklight (1);
			break;
		case APTHOOK_ONRESTORE:
		case APTHOOK_ONWAKEUP:
			uib_setBacklight (old_state);
			break;
		case APTHOOK_ONEXIT:
			break;
		default:
			break;
	}
}

int main() {
	int i;
	SDL_Event e;
	char buf[512];
	struct vjoy_udp_client udpclient;
	struct dsu_server dsuserver;
	u32 kHeld;
	circlePosition posCp, posStk;
	touchPosition touch;
	accelVector accel;
	angularRate gyro;

	osSetSpeedupEnable(1);

	atexit(safeexit);
	// init romfs file system
	romfsInit();
	atexit((void (*)())romfsExit);
	
	SDL_Init(SDL_INIT_VIDEO | SDL_INIT_JOYSTICK);
	SDL_JoystickOpen(0);

	sdl=SDL_SetVideoMode(400,240,32, SDL_TOPSCR);
	SDL_ShowCursor(SDL_DISABLE);
	bgimg = myIMG_Load("romfs:/background.png");

	SDL_BlitSurface(bgimg, NULL, sdl, NULL);
	SDL_Flip(sdl);

	mkpath(config_filename, 0);

	SOC_buffer = (u32*)memalign(SOC_ALIGN, SOC_BUFFERSIZE);
	socInit(SOC_buffer, SOC_BUFFERSIZE);

	rfbClientLog=log_msg;
	rfbClientErr=log_err;

	uib_enable_keyboard(0);
	uib_init();
	aptHook(&cookie, aptHookFunc, NULL);

	while (1) {
		// get config
		if (getconfig(&config) ||
			config.host[0] == 0) goto quit;
		
		int active=0;

		user1=config.user;
		pass1=config.pass;
		if (config.backoff) uib_setBacklight (0);
		uib_clear();
		uib_update(UIB_RECALC_MENU);

		// VNC connections
		char *argv[] = {
			"TinyVNC",
			buf};
		int argc = sizeof(argv)/sizeof(char*);

		// top screen VNC
		if (!config.vncoff) {
			cl=rfbGetClient(8,3,4); // int bitsPerSample, int samplesPerPixel, int bytesPerPixel
			cl->MallocFrameBuffer = resize;
			cl->canHandleNewFBSize = TRUE;
			cl->GetCredential = get_credential;
			cl->GetPassword = get_password;
			snprintf(buf, sizeof(buf),"%s:%d",config.host, config.port);
			rfbClientLog("Connecting to %s", buf);
			if(!rfbInitClient(cl, &argc, argv))
			{
				cl = NULL; // rfbInitClient has already freed the client struct
			} else ++active;
		}
		// bottom screen VNC
		if (config.enablevnc2) {
			cl2=rfbGetClient(8,3,4); // int bitsPerSample, int samplesPerPixel, int bytesPerPixel
			cl2->MallocFrameBuffer = uibvnc_resize;
			cl2->canHandleNewFBSize = TRUE;
			cl2->GetCredential = get_credential;
			cl2->GetPassword = get_password;
			uibvnc_setScaling(config.scaling2);
			snprintf(buf, sizeof(buf),"%s:%d",config.host, config.port2);
			rfbClientLog("Connecting2 to %s", buf);
			if(!rfbInitClient(cl2, &argc, argv))
			{
				cl2 = NULL; // rfbInitClient has already freed the client struct
			} else ++active;
		}
			
		if (!config.hidekb && (cl || cl2)) uib_enable_keyboard(1);
		if (!cl && cl2) config.eventtarget = 1;
		if (!cl && !cl2) config.ctr_vnc_keys = config.ctr_vnc_touch = 0;

		if (config.enableaudio) {
			snprintf(buf, sizeof(buf),"http://%s:%d%s%s",config.host, config.audioport,
				(config.audiopath[0]=='/'?"":"/"), config.audiopath);
			start_stream(buf, config.user, config.pass);
			++active;
		}

		if (config.ctr_udp_enable) {
			// init UDP client
			if (vjoy_udp_client_init(&udpclient, config.host, config.ctr_udp_port))
				rfbClientErr("vJoy-UDP-feeder client error: %s", udpclient.lasterrmsg);
			else {
				rfbClientLog("vJoy-UDP-feeder client started");
				++active;
			}
		}
		if (config.ctr_dsu_enable) {
			if (dsu_server_init(&dsuserver, config.ctr_dsu_port))
				rfbClientErr("Cemuhook server: %s", dsuserver.lasterrmsg);
			else {
				rfbClientLog("Cemuhook started: %s:%d", dsuserver.ipstr, dsuserver.port);
				++active;
			}
		}
		if ((config.ctr_udp_enable && config.ctr_udp_motion) || config.ctr_dsu_enable) {
			HIDUSER_EnableAccelerometer();
			HIDUSER_EnableGyroscope();
		}

		// clear mouse state
		buttonMask = 0;
		int ext=0;
		int evtarget = 10, taphandling = 1;

		if (!active) rfbClientErr("Noting to do ...");
		else log_color(HEADERCOL, COL_BLACK, "Press SELECT + START to stop");

		while(active) {
			// set up event handling
			if (evtarget != (cl2!=NULL && config.eventtarget!=0)) {
				evtarget = (cl2!=NULL && config.eventtarget!=0);
				cl->appData.useRemoteCursor = evtarget;
				if (cl2) cl2->appData.useRemoteCursor = !evtarget;
				taphandling = evtarget ? !config.notaphandling : 1;
			}
			// handle events
			if (taphandling)
				// must be called once per frame to expire mouse button presses
				uib_handle_tap_processing(NULL);
			SDL_Flip(sdl);
			checkKeyRepeat();
			while (SDL_PollEvent(&e)) {
				if (uib_handle_event(&e, taphandling | (evtarget ? 2 : 0 ))) continue;
				if(!handleSDLEvent(&e)) {
					rfbClientLog("Disconnecting");
					ext=1;
					break;
				}
			}
			if (ext) break;
			push_scheduled_event();
			// vjoy udp feeder && cemuhook server
			if (config.ctr_udp_enable || config.ctr_dsu_enable) {
				kHeld = hidKeysHeld();
				hidCircleRead(&posCp);
				irrstCstickRead(&posStk);
				hidTouchRead(&touch);
				if ((config.ctr_udp_enable && config.ctr_udp_motion) || config.ctr_dsu_enable) {
					hidAccelRead(&accel);
					hidGyroRead(&gyro);
				}
			}
			if (config.ctr_udp_enable) {
				if (config.ctr_udp_motion)
					vjoy_udp_client_update(&udpclient, kHeld, &posCp, &posStk, &touch, &accel, &gyro);
				else
					vjoy_udp_client_update(&udpclient, kHeld, &posCp, &posStk, &touch, NULL, NULL);
			}
			if (config.ctr_dsu_enable &&
				(dsu_server_run(&dsuserver) ||
				dsu_server_update(&dsuserver, kHeld, &posCp, &posStk, &touch, &accel, &gyro)))
			{
				dsu_server_shutdown(&dsuserver);
				config.ctr_dsu_enable = 0;
				--active;
			}

			// audio stream
			if (config.enableaudio && run_stream()) {
				stop_stream();
				config.enableaudio = 0;
				--active;
			}
			// vnc integration
			if (cl) {
				i=WaitForMessage(cl,10);
				if(i<0 || (i>0 && !HandleRFBServerMessage(cl))) {
					rfbClientErr("VNC: error waiting for or processing messages");				
					rfbClientCleanup(cl);
					cl=NULL;
					--active;
				}
			}
			if (cl2) {
				i=WaitForMessage(cl2,10);
				if(i<0) {
					rfbClientErr("BottomVNC: error waiting for messages");				
					rfbClientCleanup(cl2);
					cl2=NULL;
					--active;
				}
				if (i) {
					if (HandleRFBServerMessage(cl2)) {uib_update(UIB_RECALC_VNC);
					} else {
						rfbClientErr("BottomVNC: error processing messages");
						rfbClientCleanup(cl2);
						cl2=NULL;
						--active;
					}
				}
			}
		}
		// cleanup udp client / dsu server
		if (config.ctr_dsu_enable)
			dsu_server_shutdown(&dsuserver);
		if (config.ctr_udp_enable)
			vjoy_udp_client_shutdown(&udpclient);
		HIDUSER_DisableAccelerometer();
		HIDUSER_DisableGyroscope();

		// clean up audio stream
		if (config.enableaudio)
			stop_stream();
		// clean up VNC clients
		cleanup();

		uib_enable_keyboard(0);
		uib_setBacklight(1);
		if (!ext) { // means, we exited due to an error
			uib_set_colors(COL_BLACK, HEADERCOL);
			uib_printf("A:retry B:quit                          ");
			uib_reset_colors();
			uib_update(UIB_RECALC_MENU);
			while (1) {
				SDL_Flip(sdl);
				checkKeyRepeat();
				if (SDL_PollEvent(&e)) {
					if (uib_handle_event(&e, 0)) continue;
					map_joy_to_key(&e);
					if (e.type == SDL_KEYDOWN) {
						if (e.key.keysym.sym == XK_a)
							break;
						if (e.key.keysym.sym == XK_b)
							goto quit;
					}
				}
			}
		}
	}
quit:
	return 1;
}
