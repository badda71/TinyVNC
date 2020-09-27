#include <stdarg.h>
#include <3ds.h>
#include <stdio.h>
#include <malloc.h>
#include <string.h>
#include <curl/curl.h>
#include <mpg123.h>
#include "streamclient.h"

#define BITS 8

/* Channel to play music on */
#define CHANNEL	0x08
//#define WAVBUFNR 64

static mpg123_handle	*mh = NULL;
static ndspWaveBuf		*ndsp_waveBuf;
static int				ndsp_activeBuf = 0;
//static int				choked = 0;
static CURL				*curl = NULL;
static CURLM			*mcurl = NULL;
static int				ndsp_channels = 0;
static int				wavbufnr = 0;
static int				still_running = 0;

static void sound_free() {
	ndspChnReset(CHANNEL);
	ndspChnWaveBufClear(CHANNEL);
	if (ndsp_waveBuf) {
		for (int i=0; i<wavbufnr; ++i) {
			if (ndsp_waveBuf[i].data_vaddr) linearFree((void*)ndsp_waveBuf[i].data_vaddr);
			ndsp_waveBuf[i].data_vaddr=NULL;
			ndsp_waveBuf[i].status = NDSP_WBUF_DONE;
		}
		free(ndsp_waveBuf);
		ndsp_waveBuf = NULL;
		wavbufnr = 0;
	}
	ndsp_activeBuf = 0;
}

static void sound_open(long rate, int channels, int encoding, int bufsize)
{
	struct mpg123_frameinfo mi;
	mpg123_info(mh, &mi);
	log_citra("Audio stream: mp3 %dkbps, %dHz, %d channels",mi.bitrate, rate, channels);
	sound_free();	
	ndspSetOutputMode(channels == MPG123_STEREO ? NDSP_OUTPUT_STEREO : NDSP_OUTPUT_MONO);
	ndspChnSetInterp(CHANNEL, NDSP_INTERP_POLYPHASE);
	ndspChnSetRate(CHANNEL, (float)rate);
	ndspChnSetFormat(CHANNEL,
		channels == MPG123_STEREO ? NDSP_FORMAT_STEREO_PCM16 : NDSP_FORMAT_MONO_PCM16);
	ndsp_channels = MPG123_STEREO ? 2 : 1;

	// set up sound buffer (~1 seconds of sound)
	wavbufnr = (1 * rate * channels * 2) / bufsize;
	ndsp_waveBuf = calloc(wavbufnr, sizeof(ndspWaveBuf));
	for (int i=0;i < wavbufnr ;i++ )
	{
		ndsp_waveBuf[i].status = NDSP_WBUF_DONE;
		ndsp_waveBuf[i].data_vaddr = linearAlloc(bufsize);
	}
}

static int sound_play(unsigned char *pbuf, size_t size)
{
//log_citra("enter %s",__func__);
	// set and queue a new buffer
	memcpy((void *)ndsp_waveBuf[ndsp_activeBuf].data_vaddr, pbuf, size);
	ndsp_waveBuf[ndsp_activeBuf].nsamples = size / (ndsp_channels * sizeof(int16_t));
	ndspChnWaveBufAdd(CHANNEL, &ndsp_waveBuf[ndsp_activeBuf]);
	ndsp_activeBuf = (ndsp_activeBuf + 1) % wavbufnr;
	return 0;
}

void sound_close()
{
	sound_free();
}

static int stream_info_callback(void *clientp, curl_off_t dltotal, curl_off_t dlnow, curl_off_t ultotal, curl_off_t ulnow) {
	// check if connection is choked and we can unchoke again

/*	if (choked &&
		ndsp_waveBuf[ndsp_activeBuf].status == NDSP_WBUF_DONE) {
		choked=0;
		curl_easy_pause(curl, CURLPAUSE_CONT);
	}*/
	return 0;
}

static size_t stream_write_callback(void *buffer, size_t size, size_t nmemb, void *userp) {
//log_citra("enter %s",__func__);
	int err;
	off_t frame_offset;
	unsigned char *audio;
	size_t done;
	int channels, encoding;
	long rate;

/*
	if (choked || ndsp_waveBuf[ndsp_activeBuf].status != NDSP_WBUF_DONE) {
		choked = 1;
		return CURL_WRITEFUNC_PAUSE;
	}
*/
	mpg123_feed(mh, (const unsigned char *)buffer, size * nmemb);
	do {
/*
		if (ndsp_waveBuf[ndsp_activeBuf].status != NDSP_WBUF_DONE) {
			choked = 1;
			return CURL_WRITEFUNC_PAUSE;
		}
*/
		err = mpg123_decode_frame(mh, &frame_offset, &audio, &done);
		switch(err) {
		case MPG123_NEW_FORMAT:
			mpg123_getformat(mh, &rate, &channels, &encoding);
			// Ensure that this output format will not change (it might, when we allow it).
			mpg123_format_none(mh);
			mpg123_format(mh, rate, channels, encoding);
			// open sound output
			sound_open(rate, channels, encoding, mpg123_outblock(mh));
			break;
		case MPG123_OK:
			if (sound_play(audio, done) == CURL_WRITEFUNC_PAUSE)
				return CURL_WRITEFUNC_PAUSE;
			break;
		case MPG123_NEED_MORE:
			break;
		default:
			break;
		}
	} while (done > 0);
	return size * nmemb;
}

#define HTTP_MAX_REDIRECTS 50
#define HTTP_TIMEOUT_SEC 15
#define HTTP_TIMEOUT_NS ((u64) HTTP_TIMEOUT_SEC * 1000000000)

int start_stream(char *url)
{
	static char sysversion[32]={0};

	if (sysversion[0] == 0)
		osGetSystemVersionDataString(NULL, NULL, sysversion, sizeof(sysversion));

	log_citra("Starting stream %s", url);
	ndspInit();
	sound_free();

	mpg123_init();
	mh = mpg123_new(NULL, NULL);
	mpg123_open_feed(mh);

	curl = curl_easy_init();

	curl_easy_setopt(curl, CURLOPT_URL, url);
	curl_easy_setopt(curl, CURLOPT_ACCEPT_ENCODING, "");
	char buf[200];
	snprintf(buf,sizeof(buf),"curl/%s (Nintendo 3DS/%s) TinyVNC/%s",curl_version_info(CURLVERSION_NOW)->version, sysversion, VERSION);
	curl_easy_setopt(curl, CURLOPT_USERAGENT, buf);
	curl_easy_setopt(curl, CURLOPT_BUFFERSIZE, 1024L);
	curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, (long) HTTP_TIMEOUT_SEC);
	curl_easy_setopt(curl, CURLOPT_MAXREDIRS, (long) HTTP_MAX_REDIRECTS);
	curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
	curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1L);
	curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, stream_write_callback);
	curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
	curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, stream_info_callback);

	mcurl = curl_multi_init();
	curl_multi_add_handle(mcurl, curl);
	curl_multi_perform(mcurl, &still_running);
	return 0;
}

void stop_stream()
{
	if (mcurl != NULL) {
		log_citra("Audio stream stopped");
		curl_multi_cleanup(mcurl);
		curl_easy_cleanup(curl);
		mpg123_close(mh);
		mpg123_delete(mh);
		mpg123_exit();
		sound_close();
		ndspExit();
		mh = mcurl = curl = NULL;
	}
}

int run_stream()
{
	if (still_running > 0) {
		curl_multi_perform(mcurl, &still_running);
	}

	if (still_running <= 0 && mcurl != NULL) {
		stop_stream();
		return 1;
	}
	return 0;
}
