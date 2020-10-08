/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2014 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software
     in a product, an acknowledgment in the product documentation would be
     appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.
*/
//#include "../../SDL_internal.h"

#include <stdio.h>
#include <stdlib.h>

#include "SDL_config.h"

#include "SDL_error.h"
#include "SDL_thread.h"

#include <3ds.h>

struct SDL_semaphore {
    LightSemaphore sem;
};


/* Create a semaphore */
SDL_sem *SDL_CreateSemaphore(Uint32 initial_value)
{
    SDL_sem *sem;

    sem = (SDL_sem *) malloc(sizeof(*sem));
    if (sem != NULL) {
        LightSemaphore_Init(&sem->sem, initial_value, 255);
    } else {
        SDL_OutOfMemory();
    }

    return sem;
}

/* Free the semaphore */
void SDL_DestroySemaphore(SDL_sem *sem)
{
    if (sem != NULL) {
        free(sem);
    }
}

/* Waits on a semaphore */
int SDL_SemWaitTimeout(SDL_sem *sem, Uint32 timeout)
{
    if (sem == NULL) {
        SDL_SetError("Passed a NULL sem");
        return -1;
    }

    if (timeout == SDL_MUTEX_MAXWAIT) {
        LightSemaphore_Acquire(&sem->sem, 1);
        return 0;
    }
    else if (timeout == 0) {
        int rc = LightSemaphore_TryAcquire(&sem->sem, 1);
        return rc != 0 ? SDL_MUTEX_TIMEDOUT : 0;
    }
    else {
        SDL_SetError("Non-trivial semaphore wait timeout values not supported");
        return -1;
    }
}

int SDL_SemTryWait(SDL_sem *sem)
{
    return SDL_SemWaitTimeout(sem, 0);
}

int SDL_SemWait(SDL_sem *sem)
{
    return SDL_SemWaitTimeout(sem, SDL_MUTEX_MAXWAIT);
}

/* Returns the current count of the semaphore */
Uint32 SDL_SemValue(SDL_sem *sem)
{
    return sem->sem.current_count;
}

int SDL_SemPost(SDL_sem *sem)
{
    LightSemaphore_Release(&sem->sem, 1);
    return 0;
}
