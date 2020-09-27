#include <malloc.h>
#include <stdio.h>
#include <sys/stat.h>
#include <errno.h>
#include <3ds.h>
#include <SDL/SDL.h>
#include <SDL/SDL_image.h>
#include <rfb/rfbclient.h>
#include "streamclient.h"

#define SOC_ALIGN       0x1000
#define SOC_BUFFERSIZE  0x100000
#define NUMCONF 25

typedef struct {
	char name[128];
	char host[128];
	int port;
	int audioport;
	char user[128];
	char pass[128];
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
static vnc_config conf[NUMCONF] = {0};
static int cpy = -1;
static u32 *SOC_buffer = NULL;
static int viewOnly=0, buttonMask=0;
/* client's pointer position */
static int x,y;
static rfbClient* cl;
static SDL_Surface *bgimg;

// default
struct {
	char *name;
	unsigned int key;
	SDLKey sdl_key;
	int rfb_key;
} buttons3ds[] = {
	{"A", KEY_A, SDLK_a,	XK_a},
	{"B", KEY_B, SDLK_b, XK_b},
	{"X", KEY_X, SDLK_x, XK_x},
	{"Y", KEY_Y, SDLK_y, XK_y},
	{"L", KEY_L, SDLK_q, XK_q},
	{"R", KEY_R, SDLK_w, XK_w},
	{"ZL", KEY_ZL, SDLK_1, XK_1},
	{"ZR", KEY_ZR, SDLK_2, XK_2},
	{"START", KEY_START, SDLK_RETURN, -0x100}, // disconnect
	{"SELECT", KEY_SELECT, SDLK_ESCAPE, XK_Escape},

	{"CPAD_UP", KEY_CPAD_UP, SDLK_UP, XK_Up}, // C-PAD
	{"CPAD_DOWN", KEY_CPAD_DOWN, SDLK_DOWN, XK_Down},
	{"CPAD_LEFT", KEY_CPAD_LEFT, SDLK_LEFT, XK_Left},
	{"CPAD_RIGHT", KEY_CPAD_RIGHT, SDLK_RIGHT, XK_Right},

	{"DPAD_UP", KEY_DUP, SDLK_t, XK_t},	// D-PAD
	{"DPAD_DOWN", KEY_DDOWN, SDLK_g, XK_g},
	{"DPAD_LEFT", KEY_DLEFT, SDLK_f, XK_f},
	{"DPAD_RIGHT", KEY_DRIGHT, SDLK_h, XK_h},

	{"CSTCK_UP", KEY_CSTICK_UP, SDLK_i, XK_i}, // C-STICK
	{"CSTCK_DOWN", KEY_CSTICK_DOWN, SDLK_k, XK_k},
	{"CSTCK_LEFT", KEY_CSTICK_LEFT, SDLK_j, XK_j},
	{"CSTCK_RIGHT", KEY_CSTICK_RIGHT, SDLK_l, XK_l},

	{NULL, 0,0,0}
};

static void vlog_citra(const char *format, va_list arg ) {
	char buf[2000];
    vsnprintf(buf, 2000, format, arg);
	int i=strlen(buf);
	svcOutputDebugString(buf, i);
	printf("%s",buf);
	if (i && buf[i-1] != '\n')
		printf("\n");
}

void log_citra(const char *format, ...)
{
    va_list argptr;
    va_start(argptr, format);
	vlog_citra(format, argptr);
    va_end(argptr);
}

static rfbBool resize(rfbClient* client) {
	int width=client->width;
	int height=client->height;
	int depth=client->format.bitsPerPixel;
//log_citra("%s: %d %d %d",__func__,width,height,depth);

	if (width > 1024 || height > 1024) {
		rfbClientErr("resize: screen size >1024px not supported");
		return FALSE;
	}

	client->updateRect.x = client->updateRect.y = 0;
	client->updateRect.w = width;
	client->updateRect.h = height;

	/* (re)create the surface used as the client's framebuffer */
	int flags = SDL_TOPSCR;
	if (width > 400 || height > 240) {
		flags |= ((width * 1000) / 400 > (height * 1000) / 240)? SDL_FITWIDTH : SDL_FITHEIGHT;
	}
	SDL_Surface* sdl = SDL_SetVideoMode(width, height, depth, flags);
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

static int SDL_key2rfbKeySym(SDL_KeyboardEvent* e) {
	SDLKey sym = e->keysym.sym;

	// init 3DS buttons to their values
	for (int i=0; buttons3ds[i].key!=0; ++i)
		if (sym == buttons3ds[i].sdl_key) return buttons3ds[i].rfb_key;
	return 0;
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

// *****************************
// lots of stuff below inspired from synaptics touchpad driver
// https://github.com/freedesktop/xorg-xf86-input-synaptics
// ******************************

int mainMenu_max_tap_time=250;
int mainMenu_click_time=100;
int mainMenu_single_tap_timeout=250;
int mainMenu_max_double_tap_time=250;
int mainMenu_locked_drag_timeout=5000;
int mainMenu_tap_and_drag_gesture=1;
int mainMenu_locked_drags=0;

typedef enum {
    TS_START,                   /* No tap/drag in progress */
    TS_1,                       /* After first touch */
    TS_MOVE,                    /* Pointer movement enabled */
    TS_2A,                      /* After first release */
    TS_2B,                      /* After second/third/... release */
    TS_SINGLETAP,               /* After timeout after first release */
    TS_3,                       /* After second touch */
    TS_DRAG,                    /* Pointer drag enabled */
    TS_4,                       /* After release when "locked drags" enabled */
    TS_5,                       /* After touch when "locked drags" enabled */
    TS_CLICKPAD_MOVE,           /* After left button press on a clickpad */
	TS_MOUSEDOWN,
	TS_MOUSEUP,
} TapState;

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
    default:
		break;
	}
	return 0;
}

static int mouse_state = 0;

static void setMouseButton(int state, int button) {
//log_citra("enter %s",__func__);
	SDL_Event ev = {0};
	if (!state) {
		ev.type = SDL_MOUSEBUTTONUP;
		ev.button.state = SDL_RELEASED;
	} else {
		ev.type = SDL_MOUSEBUTTONDOWN;
		ev.button.state = SDL_PRESSED;
	}
	ev.button.which = 1;
	ev.button.button = button;
	ev.button.x = x;
	ev.button.y = y;
	SDL_PushEvent(&ev);
}

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
		setMouseButton(x, SDL_BUTTON_LEFT);
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
    case TS_START: /* No tap/drag in progress */
        if (e && e->type==SDL_MOUSEBUTTONDOWN)
            SETSTATE(TS_1)
        break;
    case TS_1: /* After first touch */
        if (is_timeout) {
            SETSTATE(TS_MOVE)
            goto restart;
        }
        else if (e && e->type==SDL_MOUSEBUTTONUP) {
            SETSTATE(TS_2A)
        }
        break;
    case TS_MOVE: /* Pointer movement enabled */
        if (e && e->type==SDL_MOUSEBUTTONUP) {
            SETSTATE(TS_START)
        }
        break;
    case TS_2A: /* After first release */
        if (e && e->type==SDL_MOUSEBUTTONDOWN)
            SETSTATE(TS_3)
        else if (is_timeout)
            SETSTATE(TS_SINGLETAP)
        break;
    case TS_2B: /* After second/third/... release */
        if (e && e->type==SDL_MOUSEBUTTONDOWN) {
            SETSTATE(TS_3)
        }
        else if (is_timeout) {
            SETSTATE(TS_START)
            status2=2;
        }
        break;
    case TS_SINGLETAP: /* After timeout after first release */
        if (e && e->type==SDL_MOUSEBUTTONDOWN)
            SETSTATE(TS_1)
        else if (is_timeout)
            SETSTATE(TS_START)
        break;
    case TS_3: /* After second touch */
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

static void safeexit();

static rfbBool handleSDLEvent(rfbClient *cl, SDL_Event *e)
{
	switch(e->type) {
	case SDL_MOUSEBUTTONUP:
	case SDL_MOUSEBUTTONDOWN:
	case SDL_MOUSEMOTION:
	{
		int state, i;
		if (viewOnly)
			break;

		if (e->type == SDL_MOUSEMOTION) {
			if (e->motion.state) {
				x += e->motion.xrel;
				if (x < 0) x=0;
				if (x > cl->width) x=cl->width;
				y += e->motion.yrel;
				if (y < 0) y=0;
				if (y > cl->height) y = cl->height;
			}
		}
		else {
			if (e->button.which == 0) {
				uib_handle_tap_processing(e);
				return TRUE;
			} 
			state = e->button.button;
			for (i = 0; buttonMapping[i].sdl; i++) {
				if (state == buttonMapping[i].sdl) {
					state = buttonMapping[i].rfb;
					if (e->type == SDL_MOUSEBUTTONDOWN)
						buttonMask |= state;
					else
						buttonMask &= ~state;
					break;
				}
			}
		}
		SendPointerEvent(cl, x, y, buttonMask);
		buttonMask &= ~(rfbButton4Mask | rfbButton5Mask); // clear wheel up and wheel down state
		break;
	}
	case SDL_KEYUP:
	case SDL_KEYDOWN:
		if (viewOnly)
			break;
		int s =  SDL_key2rfbKeySym(&e->key);
		if (s == -256) {
			return 0; // disconnect
		} else if (s<0) {
			// mouse button 1-5: -1 - -5
			setMouseButton(e->type == SDL_KEYDOWN ? TRUE : FALSE, -s);
		} else {
			SendKeyEvent(cl, s, e->type == SDL_KEYDOWN ? TRUE : FALSE);
		}
		break;
	case SDL_QUIT:
		safeexit();
	default:
		rfbClientLog("ignore SDL event: 0x%x", e->type);
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
	printf(	"\x1b[2J\x1b[H\x1b[46;30m"
			" %-38.38s ", "TinyVNC v" VERSION " by badda71");
	if (cpy!=-1) {
		snprintf(buf,41,"L:paste '%s'                              ",conf[cpy].name);
		printf("\x1b[29;0H%s", buf);
	}
	printf(	"\x1b[30;0H"
			"A:select B:quit X:delete Y:copy R:edit  "
			"\x1b[0m");
	for (int i=0;i<NUMCONF; ++i) {
		printf("\x1b[%d;0H", i+3);
		if (i==num) printf("\x1b[7m");
		printf("%-40.40s", conf[i].host[0]?conf[i].name:"-");
		if (i==num) printf("\x1b[0m");
	}
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
	EDITCONF_USER,
	EDITCONF_PASS,
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

	vnc_config nc = *c;
	if (nc.port <= 0) nc.port = SERVER_PORT_OFFSET;
	int upd = 1;

	while (ret==0) {
		gfxFlushBuffers();
		gspWaitForVBlank();
		if (upd) {
			printf(	"\x1b[2J\x1b[H\x1b[46;30m"
					" Edit VNC Server                        ");
			if (msg) printf(
					"\x1b[28;0H%-.40s",msg);
			printf(	"\x1b[30;0H"
					"A:edit B:cancel Y:OK                    "
					"\x1b[0m");
			printf(	"\x1b[3;0HName: ");
			if (sel == EDITCONF_NAME) printf("\x1b[7m");
			printf(	"%-34.34s", nc.name);
			if (sel == EDITCONF_NAME) printf("\x1b[0m");
			printf(	"\x1b[5;0H%s", sep);
			printf(	"\x1b[7;0HHost: ");
			if (sel == EDITCONF_HOST) printf("\x1b[7m");
			printf(	"%-34.34s", nc.host);
			if (sel == EDITCONF_HOST) printf("\x1b[0m");
			printf(	"\x1b[9;0HPort: ");
			if (sel == EDITCONF_PORT) printf("\x1b[7m");
			printf(	"%-34d", nc.port);
			if (sel == EDITCONF_PORT) printf("\x1b[0m");
			printf(	"\x1b[11;0HAudio Stream Port: ");
			if (sel == EDITCONF_AUDIOPORT) printf("\x1b[7m");
			printf(	"%-21s", nc.audioport?itoa(nc.audioport,input,10):"");
			if (sel == EDITCONF_AUDIOPORT) printf("\x1b[0m");
			printf(	"\x1b[13;0H%s", sep);
			printf(	"\x1b[15;0HUsername: ");
			if (sel == EDITCONF_USER) printf("\x1b[7m");
			printf(	"%-30.30s", nc.user);
			if (sel == EDITCONF_USER) printf("\x1b[0m");
			printf(	"\x1b[17;0HPassword: ");
			if (sel == EDITCONF_PASS) printf("\x1b[7m");
			printf(	"%*.*s%*s", strlen(nc.pass), strlen(nc.pass), pass, 30-strlen(nc.pass), "");
			if (sel == EDITCONF_PASS) printf("\x1b[0m");
			SDL_Flip(SDL_GetVideoSurface());
		}
		while (SDL_PollEvent(&e)) {
			if (e.type == SDL_QUIT)
				safeexit();
			if (e.type == SDL_KEYDOWN) {
				switch (e.key.keysym.sym) {
				case SDLK_b:
					ret=-1; break;
				case SDLK_y:
					// validate input
					if (!nc.host[0]) {
						msg = "Host name is required";
					} else ret=1;
					break;
				case SDLK_DOWN:
				case SDLK_g:
				case SDLK_k:
					sel = (sel+1) % EDITCONF_END;
					upd = 1;
					break;
				case SDLK_UP:
				case SDLK_t:
				case SDLK_i:
					sel = (sel + EDITCONF_END - 1) % EDITCONF_END;
					upd = 1;
					break;
				case SDLK_a:
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
					case EDITCONF_AUDIOPORT: // port
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
					}
					break;
				default:
					break;
				}
			}
		}
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
	int i, sel=0;

	SDL_EnableKeyRepeat(SDL_DEFAULT_REPEAT_DELAY, SDL_DEFAULT_REPEAT_INTERVAL);
	int upd = 1;

	// check which sentry to select or jump into edit mode right away
	for (i = 0; i<NUMCONF; ++i) {
		if (conf[i].host[0]) break;
	}
	if (i<NUMCONF) sel=i;
	else editconfig(&(conf[sel]));

	// event loop
	while (ret == -1) {
		gfxFlushBuffers();
		gspWaitForVBlank();
		//gfxSwapBuffers();
		if (upd) {
			printlist(sel);
			upd=0;
		}
		while (SDL_PollEvent(&e)) {
			if (e.type == SDL_QUIT)
				safeexit();
			if (e.type == SDL_KEYDOWN) {
				switch (e.key.keysym.sym) {
				case SDLK_b:
					ret=1; break;
				case SDLK_DOWN:
				case SDLK_g:
				case SDLK_k:
					sel = (sel+1) % NUMCONF;
					upd = 1;
					break;
				case SDLK_UP:
				case SDLK_t:
				case SDLK_i:
					sel = (sel+NUMCONF-1) % NUMCONF;
					upd = 1;
					break;
				case SDLK_x:
					memset(&(conf[sel]), 0, sizeof(vnc_config));
					upd = 1;
					break;
				case SDLK_y:
					if (conf[sel].host[0])
						cpy = sel;
					else cpy = -1;
					upd = 1;
					break;
				case SDLK_q:
					if (cpy != -1)
						memcpy(&(conf[sel]), &(conf[cpy]), sizeof(vnc_config));
					cpy = -1;
					upd = 1;
					break;
				case SDLK_a:
					if (conf[sel].host[0]) {
						memcpy(c, &(conf[sel]), sizeof(vnc_config));
						ret=0;
					} else {
						editconfig(&(conf[sel]));
					}
					upd = 1;
					break;
				case SDLK_w:
					editconfig(&(conf[sel]));
					upd = 1;
					break;
				default:
					break;
				}
			}
		}
	}
	SDL_EnableKeyRepeat(0,0);
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

const char *keymap_filename = "/3ds/TinyVNC/keymap";
#define BUFSIZE 1024

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
			for (i=0; buttons3ds[i].key!=0; ++i) {
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
			
			"# -0x100 = disconnect\n"
			"# -0x1 ... -0x5 = mouse button 1-5\n");
		for (i=0; buttons3ds[i].key != 0; ++i) {
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
	char buf[256];
	osSetSpeedupEnable(1);

	// init romfs file system
	romfsInit();
	atexit((void (*)())romfsExit);
	
	SDL_Init(SDL_INIT_VIDEO);
	SDL_Surface *sdl=SDL_SetVideoMode(400,240,32, SDL_TOPSCR);
	SDL_ShowCursor(SDL_DISABLE);
	bgimg = IMG_Load("romfs:/background.png");

	SDL_BlitSurface(bgimg, NULL, sdl, NULL);
	SDL_Flip(sdl);
	consoleInit(GFX_BOTTOM, NULL);

	mkpath(config_filename, false);

	// init 3DS buttons to their values
	for (int i=0; buttons3ds[i].key!=0; ++i)
		SDL_N3DSKeyBind(buttons3ds[i].key, buttons3ds[i].sdl_key);

	SOC_buffer = (u32*)memalign(SOC_ALIGN, SOC_BUFFERSIZE);
	socInit(SOC_buffer, SOC_BUFFERSIZE);

	rfbClientLog=rfbClientErr=log_citra;

	while (1) {
		// get config
		vnc_config config;

		if (getconfig(&config) ||
			config.host[0] == 0) goto quit;
		
		user1=config.user;
		pass1=config.pass;
		printf("\x1b[2J"); // clear screen
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
				snprintf(buf, sizeof(buf),"http://%s:%d/",config.host, config.audioport);
				start_stream(buf);
			}
		}

		// clear mouse state
		buttonMask = 0;
		int ext=0;
		while(cl) {
			// handle events
			// must be called once per frame to expire mouse button presses
			uib_handle_tap_processing(NULL);
			while (SDL_PollEvent(&e)) {
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
			SDL_Flip(rfbClientGetClientData(cl, SDL_Init));
		}
		if (config.audioport)
			stop_stream();
		cleanup();

		printf("\x1b[46;30mA:retry B:quit                          \x1b[0m");
		while (1) {
			while (!SDL_PollEvent(&e)) SDL_Delay(20);
			if (e.type == SDL_KEYDOWN) {
				if (e.key.keysym.sym == SDLK_a)
					break;
				if (e.key.keysym.sym == SDLK_b)
					goto quit;
			}
		}
	}
quit:
	safeexit();
}
