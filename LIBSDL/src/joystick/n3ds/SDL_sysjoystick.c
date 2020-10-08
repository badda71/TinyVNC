#include "SDL_config.h"

#include <3ds.h>

#include "SDL_error.h"
#include "SDL_events.h"
#include "SDL_joystick.h"
#include "../SDL_sysjoystick.h"
#include "../SDL_joystick_c.h"

#include "../../video/n3ds/SDL_n3dsvideo.h"

int old_x = 0, old_y = 0;
int old_cs_x = 0, old_cs_y = 0;
u32 key_press, key_release = 0;
int old_dpad_state = 0;

int SDL_SYS_JoystickInit (void) {
	SDL_numjoysticks = 1;

	return 1;
}

const char *SDL_SYS_JoystickName (int index) {
	if(!index)
		return "3DS builtin joypad";

	SDL_SetError ("No joystick available with that index");

	return (NULL);
}

int SDL_SYS_JoystickOpen(SDL_Joystick *joystick) {
	joystick->nbuttons = 10;
	joystick->nhats = 1;
	joystick->nballs = 0;
	joystick->naxes = 4;

	return 0;
}

void SDL_SYS_JoystickUpdate (SDL_Joystick *joystick) {

	if (!aptIsActive()) {
		// Avoid pumping events if we are not in the foreground.
		return;
	}

	circlePosition circlePad;
	hidCircleRead(&circlePad);
	int x,y;
	x = circlePad.dx;
	y = circlePad.dy;
	if (x > 156) x= 156;
	if (x < -156) x= -156;
	if (y > 156) y= 156;
	if (y < -156) y= -156;
	if (old_x != x) {
		old_x = x;
		SDL_PrivateJoystickAxis (joystick, 0, x * 210);
	}
	if (old_y != y) {
		old_y = y;
		SDL_PrivateJoystickAxis (joystick, 1, - y * 210);
	}

	circlePosition circleStick = {0};
	irrstCstickRead(&circleStick);
	x = circleStick.dx;
	y = circleStick.dy;
	if (x > 156) x= 156;
	if (x < -156) x= -156;
	if (y > 156) y= 156;
	if (y < -156) y= -156;
	if (old_cs_x != x) {
		old_cs_x = x;
		SDL_PrivateJoystickAxis (joystick, 2, x * 210);
	}
	if (old_cs_y != y) {
		old_cs_y = y;
		SDL_PrivateJoystickAxis (joystick, 3, - y * 210);
	}

	key_press = hidKeysDown ();
	int dpad_state = old_dpad_state;
	if ((key_press & KEY_A)) {
		SDL_PrivateJoystickButton (joystick, 1, SDL_PRESSED);
	}
	if ((key_press & KEY_B)) {
		SDL_PrivateJoystickButton (joystick, 2, SDL_PRESSED);
	}
	if ((key_press & KEY_X)) {
		SDL_PrivateJoystickButton (joystick, 3, SDL_PRESSED);
	}
	if ((key_press & KEY_Y)) {
		SDL_PrivateJoystickButton (joystick, 4, SDL_PRESSED);
	}
	if ((key_press & KEY_SELECT)) {
		SDL_PrivateJoystickButton (joystick, 7, SDL_PRESSED);
	}
	if ((key_press & KEY_START)) {
		SDL_PrivateJoystickButton (joystick, 0, SDL_PRESSED);
	}
	if ((key_press & KEY_L)) {
		SDL_PrivateJoystickButton (joystick, 5, SDL_PRESSED);
	}
	if ((key_press & KEY_R)) {
		SDL_PrivateJoystickButton (joystick, 6, SDL_PRESSED);
	}
	if ((key_press & KEY_DDOWN)) {
		dpad_state |= SDL_HAT_DOWN;
	}
	if ((key_press & KEY_DLEFT)) {
		dpad_state |= SDL_HAT_LEFT;
	}
	if ((key_press & KEY_DUP)) {
		dpad_state |= SDL_HAT_UP;
	}
	if ((key_press & KEY_DRIGHT)) {
		dpad_state |= SDL_HAT_RIGHT;
	}
	if ((key_press & KEY_ZL)) {
		SDL_PrivateJoystickButton (joystick, 8, SDL_PRESSED);
	}
	if ((key_press & KEY_ZR)) {
		SDL_PrivateJoystickButton (joystick, 9, SDL_PRESSED);
	}

	key_release = hidKeysUp ();
	if ((key_release & KEY_A)) {
		SDL_PrivateJoystickButton (joystick, 1, SDL_RELEASED);
	}
	if ((key_release & KEY_B)) {
		SDL_PrivateJoystickButton (joystick, 2, SDL_RELEASED);
	}
	if ((key_release & KEY_X)) {
		SDL_PrivateJoystickButton (joystick, 3, SDL_RELEASED);
	}
	if ((key_release & KEY_Y)) {
		SDL_PrivateJoystickButton (joystick, 4, SDL_RELEASED);
	}
	if ((key_release & KEY_SELECT)) {
		SDL_PrivateJoystickButton (joystick, 7, SDL_RELEASED);
	}
	if ((key_release & KEY_START)) {
		SDL_PrivateJoystickButton (joystick, 0, SDL_RELEASED);
	}
	if ((key_release & KEY_L)) {
		SDL_PrivateJoystickButton (joystick, 5, SDL_RELEASED);
	}
	if ((key_release & KEY_R)) {
		SDL_PrivateJoystickButton (joystick, 6, SDL_RELEASED);
	}
	if ((key_release & KEY_DDOWN)) {
		dpad_state &= ~SDL_HAT_DOWN;
	}
	if ((key_release & KEY_DLEFT)) {
		dpad_state &= ~SDL_HAT_LEFT;
	}
	if ((key_release & KEY_DUP)) {
		dpad_state &= ~SDL_HAT_UP;
	}
	if ((key_release & KEY_DRIGHT)) {
		dpad_state &= ~SDL_HAT_RIGHT;
	}
	if ((key_release & KEY_ZL)) {
		SDL_PrivateJoystickButton (joystick, 8, SDL_RELEASED);
	}
	if ((key_release & KEY_ZR)) {
		SDL_PrivateJoystickButton (joystick, 9, SDL_RELEASED);
	}

	if (dpad_state != old_dpad_state) {
		old_dpad_state = dpad_state;
		SDL_PrivateJoystickHat (joystick, 0, dpad_state);
	}
}

void SDL_SYS_JoystickClose (SDL_Joystick *joystick) {
}

void SDL_SYS_JoystickQuit (void) {
}
