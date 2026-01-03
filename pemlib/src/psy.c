/*
 * psy.c - Simplified psychoacoustic model
 *
 * Copyright (C) 1998-2025 Mark Phillips. All rights reserved.
 *
 * Original algorithm: Segher Boessenkool (1998-2002)
 * Embedded integration: Interactive Objects, Inc. (2001)
 * Modern platform adaptation: Mark Phillips (2025)
 *
 * This file implements a simplified psychoacoustic model for MP3 encoding.
 * Unlike the full ISO 11172-3 model which requires FFT analysis, this
 * implementation uses energy-based band estimation for reduced complexity.
 *
 * The model calculates:
 *   - Energy per scalefactor band (eb)
 *   - Logarithmic energy representation (leb)
 *   - Spreading function application (lebs)
 *   - Masking thresholds (lthr)
 *   - Scalefactor allocation (sf, scalefac)
 *
 * Trade-off: Lower computational cost at the expense of some perceptual
 * precision. Acceptable for embedded applications and voice/music content.
 *
 * See LICENSE for terms of use.
 */

#ifdef PROF
#include "TinyProfile.h"
#endif

#include <stdio.h>
#include <math.h>

#define pow2(x) powf(2.0f, x)
#define PI M_PI

#include "codec.h"
#include "types.h"
#include "psy.h"
#include "memory.h"
#include "psy_tuning.h"

#define max(a, b) ((a) > (b) ? (a) : (b))

/*
 * Absolute Threshold of Hearing (ATH) table in dB SPL.
 * Approximation for 21 scalefactor bands at 44.1 kHz.
 * Values represent minimum audible levels for each band.
 */
static const int32_t ath_table[21] = {
    /* Bands 0-5: 0-1.7 kHz - lower frequencies need higher threshold */
    0x600000, 0x400000, 0x300000, 0x280000, 0x240000, 0x200000,
    /* Bands 6-10: 1.7-4 kHz - most sensitive hearing range */
    0x1C0000, 0x180000, 0x140000, 0x100000, 0x0E0000,
    /* Bands 11-15: 4-8 kHz - sensitivity decreasing */
    0x120000, 0x160000, 0x1A0000, 0x1E0000, 0x220000,
    /* Bands 16-20: 8-16 kHz - high frequency rolloff */
    0x280000, 0x300000, 0x400000, 0x500000, 0x600000
};


void start_scalefacs(struct granule_t *gi)
{
	int i;

	for (i = 0; i < 21; i++) {
		FASTVAR(scalefac)[i] = 0;
		FASTVAR(sf)[i] = 0;
	}
	FASTVAR(slen0) = FASTVAR(slen1) = 0;
	gi->scalefac_compress = 0;
	gi->scalefac_scale = 0;
	gi->part2_length = 0;
}

/* eb: energy band? tmp gets sum of squares of xr's within a
 scalefactor band. FASTVAR(heb)[ch][i] stores previous granule's sum of squares.
 (historical energy band). Current and previous sum of squares within
 a band are combined, then weighted by sbew. (Inverse fletcher-munson?)
 */
void calc_eb(int ch)
{
	int i, j;
	fixed64_32 sum;
	fixed64_32 tmp;

	for (i = 0; i < maxpb[ch]; i++) {
		tmp = 0;
		for (j = sbo[i]; j < sbo[i+1]; j++) {
			tmp += fm16_16_64_32(FASTVAR(xr)[j], FASTVAR(xr)[j]);
		}
		sum = FASTVAR(heb)[ch][i] + tmp;
		FASTVAR(heb)[ch][i] = tmp;
		FASTVAR(eb)[i] = fm16_64_32_64_32(sbew[i], sum);
	}
}


void calc_leb(int ch)
{
	int i;

	for (i = 0; i < 21-2*ch; i++) {  // maxpb[ch], not 21-2*ch
		long long x;
		int s;

		x = FASTVAR(eb)[i];
		s = 0;
		while (x >= 256) {
			x >>= 1;
			s += 1 << 16;
		}
		FASTVAR(leb)[i] = afac + fm16(mfac, s + l2[x]);
	}
}


void calc_lebs(int ch)
{
	int i;
	fixed16 l;
	int m = maxpb[ch];

//for (i = 0; i < m; i++) fprintf(stderr, "FASTVAR(leb)[%2d] = 0x%08x\n", i, FASTVAR(leb)[i]);

	FASTVAR(lebs)[0] = -1000*65536;
	l = FASTVAR(leb)[0] - 9 * (bark[1] - bark[0]) / 2;

	for (i = 1; i < m - 1; i++) {
		FASTVAR(lebs)[i] = l;

		if (FASTVAR(leb)[i] > l)
			l = FASTVAR(leb)[i];

		l -= 9 * (bark[i+1] - bark[i]) / 2          *2;
	}
	FASTVAR(lebs)[m-1] = l;

//for (i = 0; i < m; i++) fprintf(stderr, "FASTVAR(lebs)[%2d] = 0x%08x\n", i, FASTVAR(lebs)[i]);

	l = FASTVAR(leb)[m-1] - 9 * (bark[m-1] - bark[m-2]);

	for (i = m - 2; i > 0; i--) {
		if (l > FASTVAR(lebs)[i])
			FASTVAR(lebs)[i] = l;

		if (FASTVAR(leb)[i] > l)
			l = FASTVAR(leb)[i];

		l -= 9 * (bark[i] - bark[i-1])       *2;
	}
	FASTVAR(lebs)[0] = l;

//for (i = 0; i < m; i++) fprintf(stderr, "FASTVAR(lebs)[%2d] = 0x%08x\n", i, FASTVAR(lebs)[i]);
}


void calc_lthr(int ch)
{
	int i;
	fixed16 lt;
	const encoder_tuning_t *tuning = psy_tuning_get();
	int32_t mask_offset = tuning->psy.mask_threshold_offset;
	int ath_sens = tuning->psy.ath_sensitivity;

	for (i = 0; i < maxpb[ch]; i++) {
		lt = FASTVAR(lebs)[i] - mask_offset;

		/* Apply Absolute Threshold of Hearing if enabled */
		if (ath_sens > 0) {
			/* Scale ATH by sensitivity (0-100) */
			int32_t ath_scaled = (ath_table[i] * ath_sens) / 100;
			if (ath_scaled > lt)
				lt = ath_scaled;
		}

		FASTVAR(lthr)[i] = lt;
	}

}


void calc_sf(int ch)
{
	int i;
	const encoder_tuning_t *tuning = psy_tuning_get();

	/*
	 * sf_multiplier controls frequency allocation:
	 * - Lower values (0-40): More bits to high frequencies
	 * - Default (50): Balanced, uses multiplier 25000
	 * - Higher values (60-100): More bits to low frequencies
	 *
	 * Map 0-100 to range 15000-35000
	 */
	int sf_mult = tuning->psy.sf_multiplier;
	int32_t mult = 15000 + (sf_mult * 200);  /* 0->15000, 50->25000, 100->35000 */

	for (i = 0; i < maxpb[ch]; i++)
		FASTVAR(sf)[i] = fm16(mult, FASTVAR(lthr)[i] - 2 * FASTVAR(leb)[i]);
}


void norm_sf(struct granule_t *gi, int ch)
{
	int i;
	fixed16 max1, min1, max2, min2, max;
	fixed16 x;
	fixed16 sum = 0;

	gi->scalefac_scale = 0;
	max1 = max2 = ftof16(-1000.0f);
	min1 = min2 = ftof16(1000.0f);
	for (i = 0; i < 11; i++) {
		x = FASTVAR(sf)[i] - ftof16(30.0f);
		sum += FASTVAR(sf)[i];
		if (x > max1)
			max1 = x;
		if (FASTVAR(sf)[i] < min1)
			min1 = FASTVAR(sf)[i];
	}
	for (i = 11; i < maxpb[ch]; i++) {
		x = FASTVAR(sf)[i] - ftof16(14.0f);
		sum += FASTVAR(sf)[i];
		if (x > max2)
			max2 = x;
		if (FASTVAR(sf)[i] < min2)
			min2 = FASTVAR(sf)[i];
	}

	sum = fm16(sum, ftof16(1.0f/21.0f));
	max = sum;

	for (i = 0; i < maxpb[ch]; i++)
		FASTVAR(sf)[i] = (FASTVAR(sf)[i] - sum) / 2 + ftof16(1.2f);

//fprintf(stderr, "wanted gain: %12.6f\n", max);
{
	const encoder_tuning_t *tuning = psy_tuning_get();
	/* Apply user-configurable gain offset (-20 to +20) */
	gi->global_gain = max/65536 + 170 + tuning->psy.gain_offset;
}

//fprintf(stderr, "%3d ", gi->global_gain);
}


void calc_scalefacs(int ch, struct granule_t *gi)
{
	int i;
	

	calc_eb(ch);
	
	FASTVAR(eeb)[ch] = 0;
	for (i = 0; i < maxpb[ch]; i++)
		FASTVAR(eeb)[ch] += FASTVAR(eb)[i];
	
	
	calc_leb(ch);
	calc_lebs(ch);
	calc_lthr(ch);
	
	
	if (FASTVAR(mpeg).ms_stereo) {
		int i;
		const encoder_tuning_t *tuning = psy_tuning_get();
		stereo_mode_t mode = tuning->stereo.mode;
		int ms_thresh = tuning->stereo.ms_threshold;
		int hf_cutoff = tuning->stereo.hf_cutoff_band;
		int stereo_width = tuning->stereo.stereo_width;

		/* Convert threshold 0-100 to scaling factor */
		/* Lower threshold = keep more side channel (0 -> 50%, 100 -> 150%) */
		int32_t thresh_scale = 32768 + ((100 - ms_thresh) * 655);  /* 0.5 to 1.5 range */

		/* Stereo width affects how much we preserve side channel */
		/* width 0 = near-mono, width 100 = full stereo */
		int32_t width_scale = (stereo_width * 65536) / 100;

		if (!ch) {	// mid channel
			for (i = 0; i < 144; i++)
				FASTVAR(enm)[i] = max(FASTVAR(xr)[2*i], FASTVAR(xr)[2*i+1]);
		} else {	// side channel
			/* Calculate cutoff sample index from band number */
			int hf_cutoff_sample = (hf_cutoff == 0) ? 576 : sbo[hf_cutoff > 21 ? 21 : hf_cutoff];

			if (mode == STEREO_MODE_STEREO) {
				/* Simple stereo - don't zero anything */
				/* No modification to side channel */
			} else if (mode == STEREO_MODE_ADAPTIVE_MS && tuning->stereo.adaptive_ms) {
				/* Adaptive MS: make per-coefficient decisions with width control */
				for (i = 0; i < 144 && (2*i) < hf_cutoff_sample; i++) {
					fixed16 ens = max(FASTVAR(xr)[2*i], FASTVAR(xr)[2*i+1]);
					/* Apply width scaling to enfac threshold */
					int32_t scaled_enfac = fm16(enfac[i], thresh_scale);
					scaled_enfac = fm16(scaled_enfac, width_scale);
					if (ens < fm16(scaled_enfac, FASTVAR(enm)[i])) {
						FASTVAR(xr)[2*i] = FASTVAR(xr)[2*i+1] = 0;
					}
				}
				/* Zero high frequencies above cutoff */
				for (i = hf_cutoff_sample / 2; i < 288; i++)
					FASTVAR(xr)[2*i] = FASTVAR(xr)[2*i+1] = 0;
			} else {
				/* Standard MS stereo with tunable threshold */
				for (i = 0; i < 144; i++) {
					fixed16 ens = max(FASTVAR(xr)[2*i], FASTVAR(xr)[2*i+1]);
					/* Apply threshold scaling */
					int32_t scaled_enfac = fm16(enfac[i], thresh_scale);
					if (ens < fm16(scaled_enfac, FASTVAR(enm)[i]))
						FASTVAR(xr)[2*i] = FASTVAR(xr)[2*i+1] = 0;
				}
				/* Zero high frequencies above cutoff band */
				for (i = hf_cutoff_sample / 2; i < 288; i++)
					FASTVAR(xr)[2*i] = FASTVAR(xr)[2*i+1] = 0;
			}
		}
	}
	
	
	calc_sf(ch);
	norm_sf(gi, ch);
	{
		int i, j;
		for (i = 0; i < maxpb[ch]; i++) {
			int f = 1;
			for (j = sbo[i]; j < sbo[i+1]; j++)
				if (FASTVAR(xr)[j]) {
					f = 0;
					break;
				}
				if (f)
					FASTVAR(sf)[i] = -10000 * 65536;
		}
	}
}


void compute_scalefacs(int ch, struct granule_t *gi)
{
	int i;
	fixed16 mean;
	fixed16 o[21];
	fixed16 min;
	const encoder_tuning_t *tuning = psy_tuning_get();

	/*
	 * Temporal blend controls smoothing between frames:
	 * 0 = no smoothing (current frame only)
	 * 50 = equal blend (default, same as original)
	 * 100 = full smoothing (mostly previous frame)
	 */
	int blend = tuning->psy.temporal_blend;

	min = ftof16(9999);
	mean = 0;
	for (i = 0; i < maxpb[ch]; i++) {
		o[i] = FASTVAR(old_sf)[ch][i];
		FASTVAR(old_sf)[ch][i] = FASTVAR(sf)[i];

		/* Apply temporal blending based on tuning */
		if (blend == 0) {
			/* No smoothing - use current frame only */
			/* sf[i] stays as-is */
		} else if (blend == 100) {
			/* Full smoothing - mostly previous frame */
			FASTVAR(sf)[i] = (FASTVAR(sf)[i] + 3 * o[i]) / 4;
		} else {
			/* Blend: map 0-100 to weight for previous frame */
			/* blend=50 gives equal weight (original behavior) */
			int current_weight = 100 - blend;
			int prev_weight = blend;
			FASTVAR(sf)[i] = (FASTVAR(sf)[i] * current_weight + o[i] * prev_weight) / 100;
		}
	}


	for (i = 0; i < maxpb[ch]; i++) {
		if (FASTVAR(sf)[i] < min && FASTVAR(sf)[i] >= 0)
			min = FASTVAR(sf)[i];
		if (FASTVAR(sf)[i] >= 0)
			mean += FASTVAR(sf)[i];
	}

//	mean /= 90;
	mean = 0;

	for (i = 0; i < maxpb[ch]; i++) {
		FASTVAR(scalefac)[i] = (FASTVAR(sf)[i] - min - mean)/65536;
		if (FASTVAR(scalefac)[i] < 0)
			FASTVAR(scalefac)[i] = 0;
	}

//	/* try preemphasis */
//	gi->pre_emphasis = 1;
//	for (i = 0; i < 21; i++)
//		if (FASTVAR(scalefac)[i] < preemp[i])
//			gi->pre_emphasis = 0;
//	if (gi->pre_emphasis)
//		for (i = 0; i < 21; i++)
//			FASTVAR(scalefac)[i] -= preemp[i];

	for (i = 0; i < maxpb[ch]; i++) {
//		if (FASTVAR(scalefac)[i] < 0) FASTVAR(scalefac)[i] = 0;
		if (FASTVAR(scalefac)[i] > 15) FASTVAR(scalefac)[i] = 15;
///if (FASTVAR(scalefac)[i] > 7) FASTVAR(scalefac)[i] = 7;
		if (FASTVAR(scalefac)[i] > 7 && i >= 11) FASTVAR(scalefac)[i] = 7;
	}


//if (FASTVAR(scalefac)[0] < 4)
//	FASTVAR(scalefac)[0] = 4;


	// Compute best scalefac_compress
	FASTVAR(slen0) = FASTVAR(slen1) = 0;
	for (i = 0; i < 11; i++)
		if (FASTVAR(scalefac)[i] > FASTVAR(slen0))
			FASTVAR(slen0) = FASTVAR(scalefac)[i];
	for ( ; i < maxpb[ch]; i++)
		if (FASTVAR(scalefac)[i] > FASTVAR(slen1))
			FASTVAR(slen1) = FASTVAR(scalefac)[i];
	FASTVAR(slen0) = slen[FASTVAR(slen0)];
	FASTVAR(slen1) = slen[FASTVAR(slen1)];
	for (i = 0; i < 16; i++)
		if (sf0[i] >= FASTVAR(slen0) && sf1[i] >= FASTVAR(slen1))
			break;
	if (i > 15) 
		longjmp(error_jbuf, PEM_ILLEGAL_SCALEFAC_COMPRESS);
	FASTVAR(slen0) = sf0[i];
	FASTVAR(slen1) = sf1[i];
	gi->scalefac_compress = i;


	if (gi->block_type != 2)
		gi->part2_length = 11 * FASTVAR(slen0) + 10 * FASTVAR(slen1);
	else
		gi->part2_length = 18 * FASTVAR(slen0) + 18 * FASTVAR(slen1);
}


void apply_scalefacs(int ch, struct granule_t *gi)
{
	int i, j;

	int shift;

	if (gi->scalefac_scale) {
		for (i = 0; i < maxpb[ch]; i++) {
			shift = FASTVAR(scalefac)[i];
			for (j = sbo[i]; j < sbo[i+1]; j++)
				FASTVAR(xr)[j] = FASTVAR(xr)[j] << shift;
		}
	} else {
		for (i = 0; i < maxpb[ch]; i++) {
			shift = FASTVAR(scalefac)[i] >> 1;
			if (FASTVAR(scalefac)[i] & 1) {
				for (j = sbo[i]; j < sbo[i+1]; j++)
					FASTVAR(xr)[j] = fm16(FASTVAR(xr)[j] << shift, 0x16a09); // sqrt(2)
			} else {
				for (j = sbo[i]; j < sbo[i+1]; j++)
					FASTVAR(xr)[j] = FASTVAR(xr)[j] << shift;
			}
		}
	}
}

