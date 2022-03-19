#include <3ds.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include "utilities.h"

int fastscale(unsigned char *dst, int dst_pitch, unsigned char *src, int src_width, int src_height, int src_pitch, int factor)
{
	if (factor < 2) return -1;

	int temp_r, temp_g, temp_b;
	int i1,i2;

	int dst_width = src_width / factor;
	int dst_height = src_height / factor;
	if (!dst_height || !dst_width) return -1;
	int factor_pow2 = factor * factor;
	int factor_mul4 = factor << 2;
	int src_skip1 = src_pitch - factor_mul4;
	int src_skip2 = factor_mul4 - factor * src_pitch;
	int src_skip3 = src_pitch * factor - dst_width * factor_mul4;
	int dst_skip = dst_pitch - (dst_width << 2);

	for (i1 = 0; i1 < dst_height; ++i1)
	{
		for (i2 = 0; i2 < dst_width; ++i2)
		{
			temp_r = temp_g = temp_b = 0;
			DUFFS_LOOP ({
				DUFFS_LOOP ({
					src++; // alpha
					temp_r += *(src++);
					temp_g += *(src++);
					temp_b += *(src++);
				}, factor);
				src += src_skip1;
			}, factor);
			*(dst++) = 255; // alpha
			*(dst++) = temp_r / factor_pow2;
			*(dst++) = temp_g / factor_pow2;
			*(dst++) = temp_b / factor_pow2;
			src += src_skip2;
		}
		dst += dst_skip;
		src += src_skip3;
	}
	return 0;
}

u64 getmicrotime() {
    struct timeval tv;
    gettimeofday(&tv,NULL);
    return tv.tv_sec*(u64)1000000+tv.tv_usec;
}

void printBits(size_t const size, void const * const ptr)
{
    unsigned char *b = (unsigned char*) ptr;
    unsigned char byte;
    int i, j;

    for (i = size-1; i >= 0; i--) {
        for (j = 7; j >= 0; j--) {
            byte = (b[i] >> j) & 1;
            printf("%u", byte);
        }
    }
}

void hex_dump(char *data, int size, char *caption)
{
	int i; // index in data...
	int j; // index in line...
	char temp[16];
	char buffer[128];
	char *ascii;

	memset(buffer, 0, 128);

	log_citra("---------> %s <--------- (%d bytes from %p)", caption, size, data);
	// Printing the ruler...
	//log_citra("        +0          +4          +8          +c            0   4   8   c   ");

	// Hex portion of the line is 8 (the padding) + 3 * 16 = 52 chars long
	// We add another four bytes padding and place the ASCII version...
	ascii = buffer + 58;
	memset(buffer, ' ', 58 + 16);
	buffer[58 + 16] = '\0';
	buffer[0] = '+';
	buffer[1] = '0';
	buffer[2] = '0';
	buffer[3] = '0';
	buffer[4] = '0';
	for (i = 0, j = 0; i < size; i++, j++)
	{
		if (j == 16)
		{
			log_citra("%s", buffer);
			memset(buffer, ' ', 58 + 16);

			sprintf(temp, "+%04x", i);
			memcpy(buffer, temp, 5);

			j = 0;
		}

		sprintf(temp, "%02x", 0xff & data[i]);
		memcpy(buffer + 8 + (j * 3), temp, 2);
		if ((data[i] > 31) && (data[i] < 127))
			ascii[j] = data[i];
		else
			ascii[j] = '.';
	}

	if (j != 0)
		log_citra("%s", buffer);
}
