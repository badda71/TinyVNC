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

    This file written by Ryan C. Gordon (icculus@icculus.org)
*/
#include "SDL_config.h"

/* Output audio to nowhere... */

#include "SDL_rwops.h"
#include "SDL_timer.h"
#include "SDL_audio.h"
#include "../SDL_audiomem.h"
#include "../SDL_audio_c.h"
#include "../SDL_audiodev_c.h"
#include "SDL_n3dsaudio.h"

#include <3ds.h>
#include <assert.h>

/* The tag name used by N3DS audio */
#define N3DSAUD_DRIVER_NAME         "n3ds"

static dspHookCookie g_dspHook;
static SDL_AudioDevice *g_audDev;

/* Audio driver functions */
static int N3DSAUD_OpenAudio(_THIS, SDL_AudioSpec *spec);
static void N3DSAUD_WaitAudio(_THIS);
static void N3DSAUD_PlayAudio(_THIS);
static Uint8 *N3DSAUD_GetAudioBuf(_THIS);
static void N3DSAUD_CloseAudio(_THIS);

/* Audio driver bootstrap functions */
static int N3DSAUD_Available(void)
{
	return(1);
}

static inline void contextLock(_THIS)
{
	LightLock_Lock(&this->hidden->lock);
}

static inline void contextUnlock(_THIS)
{
	LightLock_Unlock(&this->hidden->lock);
}

static void N3DSAUD_LockAudio(_THIS)
{
	contextLock(this);
}

static void N3DSAUD_UnlockAudio(_THIS)
{
	contextUnlock(this);
}

static void N3DSAUD_DeleteDevice(SDL_AudioDevice *device)
{
	if ( device->hidden->mixbuf != NULL ) {
		SDL_FreeAudioMem(device->hidden->mixbuf);
		device->hidden->mixbuf = NULL;
	}
	if 	( device->hidden->waveBuf[0].data_vaddr!= NULL ) {
		linearFree((void*)device->hidden->waveBuf[0].data_vaddr);
		device->hidden->waveBuf[0].data_vaddr = NULL;
	}

	ndspExit();

	SDL_free(device->hidden);
	SDL_free(device);
}

static SDL_AudioDevice *N3DSAUD_CreateDevice(int devindex)
{
	SDL_AudioDevice *this;

	/* Initialize all variables that we clean on shutdown */
	this = (SDL_AudioDevice *)SDL_malloc(sizeof(SDL_AudioDevice));
	if ( this ) {
		SDL_memset(this, 0, (sizeof *this));
		this->hidden = (struct SDL_PrivateAudioData *)
				SDL_malloc((sizeof *this->hidden));
	}
	if ( (this == NULL) || (this->hidden == NULL) ) {
		SDL_OutOfMemory();
		if ( this ) {
			SDL_free(this);
		}
		return(0);
	}

	//start 3ds DSP init
	Result rc = ndspInit();
	if (R_FAILED(rc)) {
		SDL_free(this);
		if ((R_SUMMARY(rc) == RS_NOTFOUND) && (R_MODULE(rc) == RM_DSP))
			SDL_SetError("DSP init failed: dspfirm.cdc missing!");
		else
			SDL_SetError("DSP init failed. Error code: 0x%X", rc);
		return(0);
	}

	/* Initialize internal state */
	SDL_memset(this->hidden, 0, (sizeof *this->hidden));
	LightLock_Init(&this->hidden->lock);
	CondVar_Init(&this->hidden->cv);

	/* Set the function pointers */
	this->OpenAudio = N3DSAUD_OpenAudio;
	this->WaitAudio = N3DSAUD_WaitAudio;
	this->PlayAudio = N3DSAUD_PlayAudio;
	this->GetAudioBuf = N3DSAUD_GetAudioBuf;
	this->CloseAudio = N3DSAUD_CloseAudio;
	this->LockAudio = N3DSAUD_LockAudio;
	this->UnlockAudio = N3DSAUD_UnlockAudio;
	this->free = N3DSAUD_DeleteDevice;

	return this;
}

AudioBootStrap N3DSAUD_bootstrap = {
	N3DSAUD_DRIVER_NAME, "SDL N3DS audio driver",
	N3DSAUD_Available, N3DSAUD_CreateDevice
};

/* called by ndsp thread on each audio frame */
static void audioFrameFinished(void *context)
{
	SDL_AudioDevice *this = (SDL_AudioDevice *) context;

	contextLock(this);

	bool shouldBroadcast = false;
	unsigned i;
	for (i = 0; i < NUM_BUFFERS; i ++) {
		if (this->hidden->waveBuf[i].status == NDSP_WBUF_DONE) {
			this->hidden->waveBuf[i].status = NDSP_WBUF_FREE;
			shouldBroadcast = true;
		}
	}

	if (shouldBroadcast)
		CondVar_Broadcast(&this->hidden->cv);

	contextUnlock(this);
}

/* This function blocks until it is possible to write a full sound buffer */
static void N3DSAUD_WaitAudio(_THIS)
{
	contextLock(this);
	while (!this->hidden->isCancelled && this->hidden->waveBuf[this->hidden->nextbuf].status != NDSP_WBUF_FREE) {
		CondVar_Wait(&this->hidden->cv, &this->hidden->lock);
	}
	contextUnlock(this);
}

static void N3DSAUD_DspHook(DSP_HookType hook)
{
	if (hook == DSPHOOK_ONCANCEL)
	{
		contextLock(g_audDev);
		g_audDev->hidden->isCancelled = true;
		g_audDev->enabled = false;
		CondVar_Broadcast(&g_audDev->hidden->cv);
		contextUnlock(g_audDev);
	}
}

static void N3DSAUD_PlayAudio(_THIS)
{
	contextLock(this);

	size_t nextbuf = this->hidden->nextbuf;
	size_t sampleLen = this->hidden->mixlen;

	if (this->hidden->isCancelled || this->hidden->waveBuf[nextbuf].status != NDSP_WBUF_FREE)
	{
		contextUnlock(this);
		return;
	}

	this->hidden->nextbuf = (nextbuf + 1) % NUM_BUFFERS;

	contextUnlock(this);

	memcpy((void*)this->hidden->waveBuf[nextbuf].data_vaddr,this->hidden->mixbuf,sampleLen);
	DSP_FlushDataCache(this->hidden->waveBuf[nextbuf].data_vaddr,sampleLen);

	ndspChnWaveBufAdd(0, &this->hidden->waveBuf[nextbuf]);
}

static Uint8 *N3DSAUD_GetAudioBuf(_THIS)
{
	return(this->hidden->mixbuf);
}

static void N3DSAUD_CloseAudio(_THIS)
{
	contextLock(this);

	dspUnhook(&g_dspHook);
	ndspSetCallback(NULL, NULL);
	if (!this->hidden->isCancelled)
		ndspChnReset(0);

	if ( this->hidden->mixbuf != NULL ) {
		SDL_FreeAudioMem(this->hidden->mixbuf);
		this->hidden->mixbuf = NULL;
	}
	if ( this->hidden->waveBuf[0].data_vaddr!= NULL ) {
		linearFree((void*)this->hidden->waveBuf[0].data_vaddr);
		this->hidden->waveBuf[0].data_vaddr = NULL;
	}

	if (!this->hidden->isCancelled) {
		memset(this->hidden->waveBuf,0,sizeof(ndspWaveBuf)*NUM_BUFFERS);
		CondVar_Broadcast(&this->hidden->cv);
	}

	contextUnlock(this);
}

static int N3DSAUD_OpenAudio(_THIS, SDL_AudioSpec *spec)
{
	if (this->hidden->isCancelled) {
		SDL_SetError("DSP is in cancelled state");
		return (-1);
	}

	if(spec->channels > 2)
		spec->channels = 2;

    Uint16 test_format = SDL_FirstAudioFormat(spec->format);
    int valid_datatype = 0;
    while ((!valid_datatype) && (test_format)) {
        spec->format = test_format;
        switch (test_format) {
			case AUDIO_S8:
				/* Signed 8-bit audio supported */
				this->hidden->format=(spec->channels==2)?NDSP_FORMAT_STEREO_PCM8:NDSP_FORMAT_MONO_PCM8;
				this->hidden->isSigned=1;
				this->hidden->bytePerSample = (spec->channels);
				   valid_datatype = 1;
				break;
			case AUDIO_S16:
				/* Signed 16-bit audio supported */
				this->hidden->format=(spec->channels==2)?NDSP_FORMAT_STEREO_PCM16:NDSP_FORMAT_MONO_PCM16;
				this->hidden->isSigned=1;
				this->hidden->bytePerSample = (spec->channels) * 2;
				   valid_datatype = 1;
				break;
			default:
				test_format = SDL_NextAudioFormat();
				break;
		}
	}

    if (!valid_datatype) {  /* shouldn't happen, but just in case... */
        SDL_SetError("Unsupported audio format");
        return (-1);
    }

	/* Update the fragment size as size in bytes */
	SDL_CalculateAudioSpec(spec);

	/* Allocate mixing buffer */
	if (spec->size >= UINT32_MAX/2)
		return(-1);
	this->hidden->mixlen = spec->size;
	this->hidden->mixbuf = (Uint8 *) SDL_AllocAudioMem(spec->size);
	if ( this->hidden->mixbuf == NULL ) {
		return(-1);
	}
	SDL_memset(this->hidden->mixbuf, spec->silence, spec->size);

	Uint8 * temp = (Uint8 *) linearAlloc(this->hidden->mixlen*NUM_BUFFERS);
	if (temp == NULL ) {
		SDL_free(this->hidden->mixbuf);
		return(-1);
	}
	memset(temp,0,this->hidden->mixlen*NUM_BUFFERS);
	DSP_FlushDataCache(temp,this->hidden->mixlen*NUM_BUFFERS);

	this->hidden->nextbuf = 0;
	this->hidden->channels = spec->channels;
	this->hidden->samplerate = spec->freq;

 	ndspChnReset(0);

	ndspChnSetInterp(0, NDSP_INTERP_LINEAR);
	ndspChnSetRate(0, spec->freq);
	ndspChnSetFormat(0, this->hidden->format);

	float mix[12];
	memset(mix, 0, sizeof(mix));
	mix[0] = 1.0;
	mix[1] = 1.0;
	ndspChnSetMix(0, mix);

	memset(this->hidden->waveBuf,0,sizeof(ndspWaveBuf)*NUM_BUFFERS);

	unsigned i;
	for (i = 0; i < NUM_BUFFERS; i ++) {
		this->hidden->waveBuf[i].data_vaddr = temp;
		this->hidden->waveBuf[i].nsamples = this->hidden->mixlen / this->hidden->bytePerSample;
		temp += this->hidden->mixlen;
	}

	/* Setup callback */
	g_audDev = this;
	ndspSetCallback(audioFrameFinished, this);
	dspHook(&g_dspHook, N3DSAUD_DspHook);

	/* We're ready to rock and roll. :-) */
	return(0);
}
