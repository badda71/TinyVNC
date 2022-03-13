#ifndef _DECODER_H
#define _DECODER_H

typedef struct {
	int (*init)();
	int (*feed)(void *data, int size);
	int (*decode)(void **outdata, int *size);
	int (*info)(char **type, int *rate, int *channels, int *bitrate);
	int (*close)();
	const char *(*errstr)();
	int (*checkmagic)(char *buffer);
} audioDecoder;

#endif /* _DECODER_H */

/* magic types
} magic2type[] = {
	{ "\xFF\xFB",	TYPE_MP3},
	{ "\xFF\xF3",	TYPE_MP3},
	{ "\xFF\xF2",	TYPE_MP3},
	{ "\xFF\xF1",	TYPE_ACC},
	{ "\xFF\xF9",	TYPE_ACC},
	{ "Oggs",		TYPE_OPUS},
	{ NULL,			0}
};*/
