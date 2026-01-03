/*
 * codec.c - Main encoder orchestration
 *
 * Copyright (C) 1998-2025 Mark Phillips. All rights reserved.
 *
 * Original algorithm: Segher Boessenkool (1998-2002)
 * Embedded integration: Interactive Objects, Inc. (2001)
 * Modern platform adaptation: Mark Phillips (2025)
 *
 * This file coordinates the MP3 encoding pipeline:
 *   1. Initialize encoder state and validate bitrate
 *   2. Process PCM frames through polyphase filter bank
 *   3. Apply MDCT and aliasing reduction butterfly
 *   4. Quantize and Huffman encode each granule
 *   5. Format bitstream with side information
 *
 * See LICENSE for terms of use.
 */

#include <stdio.h>
#include <string.h>

/* Profiling macros for performance analysis (disabled by default). */
#if defined(PROF)
#include <util/diag/TinyProfile.h>
#define DECLARE_PROFILE(x) static profile_data _profile_data_##x
#define START_PROFILE(x) profile_start(&_profile_data_##x);
#define END_PROFILE(x,args...) diag_printf(args, profile_end(&_profile_data_##x));
#else
#define DECLARE_PROFILE(x)
#define START_PROFILE(x)
#define END_PROFILE(x,y...)
#endif

/* Debug visualization of scalefactor allocation (disabled by default). */
/* #define SHOW_SCALEFACS */
#define SHOW_SCALEFACS_COLOR 1


#include "types.h"
#include "codec.h"
#include "polyphase.h"
#include "hybrid.h"
#include "code.h"
#include "in.h"
#include "out.h"
#include "huffman.h"
#include "reservoir.h"
#include "memory.h"
#include <setjmp.h>
/* Use standalone headers */
#include "eresult.h"
#include "codec_workspace.h"
#include "quant.h"


#ifdef SHOW_SCALEFACS
int ix_max(int, int);

static void print_mag(struct granule_t *gi, int ch)
{
	int i;

#ifdef SHOW_SCALEFACS_COLOR
{
	static int count = 0;
	static char *kl[16] = {
		"0;30;40", "0;34;40", "1;34;40", "1;34;44",
///		"1;31;41", "0;34;40", "1;34;40", "1;34;44",
////		"0;37;40", "0;34;40", "1;34;40", "1;34;44",
		"0;36;44", "1;36;44", "1;36;46", "1;37;46",
		"1;37;47", "1;37;47", "1;37;47", "1;37;47",
		"1;37;47", "1;37;47", "1;37;47", "1;37;47"
	};
	static char hex[16] = "0123456789abcdef";
	if (count == SHOW_SCALEFACS_COLOR) {
		if (ch)
			count = 0;
		fprintf(stderr, "|");
		for (i = 0; i < 21; i++)
			fprintf(stderr, "\e[%sm%c", kl[scalefac[i]], ix_max(sbo[i], sbo[i+1]) ? hex[scalefac[i]] : '-');
		fprintf(stderr, "\e[0m| %2d %4d %3d%c", gi->part2_length, gi->part3_length, gi->global_gain, ch ? '\n' : '\t');
	}
	if (ch)
		count++;
}
#else
	fprintf(stderr, "(%d)", ch);
	for (i = 0; i < 21; i++)
		fprintf(stderr, "%3d", scalefac[i]);
	fprintf(stderr, "  %3d  %2d %4d\n", gi->global_gain, gi->part2_length, gi->part3_length);
#endif
}
#endif


/* Calculate frame size for given bitrate and sample rate */
static int calc_frame_size(int bitrate, int sample_rate)
{
	/* MPEG1 Layer III frame size formula: 144 * bitrate / sample_rate */
	/* Result is in bytes, without padding byte */
	return (144 * bitrate * 1000) / sample_rate;
}

ERESULT pem_init(int bitrate)
{
	return pem_init_ex(bitrate, 44100);
}

ERESULT pem_init_ex(int bitrate, int sample_rate)
{
	int i;

	sram_p = FAST_DECODE_ADDR; // from codec_workspace.h

	memset(&dram, 0, sizeof(dram));		// automatically resets framecount, if used
	memset(sram_p, 0, sizeof(struct sram) - sizeof(struct ro_sram));
	memcpy(&sram_p->ro, &rom_ro_sram, sizeof(struct ro_sram));

	//diag_printf("sram at %08x\n", sram_p);
	//diag_printf("ro_sram at %08x\n", &sram_p->ro);
	//diag_printf("rom_ro_sram at %08x\n", &rom_ro_sram);
	//diag_printf("so sram %x\n", sizeof(struct sram));
	//diag_printf("so ro_sram %x\n", sizeof(struct ro_sram));

	for (i = 1; i < 15; i++)
		if (bitrate == bitrates[i])
			break;

	if (i == 15)
		return PEM_BAD_BITRATE;

	FASTVAR(mpeg).bitrate = bitrate;
	FASTVAR(mpeg).bitrate_index = i;

	/* Set sample rate and calculate index */
	/* MPEG1 sample rate indices: 0=44100, 1=48000, 2=32000 */
	FASTVAR(mpeg).sample_rate = sample_rate;
	if (sample_rate == 44100) {
		FASTVAR(mpeg).sample_rate_idx = 0;
	} else if (sample_rate == 48000) {
		FASTVAR(mpeg).sample_rate_idx = 1;
	} else if (sample_rate == 32000) {
		FASTVAR(mpeg).sample_rate_idx = 2;
	} else {
		return PEM_BAD_BITRATE; /* Reuse error code for invalid sample rate */
	}
	init_polyphase();
	init_hbits();
	init_code();
	reservoir_init();
	out_init();

	return PEM_NO_ERROR;
}

#ifdef PEM_DEMO_LIMITFRAMES
static void obnoxious_noise(short *pcm)
{
	int i,j;
	j=0x3fff;
	for(i=0;i<1152;i++) {
		*pcm++ = j;
		*pcm++ = j;
		if (i%128 == 127)
			j = -j;
	}
}
#endif

ERESULT pem_work(short *pcm, int *pcm_count, int e_o_s)
{
	int ch, gr;
	int mean_bits;
	int padding;
	ERESULT err;
	DECLARE_PROFILE(polyphase);
	DECLARE_PROFILE(mdct);
	DECLARE_PROFILE(butterfly);
	DECLARE_PROFILE(code);
	DECLARE_PROFILE(out);

	err = setjmp(error_jbuf);
	if (err) {
		return err;
	}

	if (*pcm_count < 1152 && !e_o_s)
		return PEM_NOT_ENOUGH_INPUT;  /* error: need more input */

	if (*pcm_count > 1152)
		*pcm_count = 1152;

	/* Calculate frame size based on sample rate */
	if (FASTVAR(mpeg).sample_rate == 44100) {
		FASTVAR(mpeg).frame_size = framesizes[FASTVAR(mpeg).bitrate_index];
	} else {
		FASTVAR(mpeg).frame_size = calc_frame_size(FASTVAR(mpeg).bitrate, FASTVAR(mpeg).sample_rate);
	}
		// or round it up, or divide it evenly

	FASTVAR(mpeg).ms_stereo = 1;

	mean_bits = 4 * FASTVAR(mpeg).frame_size - 144;

	reservoir_start(8 * FASTVAR(mpeg).frame_size);

	out_start();

#if defined(PEM_DEMO_LIMITFRAMES)
	if (dram.framecount++ > PEM_DEMO_LIMITFRAMES) {
//		longjmp(error_jbuf,PEM_DEMO_LIMIT_REACHED);
		obnoxious_noise(pcm);
	}
#endif

	for (gr = 0; gr < 2; gr++) {

		START_PROFILE(polyphase);
		polyphase(pcm+1152*gr, FASTVAR(dctbuf)[0][1-gr], FASTVAR(dctbuf)[1][1-gr]);
		END_PROFILE(polyphase,"polyphase:%d\n");

#ifdef PEM_DEBUG
{
	static int frame = 0;
	if (frame < 3) {
		int i;
		long max0 = 0, min0 = 0, max1 = 0, min1 = 0;
		int nz0 = 0, nz1 = 0;
		for (i = 0; i < 576; i++) {
			if (FASTVAR(dctbuf)[0][1-gr][i] > max0) max0 = FASTVAR(dctbuf)[0][1-gr][i];
			if (FASTVAR(dctbuf)[0][1-gr][i] < min0) min0 = FASTVAR(dctbuf)[0][1-gr][i];
			if (FASTVAR(dctbuf)[0][1-gr][i] != 0) nz0++;
			if (FASTVAR(dctbuf)[1][1-gr][i] > max1) max1 = FASTVAR(dctbuf)[1][1-gr][i];
			if (FASTVAR(dctbuf)[1][1-gr][i] < min1) min1 = FASTVAR(dctbuf)[1][1-gr][i];
			if (FASTVAR(dctbuf)[1][1-gr][i] != 0) nz1++;
		}
		fprintf(stderr, "Frame %d gr%d polyphase: dctbuf[0] max=%ld min=%ld nz=%d, dctbuf[1] max=%ld min=%ld nz=%d\n",
			frame, gr, max0, min0, nz0, max1, min1, nz1);
		/* Also check input PCM */
		short *p = pcm + 1152 * gr;
		int pcm_nz = 0;
		short pcm_max = 0, pcm_min = 0;
		for (i = 0; i < 1152; i++) {
			if (p[i*2] > pcm_max) pcm_max = p[i*2];
			if (p[i*2] < pcm_min) pcm_min = p[i*2];
			if (p[i*2] != 0 || p[i*2+1] != 0) pcm_nz++;
		}
		fprintf(stderr, "  PCM[%d]: max=%d min=%d nonzero=%d\n", gr, pcm_max, pcm_min, pcm_nz);
	}
	if (gr == 1) frame++;
}
#endif


		for (ch = 0; ch < 2; ch++) {

			START_PROFILE(mdct);
			mdct(FASTVAR(dctbuf)[ch][gr], FASTVAR(dctbuf)[ch][1-gr], FASTVAR(xr), 32 - 16 * ch);
			END_PROFILE(mdct,"mdct%d:%d\n",ch);

#ifdef PEM_DEBUG
{
	static int frame = 0;
	if (frame < 3) {
		int i;
		long max = 0, min = 0;
		int nonzero = 0;
		for (i = 0; i < 576; i++) {
			if (FASTVAR(xr)[i] > max) max = FASTVAR(xr)[i];
			if (FASTVAR(xr)[i] < min) min = FASTVAR(xr)[i];
			if (FASTVAR(xr)[i] != 0) nonzero++;
		}
		fprintf(stderr, "  After mdct gr%d ch%d: xr max=%ld min=%ld nonzero=%d\n", gr, ch, max, min, nonzero);
	}
}
#endif

			START_PROFILE(butterfly);
			butterfly(ch ? 16 : 31);
			END_PROFILE(butterfly,"butterfly%d:%d\n",ch);

#ifdef PEM_DEBUG
{
	static int frame = 0;
	int i;
	long max = 0, min = 0;
	int nonzero = 0;
	for (i = 0; i < 576; i++) {
		if (FASTVAR(xr)[i] > max) max = FASTVAR(xr)[i];
		if (FASTVAR(xr)[i] < min) min = FASTVAR(xr)[i];
		if (FASTVAR(xr)[i] != 0) nonzero++;
	}
	if (frame < 3) fprintf(stderr, "  After butterfly gr%d ch%d: xr max=%ld min=%ld nonzero=%d\n", gr, ch, max, min, nonzero);
	if (gr == 1 && ch == 1) frame++;
}
#endif

			START_PROFILE(code);
			code(ch, &FASTVAR(granule)[gr][ch], mean_bits);
			END_PROFILE(code,"code%d:%d\n",ch);

#ifdef PEM_DEBUG
{
	static int frame = 0;
	struct granule_t *gi = &FASTVAR(granule)[gr][ch];
	if (frame < 3) {
		fprintf(stderr, "  -> part2=%d part3=%d big_values=%d gain=%d pos=[%d,%d,%d,%d]\n",
			gi->part2_length, gi->part3_length, gi->pos2 >> 1, gi->global_gain,
			gi->pos0, gi->pos1, gi->pos2, gi->pos3);
	}
	if (gr == 1 && ch == 1) frame++;
}
#endif

#ifdef SHOW_SCALEFACS
print_mag(&FASTVAR(granule)[gr][ch], ch);
#endif

			START_PROFILE(out);
			out(&FASTVAR(granule)[gr][ch]);
			END_PROFILE(out,"out%d:%d\n",ch);
		}
	}
	padding = reservoir_end();
	START_PROFILE(out);
	out_done(padding);
	END_PROFILE(out,"out_done:%d\n");

	return PEM_NO_ERROR;
}
ERESULT pem_save_state(void* buf, int len)
{
    if( len < (sizeof(dram)+sizeof(struct sram)) ) {
        return PEM_INSUFFICIENT_SPACE;
    }
    memcpy( buf, &dram, sizeof(dram) );
    buf += sizeof(dram);
    // this copies the ro section too - oh well
    memcpy( buf, sram_p, sizeof(struct sram) );
    
    return PEM_NO_ERROR;
}

ERESULT pem_restore_state(const void* buf)
{
    memcpy( &dram, buf, sizeof(dram) );
    buf += sizeof(dram);
    memcpy( sram_p, buf, sizeof(struct sram) );
    
    return PEM_NO_ERROR;
}

int pem_get_state_size(void)
{
    return (sizeof(dram) + sizeof(struct sram) );
}

void pem_fini(void)
{
	out_fini();
}

/* VBR support functions */

ERESULT pem_set_bitrate(int bitrate)
{
	int i;

	for (i = 1; i < 15; i++)
		if (bitrate == bitrates[i])
			break;

	if (i == 15)
		return PEM_BAD_BITRATE;

	FASTVAR(mpeg).bitrate = bitrate;
	FASTVAR(mpeg).bitrate_index = i;

	return PEM_NO_ERROR;
}

float pem_get_complexity(void)
{
	/*
	 * Calculate audio complexity from the psychoacoustic model's
	 * energy estimates. Use the sum of energy in both channels
	 * normalized against a reference level.
	 *
	 * eeb[ch] contains the sum of energy across all bands for each channel.
	 * A typical complex signal (loud music) might have eeb values around
	 * 0x10000000 (268M), while silence is near 0.
	 */
	long long total_energy;
	float complexity;

	/* Sum energy from both channels */
	total_energy = FASTVAR(eeb)[0] + FASTVAR(eeb)[1];

	/*
	 * Normalize to 0.0 - 1.0 range.
	 * Using logarithmic scale since energy varies over many orders of magnitude.
	 * Reference: 0x100 (256) = 0.0 (near silence)
	 *            0x10000000 (268M) = 1.0 (full complexity)
	 */
	if (total_energy < 256) {
		complexity = 0.0f;
	} else {
		/* log2 scale: map ~8 to ~28 bits of energy to 0.0-1.0 */
		float log_energy = 0.0f;
		long long temp = total_energy;
		while (temp > 1) {
			temp >>= 1;
			log_energy += 1.0f;
		}
		/* Map log2(256)=8 to 0.0, log2(268M)=28 to 1.0 */
		complexity = (log_energy - 8.0f) / 20.0f;
		if (complexity < 0.0f) complexity = 0.0f;
		if (complexity > 1.0f) complexity = 1.0f;
	}

	return complexity;
}

int pem_get_frame_size(void)
{
	return FASTVAR(mpeg).frame_size;
}

