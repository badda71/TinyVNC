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
#include "streamclient.h"
#include "uibottom.h"

#define SOC_ALIGN       0x1000
#define SOC_BUFFERSIZE  0x100000
#define NUMCONF 25

typedef struct {
	char name[128];
	char host[128];
	int port;
	int audioport;
	char audiopath[128];
	char user[128];
	char pass[128];
	int scaling;
} vnc_config;

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
const char *keymap_filename = "/3ds/TinyVNC/keymap";
#define BUFSIZE 1024
static vnc_config conf[NUMCONF] = {0};
static int cpy = -1;
static u32 *SOC_buffer = NULL;
static int viewOnly=0, buttonMask=0;
/* client's pointer position */
static int x=0,y=0;
rfbClient* cl;
static SDL_Surface *bgimg;
SDL_Surface* sdl=NULL;
static int sdl_pos_x, sdl_pos_y;
static vnc_config config;
static int have_scrollbars=0;

extern void SDL_SetVideoPosition(int x, int y);
extern void SDL_ResetVideoPosition();

// default
struct {
	char *name;
	int rfb_key;
} buttons3ds[] = {
	{"A", XK_a},
	{"B", XK_b},
	{"X", XK_x},
	{"Y", XK_y},
	{"L", XK_q},
	{"R", XK_w},
	{"ZL", XK_1},
	{"ZR", XK_2},
	{"START", 2}, // disconnect
//	{"SELECT", 8}, // toggle scaling
	{"SELECT", XK_Escape}

	{"CPAD_UP", XK_Up}, // C-PAD
	{"CPAD_DOWN", XK_Down},
	{"CPAD_LEFT", XK_Left},
	{"CPAD_RIGHT", XK_Right},

	{"DPAD_UP", XK_t},	// D-PAD
	{"DPAD_DOWN", XK_g},
	{"DPAD_LEFT", XK_f},
	{"DPAD_RIGHT", XK_h},

	{"CSTCK_UP", XK_i}, // C-STICK
	{"CSTCK_DOWN", XK_k},
	{"CSTCK_LEFT", XK_j},
	{"CSTCK_RIGHT", XK_l},

	{NULL,0}
};

static void vwrite_log(const char *format, va_list arg, int channel)
{
	int i=vsnprintf(NULL, 0, format, arg);
	if (i) {
		char *buf = malloc(i+1);
		vsnprintf(buf, i+1, format, arg);
		while (i && buf[i-1]=='\n') buf[--i]=0; // strip trailing newlines
		if (channel & 2) svcOutputDebugString(buf, i);
		if (channel & 1) {
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
		rfbClientErr("resize: screen size >1024px not supported");
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
			flags |= ((width * 1000) / 400 > (height * 1000) / 240)? SDL_FITWIDTH : SDL_FITHEIGHT;
		}
	} else {
		have_scrollbars = (width>400?1:0) + (height>240?2:0);
		int w = have_scrollbars & 2 ? 398 : 400;
		int h = have_scrollbars & 1 ? 238 : 240;
		// center screen around mouse cursor if not scaling
		SDL_SetVideoPosition(sdl_pos_x = LIMIT(-x+w/2, -width+w,0), sdl_pos_y = LIMIT( -y+h/2, -height+h, 0));
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

	rfbClientSetClientData(client, SDL_Init, sdl);
	client->width = sdl->pitch / (depth / 8);
	client->frameBuffer=sdl->pixels;

	client->format.bitsPerPixel=depth;
	client->format.redShift=sdl->format->Rshift;
	client->format.greenShift=sdl->format->Gshift;
	client->format.blueShift=sdl->format->Bshift;

	client->format.redShift=sdl->format->Rshift;
	client->format.greenShift=sdl->format->Gshift;
	client->format.blueShift=sdl->format->Bshift;

	client->format.redMax=sdl->format->Rmask>>client->format.redShift;
	client->format.greenMax=sdl->format->Gmask>>client->format.greenShift;
	client->format.blueMax=sdl->format->Bmask>>client->format.blueShift;
	SetFormatAndEncodings(client);

	return TRUE;
}

/*
static void update(rfbClient* cl,int x,int y,int w,int h) {
}

static void kbd_leds(rfbClient* cl, int value, int pad) {
	// note: pad is for future expansion 0=unused
	rfbClientErr("Led State= 0x%02X", value);
}

// trivial support for textchat
static void text_chat(rfbClient* cl, int value, char *text) {
	switch(value) {
	case rfbTextChatOpen:
		rfbClientErr("TextChat: We should open a textchat window!");
		TextChatOpen(cl);
		break;
	case rfbTextChatClose:
		rfbClientErr("TextChat: We should close our window!");
		break;
	case rfbTextChatFinished:
		rfbClientErr("TextChat: We should close our window!");
		break;
	default:
		rfbClientErr("TextChat: Received \"%s\"", text);
		break;
	}
	fflush(stderr);
}
*/

static void cleanup()
{
  if(cl)
    rfbClientCleanup(cl);
  cl = NULL;
}

// function for translating the joystick events to keyboard events (analog to old SDL version)
#define THRESHOLD 16384
static void map_joy_to_key(SDL_Event *e)
{
	static int old_status = 0;
	static int old_dp_status = 0;
	int i1,i2, status;

	static int buttonkeys[10]={8, 0, 1, 2, 3, 4, 5, 9, 6, 7};
	static int axiskeys[8]={13, 12, 11, 10, 21, 20, 19, 18};
	static int hatkeys[4]={14, 17, 15, 16};

	switch (e->type) {
	case SDL_JOYHATMOTION:
		status = e->jhat.value;
		if (status != old_dp_status) {
			for (i1 = 0; i1 < 4; ++i1) {
				i2 = 1 << i1;
				if ((status & i2) != (old_dp_status & i2))
					push_key_event(buttons3ds[hatkeys[i1]].rfb_key, status & i2);
			}
			old_dp_status = status;
		}
		break;
	case SDL_JOYBUTTONDOWN:
	case SDL_JOYBUTTONUP:
		i1 = e->type == SDL_JOYBUTTONDOWN ? 1 : 0;
		make_key_event(e, buttons3ds[buttonkeys[e->jbutton.button]].rfb_key, i1); break;
		break;
	case SDL_JOYAXISMOTION:
		i1 = 3 << (e->jaxis.axis * 2);
		i2 = (e->jaxis.value > THRESHOLD ? 1 : (e->jaxis.value < -THRESHOLD ? 2 : 0)) << (e->jaxis.axis * 2);
		status = (old_status & (~i1)) | i2;
		if (old_status != status) {
			for (i1 = 0; i1 < 8; ++i1) {
				i2 = 1 << i1;
				if ((status & i2) != (old_status & i2))
					push_key_event(buttons3ds[axiskeys[i1]].rfb_key, status & i2);
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

// event handler while VNC is running
static rfbBool handleSDLEvent(rfbClient *cl, SDL_Event *e)
{
	int s;
	map_joy_to_key(e);
	switch(e->type) {
	case SDL_MOUSEBUTTONUP:
	case SDL_MOUSEBUTTONDOWN:
	case SDL_MOUSEMOTION:
	{
		if (viewOnly)
			break;

		if (e->type == SDL_MOUSEMOTION) {
			x += e->motion.xrel;
			if (x < 0) x=0;
			if (x > cl->width) x=cl->width;
			y += e->motion.yrel;
			if (y < 0) y=0;
			if (y > cl->height) y = cl->height;

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
		else {
			record_mousebutton_event(e->button.button, e->type == SDL_MOUSEBUTTONDOWN?1:0);
		}
		SendPointerEvent(cl, x, y, buttonMask);
		buttonMask &= ~(rfbButton4Mask | rfbButton5Mask); // clear wheel up and wheel down state
		break;
	}
	case SDL_KEYUP:
	case SDL_KEYDOWN:
		s =  e->key.keysym.sym;
		if (s == 2)
			return 0; // disconnect
		if (viewOnly) break;
		if (s>2 && s<8) {
			// mouse button 1-5: 3-7
			record_mousebutton_event(s-2, e->type == SDL_KEYDOWN?1:0);
			SendPointerEvent(cl, x, y, buttonMask);
			buttonMask &= ~(rfbButton4Mask | rfbButton5Mask); // clear wheel up and wheel down state
		} else if (s == 8 && e->type == SDL_KEYDOWN) {
			// toggle scaling
			config.scaling = !config.scaling;
			// resize the SDL screen
			resize(cl);
			SendFramebufferUpdateRequest(cl, 0, 0, cl->width, cl->height, false);
		} else {
			SendKeyEvent(cl, s, e->type == SDL_KEYDOWN ? TRUE : FALSE);
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

#define HEADERCOL COL_MAKE(0x47, 0x80, 0x82)

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
	uib_printf(	"A:select B:quit X:delete Y:copy R:edit  ");
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
	EDITCONF_AUDIOPORT,
	EDITCONF_AUDIOPATH,
	EDITCONF_USER,
	EDITCONF_PASS,
	EDITCONF_SCALING,
	EDITCONF_END
};

static int editconfig(vnc_config *c) {
	int sel = EDITCONF_HOST;
	const char *sep = "----------------------------------------";
	const char *pass = "****************************************";
	char input[128];
	int ret = 0;
	static SwkbdState swkbd;
	SDL_Event e;
	SwkbdButton button;
	char *msg=NULL;
	int showmsg = 0, msg_state=0;

	vnc_config nc = *c;
	if (nc.port <= 0)
		nc.port = SERVER_PORT_OFFSET;
	if (!nc.audiopath[0])
		strcpy(nc.audiopath,"/");
	if (!nc.host[0])
		nc.scaling = 1;

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
			uib_printf(	" Edit VNC Server                        ");
			uib_set_position(0,27);
			if (msg && showmsg) {
				uib_printf("%-.40s",msg);
			}
			uib_set_position(0,29);
			uib_printf(	"A:edit B:cancel Y:OK                    ");
			uib_reset_colors();

			uib_set_position(0,2);
			uib_printf(	"Name: ");
			if (sel == EDITCONF_NAME) uib_invert_colors();
			uib_printf(	"%-34.34s", nc.name);
			if (sel == EDITCONF_NAME) uib_reset_colors();

			uib_set_position(0,4);
			uib_printf( "%s", sep);
			uib_set_position(0,6);
			uib_printf(	"Host: ");
			if (sel == EDITCONF_HOST) uib_invert_colors();
			uib_printf(	"%-34.34s", nc.host);
			if (sel == EDITCONF_HOST) uib_reset_colors();
			uib_set_position(0,8);
			uib_printf(	"Port: ");
			if (sel == EDITCONF_PORT)uib_invert_colors();
			uib_printf(	"%-34d", nc.port);
			if (sel == EDITCONF_PORT) uib_reset_colors();
			uib_set_position(0,10);
			uib_printf(	"Audio Stream Port: ");
			if (sel == EDITCONF_AUDIOPORT) uib_invert_colors();
			uib_printf(	"%-21s", nc.audioport?itoa(nc.audioport,input,10):"");
			if (sel == EDITCONF_AUDIOPORT) uib_reset_colors();
			uib_set_position(0,12);
			uib_printf(	"Audio Stream Path: ");
			if (sel == EDITCONF_AUDIOPATH) uib_invert_colors();
			uib_printf(	"%-21s", nc.audiopath);
			if (sel == EDITCONF_AUDIOPATH) uib_reset_colors();

			uib_set_position(0,14);
			uib_printf(	"%s", sep);
			uib_set_position(0,16);
			uib_printf(	"Username: ");
			if (sel == EDITCONF_USER) uib_invert_colors();
			uib_printf(	"%-30.30s", nc.user);
			if (sel == EDITCONF_USER) uib_reset_colors();
			uib_set_position(0,18);
			uib_printf(	"Password: ");
			if (sel == EDITCONF_PASS) uib_invert_colors();
			uib_printf(	"%*.*s%*s", strlen(nc.pass), strlen(nc.pass), pass, 30-strlen(nc.pass), "");
			if (sel == EDITCONF_PASS) uib_reset_colors();

			uib_set_position(0,20);
			uib_printf(	"%s", sep);
			uib_set_position(0,22);
			uib_printf(nc.scaling?"\x91 ":"\x90 ");
			if (sel == EDITCONF_SCALING) uib_invert_colors();
			uib_printf(	"Scale to fit screen");
			if (sel == EDITCONF_SCALING) uib_reset_colors();

			uib_update(UIB_RECALC_MENU);
		}
		SDL_Flip(sdl);
		while (SDL_PollEvent(&e)) {
			if (uib_handle_event(&e)) continue;
			if (e.type == SDL_QUIT)
				safeexit();
			map_joy_to_key(&e);
			if (e.type == SDL_KEYDOWN) {
				switch (e.key.keysym.sym) {
				case XK_b:
					ret=-1; break;
				case XK_y:
					// validate input
					if (!nc.host[0]) {
						msg = "Host name is required";
					} else ret=1;
					break;
				case XK_Down:
				case XK_g:
				case XK_k:
					sel = (sel+1) % EDITCONF_END;
					upd = 1;
					break;
				case XK_Up:
				case XK_t:
				case XK_i:
					sel = (sel + EDITCONF_END - 1) % EDITCONF_END;
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
							if (po < 0) po=0;
							if (po > 0xffff) po=0xffff;
							checkset(nc.name, nc.host, po, nc.user, nc.host, nc.port, nc.user);
							nc.port = po;
						}
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
							if (po < 0) po=0;
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
					case EDITCONF_SCALING:
						nc.scaling = !nc.scaling;
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
		FILE *f;
		if((f=fopen(config_filename, "r"))!=NULL) {
			// check correct filesize
			fseek(f, 0L, SEEK_END);
			long int sz = ftell(f);
			fseek(f, 0L, SEEK_SET);
			if (sz == sizeof(vnc_config_0_9) * NUMCONF) {
				// starting for first time after upgrade from 0.9, delete the keymap file
				unlink(keymap_filename);
				// read 0.9 config
				vnc_config_0_9 c[NUMCONF] = {0};
				fread((void*)c, sizeof(vnc_config_0_9), NUMCONF, f);
				for(int i=0; i<NUMCONF; ++i) {
					strcpy(conf[i].name, c[i].name);
					strcpy(conf[i].host, c[i].host);
					conf[i].port = c[i].port;
					strcpy(conf[i].user, c[i].user);
					strcpy(conf[i].pass, c[i].pass);
					conf[i].scaling = 1;
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
			if (uib_handle_event(&e)) continue;
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
					memset(&(conf[sel]), 0, sizeof(vnc_config));
					upd = 1;
					break;
				case XK_y:
					if (conf[sel].host[0])
						cpy = sel;
					else cpy = -1;
					upd = 1;
					break;
				case XK_q:
					if (cpy != -1)
						memcpy(&(conf[sel]), &(conf[cpy]), sizeof(vnc_config));
					cpy = -1;
					upd = 1;
					break;
				case XK_a:
					if (conf[sel].host[0]) {
						memcpy(c, &(conf[sel]), sizeof(vnc_config));
						ret=0;
					} else {
						editconfig(&(conf[sel]));
					}
					upd = 1;
					break;
				case XK_w:
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
	exit(0);
}

static void readkeymaps(char *cname) {
	FILE *f=NULL;
	int i, x;
	char name[32], *p=NULL, *fn=NULL, buf[BUFSIZE];
	if (cname && cname[0]) {
		p=malloc(strlen(keymap_filename)+2+strlen(cname));
		strcpy(p, keymap_filename);
		sprintf(strrchr(p,'/'),"/%s.keymap", cname);
	}
	if( (p && (f=fopen(p, "r"))!=NULL && (fn=p)) ||
		((f=fopen(keymap_filename, "r"))!=NULL && (fn=(char*)keymap_filename))) {
		rfbClientLog("Reading keymap from %s", fn);
		while (fgets(buf, BUFSIZE, f)) {
			if (buf[0]=='#' || sscanf(buf,"%s %x\n",name, &x) != 2) continue;
			for (i=0; buttons3ds[i].rfb_key!=0; ++i) {
				if (strcmp(buttons3ds[i].name, name)==0) {
					buttons3ds[i].rfb_key=x;
					break;
				}
			}
		}
		fclose(f);
	} else if ((f=fopen(keymap_filename, "w"))!=NULL) {
		rfbClientLog("Saving standard keymap to %s", keymap_filename);
		
		fprintf(f,
			"# mappings as per https://libvnc.github.io/doc/html/keysym_8h_source.html\n"
			"# 1 = toggle keyboard\n"
			"# 2 = disconnect\n"
			"# 3-7 = mouse button 1-5 (1=left, 2=middle, 3=right, 4=wheelup, 5=wheeldown\n"
			"# 8 = toggle scaling\n"
		);
		for (i=0; buttons3ds[i].rfb_key != 0; ++i) {
			fprintf(f,"%s\t%s0x%04X\n",
				buttons3ds[i].name,
				buttons3ds[i].rfb_key < 0 ? "-" : "",
				abs(buttons3ds[i].rfb_key));
		}
		fclose(f);
	}
	if (p) free(p);
}

int main() {
	int i;
	SDL_Event e;
	char buf[512];
	osSetSpeedupEnable(1);

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

	mkpath(config_filename, false);

	SOC_buffer = (u32*)memalign(SOC_ALIGN, SOC_BUFFERSIZE);
	socInit(SOC_buffer, SOC_BUFFERSIZE);

	rfbClientLog=log_msg;
	rfbClientErr=log_err;

	uib_enable_keyboard(0);
	uib_init();

	while (1) {
		// get config
		if (getconfig(&config) ||
			config.host[0] == 0) goto quit;
		
		user1=config.user;
		pass1=config.pass;
		uib_clear();
		snprintf(buf, sizeof(buf),"%s:%d",config.host, config.port);

		cl=rfbGetClient(8,3,4);
		cl->MallocFrameBuffer = resize;
		cl->canHandleNewFBSize = TRUE;
		cl->GetCredential = get_credential;
		cl->GetPassword = get_password;

		char *argv[] = {
			"TinyVNC",
			buf};
		int argc = sizeof(argv)/sizeof(char*);

		readkeymaps(config.name);
		rfbClientLog("Connecting to %s", buf);
		
		if(!rfbInitClient(cl, &argc, argv))
		{
			cl = NULL; /* rfbInitClient has already freed the client struct */
		} else {
			if (config.audioport) {
				snprintf(buf, sizeof(buf),"http://%s:%d%s%s",config.host, config.audioport,
					(config.audiopath[0]=='/'?"":"/"), config.audiopath);
				start_stream(buf, config.user, config.pass);
			}
			uib_enable_keyboard(1);
		}

		// clear mouse state
		buttonMask = 0;
		int ext=0;
		while(cl) {
			// handle events
			// must be called once per frame to expire mouse button presses
			uib_handle_tap_processing(NULL);
			SDL_Flip(sdl);
			checkKeyRepeat();
			while (SDL_PollEvent(&e)) {
				if (uib_handle_event(&e)) continue;
				if(!handleSDLEvent(cl, &e)) {
					rfbClientLog("Disconnecting");
					ext=1;
					break;
				}
			}
			if (ext) break;
			// audio stream
			if (config.audioport)
				run_stream();
			// vnc integration
			i=WaitForMessage(cl,500);
			if(i<0) break; // error waiting
			if(i && !HandleRFBServerMessage(cl))
				break; // error handling
		}
		if (config.audioport)
			stop_stream();
		cleanup();
		uib_enable_keyboard(0);
		if (!ext) { // means, we exited due to an error
			uib_set_colors(COL_BLACK, HEADERCOL);
			uib_printf("A:retry B:quit                          ");
			uib_reset_colors();
			uib_update(UIB_RECALC_MENU);
			while (1) {
				SDL_Flip(sdl);
				checkKeyRepeat();
				if (SDL_PollEvent(&e)) {
					if (uib_handle_event(&e)) continue;
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
	safeexit();
}
