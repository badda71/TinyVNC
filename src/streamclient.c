/*
 * TinyVNC - A VNC client for Nintendo 3DS
 *
 * streamclient.c - functions for handling mp3 audio streaming
 *
 * Copyright 2020 Sebastian Weber
 */

#include <stdarg.h>
#include <3ds.h>
#include <stdio.h>
#include <malloc.h>
#include <string.h>
#include <curl/curl.h>
#include <rfb/rfbclient.h> // only for logging functions
#include "httpstatuscodes_c.h"
#include "streamclient.h"
#include "decoder.h"
#include "mp3decoder.h"
//#include "opusdecoder.h"

/* Channel to play music on */
#define CHANNEL	0x08

typedef struct _waveBuffer {
	struct _waveBuffer *next;
	ndspWaveBuf buf;
} waveBuffer;

// CURL variables
static CURL				*curl = NULL;
static CURLM			*mcurl = NULL;
static int				still_running = 0;
static int				curl_paused = 0;

// audio / ndsp variables
static waveBuffer		*waveBuf_tail = NULL;
static waveBuffer		*waveBuf_head = NULL;
static u32				queuedSamples = 0;
static int				ndsp_channels = 0;
static int				ndsp_rate = 0;
static int				max_wavbuf_size = 0;

// decoder specific variables
static audioDecoder		decoder={0};
static int				stream_bitrate=0;
static char				*stream_type=NULL;

void sound_close()
{
	// stop playing
	ndspChnReset(CHANNEL);
	ndspChnWaveBufClear(CHANNEL);
	// free all sound buffers
	while (waveBuf_tail) {
		waveBuffer *next = waveBuf_tail->next;
		if (waveBuf_tail->buf.data_vaddr) linearFree(waveBuf_tail->buf.data_pcm8);
		free(waveBuf_tail);
		waveBuf_tail=next;
	}
	waveBuf_head=NULL;
	queuedSamples = ndsp_channels = ndsp_rate = 0;
}

static void sound_open(long rate, int channels)
{
	max_wavbuf_size = rate; // 1 second max delay

	ndspSetOutputMode(channels == 2 ? NDSP_OUTPUT_STEREO : NDSP_OUTPUT_MONO);
	ndspChnSetInterp(CHANNEL, NDSP_INTERP_POLYPHASE);
	ndspChnSetRate(CHANNEL, (float)rate);
	ndspChnSetFormat(CHANNEL,
		channels == 2 ? NDSP_FORMAT_STEREO_PCM16 : NDSP_FORMAT_MONO_PCM16);
	ndsp_channels = channels;
}

static void sound_clean_buffers() {
	// free played sound buffers
	waveBuffer *w;
	while (waveBuf_tail && waveBuf_tail->buf.status == NDSP_WBUF_DONE) {
		w = waveBuf_tail->next;
		if (waveBuf_tail->buf.data_vaddr) linearFree(waveBuf_tail->buf.data_pcm8);
		queuedSamples -= waveBuf_tail->buf.nsamples;
		free(waveBuf_tail);
		waveBuf_tail = w;
	}
	if (!waveBuf_tail) waveBuf_head=NULL;
}

static int sound_play(unsigned char *pbuf, size_t size)
{
	waveBuffer *w;
	w = calloc(1, sizeof(waveBuffer));
	if (!w) {
		rfbClientErr("alloc1 error");
		return -1;
	}
	w->buf.data_vaddr = linearAlloc(size);
	if (!w->buf.data_vaddr) {
		rfbClientErr("alloc2 error");
		return -1;
	}
	memcpy((void *)w->buf.data_vaddr, pbuf, size);
	w->buf.nsamples = size / (ndsp_channels * sizeof(int16_t));
	ndspChnWaveBufAdd(CHANNEL, &(w->buf));
	if (!waveBuf_tail) waveBuf_tail = w;
	if (waveBuf_head) waveBuf_head->next = w;
	waveBuf_head = w;
	queuedSamples += w->buf.nsamples;
	return 0;
}

static size_t stream_write_callback(void *buffer, size_t size, size_t nmemb, void *userp) {
//log_citra("enter %s",__func__);
	unsigned char *audio;
	int done;

	// choke if we have already buffered enough
	sound_clean_buffers();
	if (queuedSamples > max_wavbuf_size) {
		curl_paused = 1;
		return CURL_WRITEFUNC_PAUSE;
	}

	// do we have an encoder ready already?
	if (decoder.init == NULL) {
		if (mp3_checkmagic(buffer) == 0) mp3_create_decoder(&decoder);
		//else if (opus_checkmagic(buffer) == 0) opus_create_decoder(&decoder);
		// ... add more decoders here
		else return 0;
		decoder.init();
	}

	// feed the decoder
	if (decoder.feed(buffer, size * nmemb) !=0)
		return 0;

	// retreive decoded data and play
	do {
		decoder.decode((void **)&audio, &done);
//log_citra("decoder.decode set done to %d", done);
		if (done < 0) return 0;
		if (done > 0) {
			if (!ndsp_rate) {
				decoder.info(&stream_type, &ndsp_rate, &ndsp_channels, &stream_bitrate);
				rfbClientLog("Audio stream: %s %dkbps, %dHz, %d channels",stream_type, stream_bitrate, ndsp_rate, ndsp_channels);
				sound_open(ndsp_rate, ndsp_channels);
			}
			sound_play(audio, done);
		}
	} while (done > 0);
	return size * nmemb;
}

#define HTTP_MAX_REDIRECTS 50
#define HTTP_TIMEOUT_SEC 15

int stream_check_health() {
	CURLMsg *msg=NULL;
	int msgs_left, return_code;
	long int http_status_code;

	if (still_running <= 0) {
		// fresh disconnect?
		if (mcurl) {
			// was there an error?
			do { // dump all messages
				msg = curl_multi_info_read(mcurl, &msgs_left);
				if (msg && msg->msg == CURLMSG_DONE) {
					CURL *eh = msg->easy_handle;
					return_code = msg->data.result;
					if (return_code == CURLE_HTTP_RETURNED_ERROR) {
						// Get HTTP status code
						curl_easy_getinfo(eh, CURLINFO_RESPONSE_CODE, &http_status_code);
						if(http_status_code!=200) {
							rfbClientErr("audio stream HTTP error: %d %s", http_status_code, HttpStatus_reasonPhrase(http_status_code));
						}
					} else if (return_code != CURLE_OK) {
						if (decoder.errstr && decoder.errstr()) {
							rfbClientErr("%s error: %s", stream_type, decoder.errstr());
						} else {
							rfbClientErr("audio stream error: %s", curl_easy_strerror(return_code));
						}
					}
				}
			} while (msgs_left);
			stop_stream();
		}
		return 1;
	}
	return 0;
}


int start_stream(char *url, char *username, char *password)
{
	static char sysversion[32]={0};

	if (sysversion[0] == 0)
		osGetSystemVersionDataString(NULL, NULL, sysversion, sizeof(sysversion));

	rfbClientLog("Starting stream %s", url);
	ndspInit();
	sound_close();

	curl = curl_easy_init();

	curl_easy_setopt(curl, CURLOPT_URL, url);
	curl_easy_setopt(curl, CURLOPT_ACCEPT_ENCODING, "");
	char buf[200];
	snprintf(buf,sizeof(buf),"curl/%s (Nintendo 3DS/%s) TinyVNC/%s",curl_version_info(CURLVERSION_NOW)->version, sysversion, VERSION);
	curl_easy_setopt(curl, CURLOPT_USERAGENT, buf);
	//curl_easy_setopt(curl, CURLOPT_BUFFERSIZE, 1024L);
	curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, (long) HTTP_TIMEOUT_SEC);
	curl_easy_setopt(curl, CURLOPT_MAXREDIRS, (long) HTTP_MAX_REDIRECTS);
	curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
	curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1L);
	curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, stream_write_callback);
	if (username) curl_easy_setopt(curl, CURLOPT_USERNAME, username);
	if (password) curl_easy_setopt(curl, CURLOPT_PASSWORD, password);

	mcurl = curl_multi_init();
	curl_multi_add_handle(mcurl, curl);
	curl_multi_perform(mcurl, &still_running);
	return stream_check_health();
}

void stop_stream()
{
	if (mcurl != NULL) {
		rfbClientLog("Audio stream stopped");
		// stop curl
		curl_multi_remove_handle(mcurl, curl);
		curl_easy_cleanup(curl);
		curl_multi_cleanup(mcurl);
		mcurl = curl = NULL;
		// stop ndsp
		sound_close();
		ndspExit();
		// stop decoder
		if (decoder.close) decoder.close();
		stream_type = (char*)(stream_bitrate = 0);
		bzero(&decoder,sizeof(decoder));
	}
}

int run_stream()
{
	if (curl_paused) {
		sound_clean_buffers();
		if (queuedSamples < max_wavbuf_size) {
			curl_paused = 0;
			curl_easy_pause(curl, CURLPAUSE_CONT);
		}
		return stream_check_health();
	}

	if (still_running > 0) {
		curl_multi_perform(mcurl, &still_running);
	}

	return stream_check_health();
}
