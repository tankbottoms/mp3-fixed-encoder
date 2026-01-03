/*
 * psy_tuning.c - Psychoacoustic model tuning implementation
 *
 * Copyright (C) 1998-2025 Mark Phillips. All rights reserved.
 *
 * This file implements runtime tuning for the psychoacoustic model
 * and joint stereo processing.
 *
 * See LICENSE for terms of use.
 */

#include "psy_tuning.h"
#include <string.h>

/* Global tuning state */
static encoder_tuning_t g_tuning;
static int g_initialized = 0;

/*
 * Default tuning values
 */
static const encoder_tuning_t default_tuning = {
    .psy = {
        .mask_threshold_offset = 0x90000,  /* 9.0 in 16.16 fixed-point */
        .temporal_blend = 50,
        .ath_sensitivity = 0,
        .sf_multiplier = 50,
        .gain_offset = 0,
        .short_block_threshold = 0
    },
    .stereo = {
        .mode = STEREO_MODE_AUTO,
        .ms_threshold = 50,
        .hf_cutoff_band = 16,
        .adaptive_ms = 0,
        .stereo_width = 50
    }
};

/*
 * Quality preset: Maximum quality, larger files
 */
static const encoder_tuning_t quality_tuning = {
    .psy = {
        .mask_threshold_offset = 0xC0000,  /* 12.0 - less aggressive masking */
        .temporal_blend = 30,              /* Less smoothing */
        .ath_sensitivity = 20,             /* Some ATH application */
        .sf_multiplier = 40,               /* More bits to high frequencies */
        .gain_offset = -5,                 /* Higher quality quantization */
        .short_block_threshold = 0
    },
    .stereo = {
        .mode = STEREO_MODE_ADAPTIVE_MS,
        .ms_threshold = 30,                /* Keep more stereo information */
        .hf_cutoff_band = 0,               /* No high frequency cutoff */
        .adaptive_ms = 1,
        .stereo_width = 70                 /* Wider stereo image */
    }
};

/*
 * Speed preset: Faster encoding, smaller files
 */
static const encoder_tuning_t speed_tuning = {
    .psy = {
        .mask_threshold_offset = 0x70000,  /* 7.0 - more aggressive masking */
        .temporal_blend = 70,              /* More smoothing */
        .ath_sensitivity = 0,
        .sf_multiplier = 60,
        .gain_offset = 5,                  /* Lower quality quantization */
        .short_block_threshold = 0
    },
    .stereo = {
        .mode = STEREO_MODE_MS_ONLY,
        .ms_threshold = 70,                /* Zero more side channel */
        .hf_cutoff_band = 14,              /* Aggressive HF cutoff */
        .adaptive_ms = 0,
        .stereo_width = 30                 /* Narrower stereo */
    }
};

/*
 * Voice preset: Optimized for speech content
 */
static const encoder_tuning_t voice_tuning = {
    .psy = {
        .mask_threshold_offset = 0x80000,  /* 8.0 */
        .temporal_blend = 60,
        .ath_sensitivity = 40,             /* Speech doesn't need deep bass */
        .sf_multiplier = 70,               /* More bits to mid frequencies */
        .gain_offset = 0,
        .short_block_threshold = 0
    },
    .stereo = {
        .mode = STEREO_MODE_MS_ONLY,
        .ms_threshold = 80,                /* Narrow stereo is fine for voice */
        .hf_cutoff_band = 18,              /* Less HF needed for voice */
        .adaptive_ms = 0,
        .stereo_width = 20                 /* Near-mono is acceptable */
    }
};

/*
 * Music preset: Optimized for music content
 */
static const encoder_tuning_t music_tuning = {
    .psy = {
        .mask_threshold_offset = 0xA0000,  /* 10.0 - slightly less aggressive */
        .temporal_blend = 40,
        .ath_sensitivity = 10,
        .sf_multiplier = 45,               /* Balanced frequency allocation */
        .gain_offset = -2,
        .short_block_threshold = 0
    },
    .stereo = {
        .mode = STEREO_MODE_ADAPTIVE_MS,
        .ms_threshold = 40,
        .hf_cutoff_band = 8,               /* Keep more HF stereo */
        .adaptive_ms = 1,
        .stereo_width = 60
    }
};

/*
 * Bass preset: Enhanced bass response
 */
static const encoder_tuning_t bass_tuning = {
    .psy = {
        .mask_threshold_offset = 0x90000,
        .temporal_blend = 50,
        .ath_sensitivity = 0,              /* Don't mask bass */
        .sf_multiplier = 30,               /* More bits to low frequencies */
        .gain_offset = -3,
        .short_block_threshold = 0
    },
    .stereo = {
        .mode = STEREO_MODE_JOINT,
        .ms_threshold = 50,
        .hf_cutoff_band = 12,
        .adaptive_ms = 0,
        .stereo_width = 50
    }
};

/*
 * Transparent preset: Near-transparent quality
 */
static const encoder_tuning_t transparent_tuning = {
    .psy = {
        .mask_threshold_offset = 0x100000, /* 16.0 - minimal masking */
        .temporal_blend = 20,              /* Minimal smoothing */
        .ath_sensitivity = 30,
        .sf_multiplier = 35,
        .gain_offset = -10,                /* Highest quality */
        .short_block_threshold = 0
    },
    .stereo = {
        .mode = STEREO_MODE_ADAPTIVE_MS,
        .ms_threshold = 20,                /* Keep most stereo information */
        .hf_cutoff_band = 0,               /* No cutoff */
        .adaptive_ms = 1,
        .stereo_width = 80                 /* Wide stereo */
    }
};

void psy_tuning_init(encoder_tuning_t *tuning)
{
    if (tuning) {
        memcpy(tuning, &default_tuning, sizeof(encoder_tuning_t));
    }
    if (!g_initialized) {
        memcpy(&g_tuning, &default_tuning, sizeof(encoder_tuning_t));
        g_initialized = 1;
    }
}

void psy_tuning_apply_preset(encoder_tuning_t *tuning, tuning_preset_t preset)
{
    const encoder_tuning_t *src;

    switch (preset) {
        case TUNING_PRESET_QUALITY:
            src = &quality_tuning;
            break;
        case TUNING_PRESET_SPEED:
            src = &speed_tuning;
            break;
        case TUNING_PRESET_VOICE:
            src = &voice_tuning;
            break;
        case TUNING_PRESET_MUSIC:
            src = &music_tuning;
            break;
        case TUNING_PRESET_BASS:
            src = &bass_tuning;
            break;
        case TUNING_PRESET_TRANSPARENT:
            src = &transparent_tuning;
            break;
        case TUNING_PRESET_DEFAULT:
        case TUNING_PRESET_BALANCED:
        default:
            src = &default_tuning;
            break;
    }

    if (tuning) {
        memcpy(tuning, src, sizeof(encoder_tuning_t));
    }
    memcpy(&g_tuning, src, sizeof(encoder_tuning_t));
}

const encoder_tuning_t *psy_tuning_get(void)
{
    if (!g_initialized) {
        psy_tuning_init(NULL);
    }
    return &g_tuning;
}

void psy_tuning_set(const encoder_tuning_t *tuning)
{
    if (tuning) {
        memcpy(&g_tuning, tuning, sizeof(encoder_tuning_t));
        g_initialized = 1;
    }
}

/* Individual parameter setters */

void psy_set_mask_threshold(int32_t offset)
{
    if (!g_initialized) psy_tuning_init(NULL);
    /* Clamp to valid range */
    if (offset < 0x40000) offset = 0x40000;
    if (offset > 0x100000) offset = 0x100000;
    g_tuning.psy.mask_threshold_offset = offset;
}

void psy_set_temporal_blend(int blend)
{
    if (!g_initialized) psy_tuning_init(NULL);
    if (blend < 0) blend = 0;
    if (blend > 100) blend = 100;
    g_tuning.psy.temporal_blend = blend;
}

void psy_set_ath_sensitivity(int sensitivity)
{
    if (!g_initialized) psy_tuning_init(NULL);
    if (sensitivity < 0) sensitivity = 0;
    if (sensitivity > 100) sensitivity = 100;
    g_tuning.psy.ath_sensitivity = sensitivity;
}

void psy_set_gain_offset(int offset)
{
    if (!g_initialized) psy_tuning_init(NULL);
    if (offset < -20) offset = -20;
    if (offset > 20) offset = 20;
    g_tuning.psy.gain_offset = offset;
}

void stereo_set_mode(stereo_mode_t mode)
{
    if (!g_initialized) psy_tuning_init(NULL);
    g_tuning.stereo.mode = mode;
}

void stereo_set_ms_threshold(int threshold)
{
    if (!g_initialized) psy_tuning_init(NULL);
    if (threshold < 0) threshold = 0;
    if (threshold > 100) threshold = 100;
    g_tuning.stereo.ms_threshold = threshold;
}

void stereo_set_hf_cutoff(int band)
{
    if (!g_initialized) psy_tuning_init(NULL);
    if (band < 0) band = 0;
    if (band > 21) band = 21;
    g_tuning.stereo.hf_cutoff_band = band;
}

void stereo_set_width(int width)
{
    if (!g_initialized) psy_tuning_init(NULL);
    if (width < 0) width = 0;
    if (width > 100) width = 100;
    g_tuning.stereo.stereo_width = width;
}

const char *psy_tuning_preset_name(tuning_preset_t preset)
{
    switch (preset) {
        case TUNING_PRESET_DEFAULT:    return "default";
        case TUNING_PRESET_QUALITY:    return "quality";
        case TUNING_PRESET_BALANCED:   return "balanced";
        case TUNING_PRESET_SPEED:      return "speed";
        case TUNING_PRESET_VOICE:      return "voice";
        case TUNING_PRESET_MUSIC:      return "music";
        case TUNING_PRESET_BASS:       return "bass";
        case TUNING_PRESET_TRANSPARENT: return "transparent";
        default:                       return "unknown";
    }
}
