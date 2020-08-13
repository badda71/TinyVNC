/**
 * @example SDLvncviewer.c
 * Once built, you can run it via `SDLvncviewer <remote-host>`.
 */

#include <malloc.h>
#include <3ds.h>
#include <SDL/SDL.h>
#include <rfb/rfbclient.h>

struct { int sdl; int rfb; } buttonMapping[]={
	{1, rfbButton1Mask},
	{2, rfbButton2Mask},
	{3, rfbButton3Mask},
	{4, rfbButton4Mask},
	{5, rfbButton5Mask},
	{0,0}
};

struct { char mask; int bits_stored; } utf8Mapping[]= {
	{0b00111111, 6},
	{0b01111111, 7},
	{0b00011111, 5},
	{0b00001111, 4},
	{0b00000111, 3},
	{0,0}
};

static int viewOnly=0, buttonMask;
/* client's pointer position */
int x,y;

/*
// default
struct {
	unsigned int key;
	SDLKey sdl_key;
	rfbKeySym rfb_key;
} buttons3ds[] = {
	{KEY_A, SDLK_a,	XK_a},
	{KEY_B, SDLK_b, XK_b},
	{KEY_X, SDLK_x, XK_x},
	{KEY_Y, SDLK_y, XK_y},
	{KEY_L, SDLK_q, XK_q},
	{KEY_R, SDLK_w, XK_w},
	{KEY_ZL, SDLK_1, XK_1},
	{KEY_ZR, SDLK_2, XK_2},
	{KEY_START, SDLK_RETURN, XK_Return},
	{KEY_SELECT, SDLK_ESCAPE, XK_Escape},

	{KEY_CPAD_UP, SDLK_UP, XK_Up}, // C-PAD
	{KEY_CPAD_DOWN, SDLK_DOWN, XK_Down},
	{KEY_CPAD_LEFT, SDLK_LEFT, XK_Left},
	{KEY_CPAD_RIGHT, SDLK_RIGHT, XK_Right},

	{KEY_DUP, SDLK_t, XK_t},	// D-PAD
	{KEY_DDOWN, SDLK_g, XK_g},
	{KEY_DLEFT, SDLK_f, XK_f},
	{KEY_DRIGHT, SDLK_h, XK_h},

	{KEY_CSTICK_UP, SDLK_i, XK_i}, // C-STICK
	{KEY_CSTICK_DOWN, SDLK_k, XK_k},
	{KEY_CSTICK_LEFT, SDLK_j, XK_j},
	{KEY_CSTICK_RIGHT, SDLK_l, XK_l},

	{0,0,0}
};
*/
// breath of the wild in cemu
struct {
	unsigned int key;
	SDLKey sdl_key;
	rfbKeySym rfb_key;
} buttons3ds[] = {
	{KEY_A, SDLK_a,	XK_space},
	{KEY_B, SDLK_b, XK_e},
	{KEY_X, SDLK_x, XK_q},
	{KEY_Y, SDLK_y, XK_c},
	{KEY_L, SDLK_q, XK_1},
	{KEY_R, SDLK_w, XK_4},
	{KEY_ZL, SDLK_1, XK_2},
	{KEY_ZR, SDLK_2, XK_3},
	{KEY_START, SDLK_RETURN, XK_plus},
	{KEY_SELECT, SDLK_ESCAPE, XK_minus},

	{KEY_CPAD_UP, SDLK_UP, XK_w}, // C-PAD
	{KEY_CPAD_DOWN, SDLK_DOWN, XK_s},
	{KEY_CPAD_LEFT, SDLK_LEFT, XK_a},
	{KEY_CPAD_RIGHT, SDLK_RIGHT, XK_d},

	{KEY_DUP, SDLK_t, XK_t},	// D-PAD
	{KEY_DDOWN, SDLK_g, XK_g},
	{KEY_DLEFT, SDLK_f, XK_f},
	{KEY_DRIGHT, SDLK_h, XK_h},

	{KEY_CSTICK_UP, SDLK_i, XK_KP_8}, // C-STICK
	{KEY_CSTICK_DOWN, SDLK_k, XK_KP_5},
	{KEY_CSTICK_LEFT, SDLK_j, XK_KP_4},
	{KEY_CSTICK_RIGHT, SDLK_l, XK_KP_6},

	{0,0,0}
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

	client->updateRect.x = client->updateRect.y = 0;
	client->updateRect.w = width;
	client->updateRect.h = height;

	/* (re)create the surface used as the client's framebuffer */
	int flags = 0; // SDL_CONSOLEBOTTOM
	if (width > 400 || height > 240) {
		flags |= ((width * 1000) / 400 > (height * 1000) / 240)? SDL_FITWIDTH : SDL_FITHEIGHT;
	}
	SDL_Surface* sdl = SDL_SetVideoMode(width, height, depth, flags);

	if(!sdl) {
		rfbClientErr("resize: error creating surface: %s\n", SDL_GetError());
		return FALSE;
	}

	SDL_ShowCursor(SDL_DISABLE);
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

static rfbKeySym SDL_key2rfbKeySym(SDL_KeyboardEvent* e) {
	SDLKey sym = e->keysym.sym;

	// init 3DS buttons to their values
	for (int i=0; buttons3ds[i].key!=0; ++i)
		if (sym == buttons3ds[i].sdl_key) return buttons3ds[i].rfb_key;
	return 0;
}

static void update(rfbClient* cl,int x,int y,int w,int h) {
}

static void kbd_leds(rfbClient* cl, int value, int pad) {
	/* note: pad is for future expansion 0=unused */
	fprintf(stderr,"Led State= 0x%02X\n", value);
	fflush(stderr);
}

/* trivial support for textchat */
static void text_chat(rfbClient* cl, int value, char *text) {
	switch(value) {
	case rfbTextChatOpen:
		fprintf(stderr,"TextChat: We should open a textchat window!\n");
		TextChatOpen(cl);
		break;
	case rfbTextChatClose:
		fprintf(stderr,"TextChat: We should close our window!\n");
		break;
	case rfbTextChatFinished:
		fprintf(stderr,"TextChat: We should close our window!\n");
		break;
	default:
		fprintf(stderr,"TextChat: Received \"%s\"\n", text);
		break;
	}
	fflush(stderr);
}

static void cleanup(rfbClient* cl)
{
  if(cl)
    rfbClientCleanup(cl);
}


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
			x = e->motion.x;
			y = e->motion.y;
			state = e->motion.state;
		}
		else {
			x = e->button.x;
			y = e->button.y;
			state = e->button.button;
			for (i = 0; buttonMapping[i].sdl; i++)
				if (state == buttonMapping[i].sdl) {
					state = buttonMapping[i].rfb;
					if (e->type == SDL_MOUSEBUTTONDOWN)
						buttonMask |= state;
					else
						buttonMask &= ~state;
					break;
				}
		}
		SendPointerEvent(cl, x, y, buttonMask);
		buttonMask &= ~(rfbButton4Mask | rfbButton5Mask);
		break;
	}
	case SDL_KEYUP:
	case SDL_KEYDOWN:
		if (viewOnly)
			break;
		SendKeyEvent(cl, SDL_key2rfbKeySym(&e->key),
			e->type == SDL_KEYDOWN ? TRUE : FALSE);
		break;
	case SDL_QUIT:
		rfbClientCleanup(cl);
		exit(0);
	default:
		rfbClientLog("ignore SDL event: 0x%x\n", e->type);
	}
	return TRUE;
}

//static void got_selection(rfbClient *cl, const char *text, int len){}

static rfbCredential* get_credential(rfbClient* cl, int credentialType){
        rfbCredential *c = malloc(sizeof(rfbCredential));
	c->userCredential.username = malloc(RFB_BUF_SIZE);
	c->userCredential.password = malloc(RFB_BUF_SIZE);

	if(credentialType != rfbCredentialTypeUser) {
	    rfbClientErr("something else than username and password required for authentication\n");
	    return NULL;
	}

	rfbClientLog("username and password required for authentication!\n");
	printf("user: ");
	fgets(c->userCredential.username, RFB_BUF_SIZE, stdin);
	printf("pass: ");
	fgets(c->userCredential.password, RFB_BUF_SIZE, stdin);

	/* remove trailing newlines */
	c->userCredential.username[strcspn(c->userCredential.username, "\n")] = 0;
	c->userCredential.password[strcspn(c->userCredential.password, "\n")] = 0;

	return c;
}

#define SOC_ALIGN       0x1000
#define SOC_BUFFERSIZE  0x100000
static u32 *SOC_buffer = NULL;

int main() {
	rfbClient* cl;
	int i;
	SDL_Event e;

	osSetSpeedupEnable(1);

	// init SDL
	SDL_Init(SDL_INIT_VIDEO);
	atexit(SDL_Quit);
	consoleInit(GFX_BOTTOM, NULL);
	SDL_SetVideoMode(400, 240, 32, SDL_SWSURFACE);

	// init 3DS buttons to their values
	for (int i=0; buttons3ds[i].key!=0; ++i)
		SDL_N3DSKeyBind(buttons3ds[i].key, buttons3ds[i].sdl_key);

	SOC_buffer = (u32*)memalign(SOC_ALIGN, SOC_BUFFERSIZE);
	socInit(SOC_buffer, SOC_BUFFERSIZE);

	rfbClientLog=rfbClientErr=log_citra;
	
	/* 16-bit: cl=rfbGetClient(5,3,2); */
	cl=rfbGetClient(8,3,4);
	cl->MallocFrameBuffer=resize;
	cl->canHandleNewFBSize = TRUE;
	cl->GotFrameBufferUpdate=update;
	cl->HandleKeyboardLedState=kbd_leds;
	cl->HandleTextChat=text_chat;
//		cl->GotXCutText = got_selection;
	cl->GetCredential = get_credential;
	cl->listenPort = LISTEN_PORT_OFFSET;
	cl->listen6Port = LISTEN_PORT_OFFSET;
	char *argv[] = {
		"vncviewer",
		"127.0.0.1:5901"};
	int argc = 2;

	if(!rfbInitClient(cl, &argc, argv))
	{
		cl = NULL; /* rfbInitClient has already freed the client struct */
	}

	while(cl) {
		if(SDL_PollEvent(&e)) {
			/*
			handleSDLEvent() return 0 if user requested window close.
			In this case, handleSDLEvent() will have called cleanup().
			*/
			if(!handleSDLEvent(cl, &e))
			break;
		}
		else {
			i=WaitForMessage(cl,500);
			if(i<0)
			{
				break;
			}
			if(i)
				if(!HandleRFBServerMessage(cl))
				{
					break;
				}
		}
		SDL_Flip(rfbClientGetClientData(cl, SDL_Init));
	}


	// Main loop
	printf("press START to exit\n");
	if (cl) SDL_Flip(rfbClientGetClientData(cl, SDL_Init));
	while (aptMainLoop())
	{
		gspWaitForVBlank();
		//gfxSwapBuffers();
		hidScanInput();

		// Your code goes here
		u32 kDown = hidKeysDown();
		if (kDown & KEY_START)
			break; // break in order to return to hbmenu
	}

	socExit();
	cleanup(cl);
	return 0;
}
