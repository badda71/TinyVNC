#include "SDL_config.h"

#include <sys/time.h>
#include <3ds.h>
#include <sys/select.h>


#include "SDL_thread.h"
#include "SDL_timer.h"
#include "SDL_error.h"
#include "../SDL_timer_c.h"

static u64 g_startTicks;

void SDL_StartTicks (void) {
	g_startTicks = svcGetSystemTick();
}

Uint32 SDL_GetTicks (void) {
	u64 elapsed = svcGetSystemTick() - g_startTicks;
	return elapsed * 1000 / SYSCLOCK_ARM11;
}

void SDL_Delay (Uint32 ms) {
	svcSleepThread((u64)ms * 1000000ULL);
}

int SDL_SYS_TimerInit (void) {
	return 0;
}

void SDL_SYS_TimerQuit (void) {
}

int SDL_SYS_StartTimer (void) {
	SDL_SetError ("Timers not implemented on 3DS");

	return -1;
}

void SDL_SYS_StopTimer (void) {
}
