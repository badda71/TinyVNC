/*
 * TinyVNC - A VNC client for Nintendo 3DS
 *
 * mp3decoder.c - functions for handling mp3 decoding
 *
 * Copyright 2020 Sebastian Weber
 */

#include <mpg123.h>
#include <string.h>
#include "decoder.h"
#include "mp3decoder.h"

static mpg123_handle *mp3_handle = NULL;
static int mp3_channels;
static long mp3_rate;
static int mp3_bitrate;
static int infoavailable = 0;

static int mp3_init()
{
	mpg123_init();
	mp3_handle = mpg123_new(NULL, NULL);
	mpg123_open_feed(mp3_handle);
	infoavailable = 0;
	return 0;
}

// returns 0 on success, non-zero on error
static int mp3_feed(void *data, int size)
{
	if (!mp3_handle) return -1;
	return mpg123_feed(mp3_handle, (const unsigned char *)data, size);
}

// returns the number of bytes decoded or a negative number on error
static int mp3_decode(void **outdata, int *outsize)
{
	if (!mp3_handle) return -1;
	size_t os;
	off_t frame_offset;
	int encoding;
	int err = mpg123_decode_frame(mp3_handle, &frame_offset, (unsigned char **)outdata, &os);
	*outsize = os;
	switch(err) {
	case MPG123_NEW_FORMAT:
		mpg123_getformat(mp3_handle, &mp3_rate, &mp3_channels, &encoding);
		// Ensure that this output format will not change (it might, when we allow it).
		mpg123_format_none(mp3_handle);
		mpg123_format(mp3_handle, mp3_rate, mp3_channels, MPG123_ENC_SIGNED_16);
		struct mpg123_frameinfo mi;
		mpg123_info(mp3_handle, &mi);
		mp3_bitrate=mi.bitrate;
		infoavailable = 1;
		return 0;
	case MPG123_OK:
		return *outsize;
	case MPG123_NEED_MORE:
		return 0;
	default: // we have an mpg error
		return -1;
	}
	return -1;
}

// return 0 on success, -1 on failure
static int mp3_info(char **type, int *rate, int *channels, int *bitrate)
{
	if (!infoavailable) return -1;
	if (type) *type = "mp3";
	if (rate) *rate = mp3_rate;
	if (channels) *channels = mp3_channels;
	if (bitrate) *bitrate = mp3_bitrate;
	return 0;
}


static int mp3_close()
{
	if (!mp3_handle) return -1;
	mpg123_close(mp3_handle);
	mpg123_delete(mp3_handle);
	mpg123_exit();
	mp3_handle = NULL;
	infoavailable = 0;
	return 0;
}

// returns the last error string
static const char *mp3_errstr()
{
	if (!mp3_handle) return NULL;
	return mpg123_strerror(mp3_handle);
}

// return 0 on success, -1 on failure
int mp3_checkmagic(char *m) {
	if (strlen(m) < 2 || m[0]!=0xff || (m[1]!=0xfb && m[1]!=0xf3 && m[1]!=0xf2))
		return -1;
	return 0;
}

// initializes an audioDecoder struct with mp3-Decoder methods
void mp3_create_decoder(audioDecoder *d) {
	d->init = mp3_init;
	d->feed = mp3_feed;
	d->decode = mp3_decode;
	d->info = mp3_info;
	d->close = mp3_close;
	d->errstr = mp3_errstr;
}
