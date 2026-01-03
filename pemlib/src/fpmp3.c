/*
 * fpmp3.c - FullPlay Media Systems encoder interface
 *
 * Copyright (C) 1998-2025 Mark Phillips. All rights reserved.
 *
 * Original algorithm: Segher Boessenkool (1998-2002)
 * Embedded integration: Interactive Objects, Inc. (2001)
 * Modern platform adaptation: Mark Phillips (2025)
 *
 * This file provides the fpmp3_* API used by FullPlay portable media
 * players. It wraps the core pem_* functions for frame-by-frame encoding.
 *
 * See LICENSE for terms of use.
 */

#include "codec.h"
#include "memory.h"
#include <stdio.h>

long
fpmp3_start(int bitrate)
{
	return pem_init(bitrate);
}

long
fpmp3_start_ex(int bitrate, int sample_rate)
{
	return pem_init_ex(bitrate, sample_rate);
}

long
fpmp3_encode(short *pcm, unsigned char *mp3out, unsigned int *bytesout)
{
	int foo = 1152;
	long res;
	static int frame_count = 0;
	sram_p->outbuf = mp3out;
	sram_p->outcount = 0;
#ifdef PEM_DEBUG
	if (frame_count < 3) {
		fprintf(stderr, "fpmp3_encode frame %d: calling pem_work\n", frame_count);
	}
#endif
	res = pem_work(pcm,&foo,0);
#ifdef PEM_DEBUG
	if (frame_count < 3) {
		fprintf(stderr, "fpmp3_encode frame %d: pem_work returned %ld, outcount=%d\n",
			frame_count, res, sram_p->outcount);
	}
#endif
	frame_count++;
	*bytesout = sram_p->outcount;
	return res;
}

long
fpmp3_finish(unsigned char *mp3out, unsigned int *bytesout)
{
	sram_p->outbuf = mp3out;
	sram_p->outcount = 0;
	pem_fini();
	*bytesout = sram_p->outcount;
	return 0;
}

/* VBR support wrappers */

long
fpmp3_set_bitrate(int bitrate)
{
	return pem_set_bitrate(bitrate);
}

float
fpmp3_get_complexity(void)
{
	return pem_get_complexity();
}

int
fpmp3_get_frame_size(void)
{
	return pem_get_frame_size();
}
