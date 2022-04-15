/*
 * TinyVNC - A VNC client for Nintendo 3DS
 *
 * utilities.h - utility functions
 *
 * Copyright 2020 Sebastian Weber
 */
#ifndef _UTILITIES_H
#define _UTILITIES_H

/* 8-times unrolled loop */
#define DUFFS_LOOP(pixel_copy_increment, width)			\
{ int n = (width+7)/8;							\
	switch (width & 7) {						\
	case 0: do {	pixel_copy_increment;				\
	case 7:		pixel_copy_increment;				\
	case 6:		pixel_copy_increment;				\
	case 5:		pixel_copy_increment;				\
	case 4:		pixel_copy_increment;				\
	case 3:		pixel_copy_increment;				\
	case 2:		pixel_copy_increment;				\
	case 1:		pixel_copy_increment;				\
		} while ( --n > 0 );					\
	}								\
}

#define MIN(a,b) \
	({ __typeof__ (a) _a = (a); \
		__typeof__ (b) _b = (b); \
		_a < _b ? _a : _b; })

#define MAX(a,b) \
	({ __typeof__ (a) _a = (a); \
		__typeof__ (b) _b = (b); \
		_a > _b ? _a : _b; })

#define LIMIT(a,l1,l2) \
	({ __typeof__ (a) _a = (a); \
		__typeof__ (l1) _l1 = (l1); \
		__typeof__ (l2) _l2 = (l2); \
		_a < _l1 ? _l1 : (_a > _l2 ? _l2 : _a); })

extern u64 getmicrotime();
extern void printBits(size_t const size, void const * const ptr);
extern void hex_dump(char *data, int size, char *caption);
extern int fastscale(unsigned char *d, int dst_pitch, unsigned char *s, int src_width, int src_height, int src_pitch, int factor);

#endif // _UTILITIES_H
