/*
    SDL - Simple DirectMedia Layer
    Copyright (C) 1997-2012 Sam Lantinga

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

    Sam Lantinga
    slouken@libsdl.org
*/
#include "SDL_config.h"

/* Being a null driver, there's no event stream. We just define stubs for
   most of the API. */

#include <3ds.h>

#include "SDL.h"
#include "../../events/SDL_sysevents.h"
#include "../../events/SDL_events_c.h"

#include "SDL_n3dsvideo.h"
#include "SDL_n3dsevents_c.h"

void N3DS_PumpEvents(_THIS)
{
	svcSleepThread(100000); // 0.1 ms

	if (!aptMainLoop())
	{
		static bool pushedQuit = false;
		if (!pushedQuit) {
			SDL_Event sdlevent;
			sdlevent.type = SDL_QUIT;
			SDL_PushEvent(&sdlevent);
			pushedQuit = true;
		}
		return;
	}

	hidScanInput();

	if (hidKeysHeld() & KEY_TOUCH) {
		touchPosition touch;

		hidTouchRead (&touch);

// TO DO: handle fit screen on x and y.Y and Y to be considered separately

		if(this->hidden->screens&SDL_TOPSCR && this->hidden->screens&SDL_BOTTOMSCR) {
			if (touch.px != 0 || touch.py != 0) {
				SDL_PrivateMouseMotion (0, 0,
					touch.px  + (this->hidden->w1 - 320)/2,
					this->hidden->y2 + touch.py + (this->hidden->h2 - 240)/2);
				if (!SDL_GetMouseState (NULL, NULL))
					SDL_PrivateMouseButton (SDL_PRESSED, 1, 0, 0);
			}
		} else {
			if (touch.px != 0 || touch.py != 0) {
				SDL_PrivateMouseMotion (0, 0, (touch.px * this->hidden->w1) / 320, (touch.py * this->hidden->h1) / 240);
				if (!SDL_GetMouseState (NULL, NULL))
					SDL_PrivateMouseButton (SDL_PRESSED, 1, 0, 0);
			}
		}
	} else {
		if (SDL_GetMouseState (NULL, NULL))
			SDL_PrivateMouseButton (SDL_RELEASED, 1, 0, 0);
	}
}

void N3DS_InitOSKeymap(_THIS)
{
	// Do nothing
}


/* end of SDL_nullevents.c ... */

