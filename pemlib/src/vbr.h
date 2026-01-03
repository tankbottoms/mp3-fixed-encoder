/*
 * vbr.h - Variable Bitrate (VBR) and Average Bitrate (ABR) support
 *
 * Copyright (C) 1998-2025 Mark Phillips. All rights reserved.
 *
 * This file provides VBR encoding with LAME-style quality presets
 * and Xing/LAME header support for accurate seeking.
 *
 * VBR Quality Presets (-V 0 through -V 9):
 *   -V 0: Highest quality (~245 kbps avg)
 *   -V 2: High quality (~190 kbps avg) [recommended]
 *   -V 4: Good quality (~165 kbps avg)
 *   -V 6: Medium quality (~130 kbps avg)
 *   -V 9: Lowest quality (~65 kbps avg)
 *
 * See LICENSE for terms of use.
 */

#ifndef VBR_H_
#define VBR_H_

#include <stdint.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

/* VBR encoding modes */
typedef enum {
    VBR_MODE_OFF = 0,    /* CBR mode (constant bitrate) */
    VBR_MODE_VBR,        /* VBR mode (quality-based) */
    VBR_MODE_ABR         /* ABR mode (average bitrate target) */
} vbr_mode_t;

/* VBR quality preset (0-9, lower = higher quality) */
typedef struct {
    int quality;         /* Quality level 0-9 */
    int min_bitrate;     /* Minimum bitrate (kbps) */
    int max_bitrate;     /* Maximum bitrate (kbps) */
    int target_bitrate;  /* Target average bitrate (kbps) */
} vbr_preset_t;

/* VBR state for encoding session */
typedef struct {
    vbr_mode_t mode;
    int quality;              /* VBR quality 0-9 */
    int abr_target;           /* ABR target bitrate */
    int min_bitrate;          /* Minimum allowed bitrate */
    int max_bitrate;          /* Maximum allowed bitrate */

    /* Frame statistics for Xing header */
    uint32_t total_frames;
    uint32_t total_bytes;     /* Total MP3 data bytes (excluding headers) */
    uint64_t total_samples;   /* Total PCM samples encoded */

    /* Table of contents for seeking (100 entries) */
    uint8_t toc[100];
    int toc_index;
    uint32_t toc_bytes_per_entry;
    uint32_t toc_accumulated;

    /* Bitrate histogram (for info header) */
    uint32_t bitrate_hist[15];

    /* Running average for ABR mode */
    uint32_t abr_bits_used;
    uint32_t abr_frames;
} vbr_state_t;

/* Quality preset table */
extern const vbr_preset_t vbr_presets[10];

/*
 * Initialize VBR state for a new encoding session.
 */
void vbr_init(vbr_state_t *vbr);

/*
 * Configure VBR mode with quality preset.
 * quality: 0-9 (0 = highest quality, 9 = lowest)
 */
void vbr_set_quality(vbr_state_t *vbr, int quality);

/*
 * Configure ABR mode with target bitrate.
 * target_kbps: Target average bitrate in kbps
 */
void vbr_set_abr(vbr_state_t *vbr, int target_kbps);

/*
 * Set minimum/maximum bitrate constraints.
 */
void vbr_set_bitrate_range(vbr_state_t *vbr, int min_kbps, int max_kbps);

/*
 * Select optimal bitrate for current frame based on audio complexity.
 * complexity: Normalized complexity value (0.0 - 1.0)
 * Returns: Bitrate index (1-14) for this frame
 */
int vbr_select_bitrate(vbr_state_t *vbr, float complexity);

/*
 * Update VBR statistics after encoding a frame.
 * bitrate_index: The bitrate index used for this frame
 * frame_bytes: Number of bytes in the encoded frame
 */
void vbr_update_stats(vbr_state_t *vbr, int bitrate_index, int frame_bytes);

/*
 * Get the size of the Xing/LAME header in bytes.
 * This space must be reserved at the start of the file.
 */
size_t vbr_xing_header_size(void);

/*
 * Write the Xing/LAME header to a buffer.
 * buffer: Output buffer (must be at least vbr_xing_header_size() bytes)
 * vbr: VBR state with statistics
 * Returns: Number of bytes written
 */
size_t vbr_write_xing_header(uint8_t *buffer, const vbr_state_t *vbr);

/*
 * Write the Xing/LAME header to a file at the current position.
 * Returns 0 on success, -1 on error.
 */
int vbr_write_xing_to_file(FILE *fp, const vbr_state_t *vbr);

#ifdef __cplusplus
}
#endif

#endif /* VBR_H_ */
