/*
 * psy_tuning.h - Psychoacoustic model tuning interface
 *
 * Copyright (C) 1998-2025 Mark Phillips. All rights reserved.
 *
 * This file provides runtime tuning parameters for the psychoacoustic
 * model and joint stereo processing. These parameters allow trading off
 * between audio quality, file size, and encoding complexity.
 *
 * See LICENSE for terms of use.
 */

#ifndef __PSY_TUNING_H__
#define __PSY_TUNING_H__

#include <stdint.h>

/*
 * Stereo mode selection
 */
typedef enum {
    STEREO_MODE_AUTO = 0,       /* Automatic selection (default) */
    STEREO_MODE_STEREO,         /* Force simple L/R stereo */
    STEREO_MODE_JOINT,          /* Force joint stereo (MS only) */
    STEREO_MODE_MS_ONLY,        /* Mid-side only, no switching */
    STEREO_MODE_ADAPTIVE_MS     /* Adaptive MS with per-band decisions */
} stereo_mode_t;

/*
 * Psychoacoustic model tuning parameters
 */
typedef struct {
    /*
     * Masking threshold offset (fixed-point, 16.16 format)
     * Default: 0x90000 (9.0 in fixed-point)
     * Lower = more aggressive masking (smaller files, more artifacts)
     * Higher = less aggressive masking (larger files, better quality)
     * Range: 0x40000 (4.0) to 0x100000 (16.0)
     */
    int32_t mask_threshold_offset;

    /*
     * Temporal masking blend factor (0-100)
     * Controls how much the previous frame's scalefactors influence current.
     * Default: 50 (equal blend)
     * 0 = no temporal smoothing (more responsive, may cause pumping)
     * 100 = full smoothing (smoother but less responsive)
     */
    int temporal_blend;

    /*
     * ATH (Absolute Threshold of Hearing) sensitivity (0-100)
     * Controls application of the absolute hearing threshold.
     * Default: 0 (disabled - matches original behavior)
     * Higher values apply ATH more aggressively at low frequencies.
     */
    int ath_sensitivity;

    /*
     * Scalefactor calculation multiplier (0-100)
     * Controls the noise allocation across frequency bands.
     * Default: 50 (balanced)
     * Lower = more bits to high frequencies
     * Higher = more bits to low frequencies
     */
    int sf_multiplier;

    /*
     * Global gain offset adjustment (-20 to +20)
     * Fine-tunes the overall quantization level.
     * Default: 0
     * Negative = higher quality (larger files)
     * Positive = lower quality (smaller files)
     */
    int gain_offset;

    /*
     * Short block threshold (not implemented in this encoder)
     * Reserved for future use with block switching.
     */
    int short_block_threshold;

} psy_tuning_t;

/*
 * Joint stereo tuning parameters
 */
typedef struct {
    /*
     * Stereo mode selection
     * Default: STEREO_MODE_AUTO
     */
    stereo_mode_t mode;

    /*
     * MS stereo threshold (0-100)
     * Controls when to zero side channel coefficients.
     * Default: 50
     * Lower = keep more side channel (wider stereo image)
     * Higher = zero more side channel (narrower image, smaller files)
     */
    int ms_threshold;

    /*
     * High frequency side channel cutoff band (0-21)
     * Bands above this are zeroed in the side channel.
     * Default: 16 (approximately 8 kHz at 44.1 kHz)
     * 0 = no cutoff (keep all frequencies)
     * 21 = aggressive cutoff (zero most high frequencies)
     */
    int hf_cutoff_band;

    /*
     * Adaptive MS threshold per-band enable
     * When enabled, MS stereo decisions are made per scalefactor band.
     * Default: 0 (disabled)
     */
    int adaptive_ms;

    /*
     * Stereo width control (0-100)
     * 0 = narrow (more mono-like)
     * 50 = normal (default)
     * 100 = wide (preserve stereo differences)
     */
    int stereo_width;

} stereo_tuning_t;

/*
 * Combined tuning configuration
 */
typedef struct {
    psy_tuning_t psy;
    stereo_tuning_t stereo;
} encoder_tuning_t;

/*
 * Preset quality levels for easy configuration
 */
typedef enum {
    TUNING_PRESET_DEFAULT = 0,  /* Balanced quality/size */
    TUNING_PRESET_QUALITY,      /* Maximum quality */
    TUNING_PRESET_BALANCED,     /* Balanced (same as default) */
    TUNING_PRESET_SPEED,        /* Faster encoding, lower quality */
    TUNING_PRESET_VOICE,        /* Optimized for speech */
    TUNING_PRESET_MUSIC,        /* Optimized for music */
    TUNING_PRESET_BASS,         /* Enhanced bass response */
    TUNING_PRESET_TRANSPARENT   /* Near-transparent quality */
} tuning_preset_t;

/* Initialize tuning with default values */
void psy_tuning_init(encoder_tuning_t *tuning);

/* Apply a preset configuration */
void psy_tuning_apply_preset(encoder_tuning_t *tuning, tuning_preset_t preset);

/* Get current tuning (for encoder core) */
const encoder_tuning_t *psy_tuning_get(void);

/* Set tuning (called from main/CLI) */
void psy_tuning_set(const encoder_tuning_t *tuning);

/* Individual parameter setters */
void psy_set_mask_threshold(int32_t offset);
void psy_set_temporal_blend(int blend);
void psy_set_ath_sensitivity(int sensitivity);
void psy_set_gain_offset(int offset);

void stereo_set_mode(stereo_mode_t mode);
void stereo_set_ms_threshold(int threshold);
void stereo_set_hf_cutoff(int band);
void stereo_set_width(int width);

/* Get preset name for display */
const char *psy_tuning_preset_name(tuning_preset_t preset);

#endif /* __PSY_TUNING_H__ */
