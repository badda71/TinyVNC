#include <stdarg.h>
#include <3ds.h>
#include <stdio.h>
#include <malloc.h>
#include <string.h>
#include <curl/curl.h>
#include <mpg123.h>
#include "httpstatuscodes_c.h"
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

	// set up sound buffer (~1 second of sound)
	// This means, we will max buffer one second. If the server transmits data than we can play,
	// we will discard the buffer contents and start playing the newly transmitted data.
	// this means: 1. on a realtime stream, we will hava a maximum latency of 1 second,
	// if the latency becomes bigger, we will skip forward one second
	// 2. non-realtime streams are NOT playable because we do NOT choke the connection
	// if data comes in too fast.
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
	if (ndsp_waveBuf[ndsp_activeBuf].status != NDSP_WBUF_DONE) {
		ndspChnWaveBufClear(CHANNEL);
		for (int i=0; i<wavbufnr; ++i)
			ndsp_waveBuf[i].status = NDSP_WBUF_DONE;
	}
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

static size_t stream_write_callback(void *buffer, size_t size, size_t nmemb, void *userp) {
//log_citra("enter %s",__func__);
	int err;
	off_t frame_offset;
	unsigned char *audio;
	size_t done;
	int channels, encoding;
	long rate;

	if (mpg123_feed(mh, (const unsigned char *)buffer, size * nmemb))
		return 0;
	do {
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
			sound_play(audio, done);
			break;
		case MPG123_NEED_MORE:
			break;
		default: // we have an mpg error, stop the stream
			return 0;
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
//	curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
//	curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, stream_info_callback);

	mcurl = curl_multi_init();
	curl_multi_add_handle(mcurl, curl);
	curl_multi_perform(mcurl, &still_running);
	return 0;
}

void stop_stream()
{
	if (mcurl != NULL) {
		log_citra("Audio stream stopped");
		curl_multi_remove_handle(mcurl, curl);
		curl_easy_cleanup(curl);
		curl_multi_cleanup(mcurl);
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
	CURLMsg *msg=NULL;
	int msgs_left, return_code;
	long int http_status_code;

	if (still_running > 0) {
		curl_multi_perform(mcurl, &still_running);
	}

	// did our stream stop?
	if (still_running <= 0) {
		// fresh disconnect?
		if (mcurl) {
			// was there an error?
			do { // get only the last message
				msg = curl_multi_info_read(mcurl, &msgs_left);
			} while (msgs_left);
			if (msg && msg->msg == CURLMSG_DONE) {
				CURL *eh = msg->easy_handle;
				return_code = msg->data.result;
				if (return_code == CURLE_HTTP_RETURNED_ERROR) {
					// Get HTTP status code
					curl_easy_getinfo(eh, CURLINFO_RESPONSE_CODE, &http_status_code);
					if(http_status_code!=200) {
						log_citra("audio stream HTTP error: %d %s", http_status_code, HttpStatus_reasonPhrase(http_status_code));
					}
				} else if (return_code != CURLE_OK) {
					if (mpg123_errcode(mh)) {
						log_citra("mp3 error: %s", mpg123_strerror(mh));
					} else {
						log_citra("audio stream error: %s", curl_easy_strerror(return_code));
					}
				}
			}
			stop_stream();
		}
		return 1;
	}
	return 0;
}
