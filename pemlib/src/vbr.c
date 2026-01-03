/*
 * vbr.c - Variable Bitrate (VBR) and Average Bitrate (ABR) implementation
 *
 * Copyright (C) 1998-2025 Mark Phillips. All rights reserved.
 *
 * This file implements VBR encoding with LAME-style quality presets
 * and Xing header support for accurate duration/seeking in players.
 *
 * Xing Header Format (placed in first frame's ancillary data):
 *   "Xing" or "Info" (4 bytes) - VBR or CBR identifier
 *   Flags (4 bytes) - Which fields are present
 *   Frames (4 bytes) - Total number of frames
 *   Bytes (4 bytes) - Total MP3 data bytes
 *   TOC (100 bytes) - Seek table (percentage -> byte offset)
 *   Quality (4 bytes) - VBR quality indicator
 *
 * See LICENSE for terms of use.
 */

#include <string.h>
#include <stdlib.h>
#include "vbr.h"

/* Valid MP3 bitrates for Layer III @ 44.1kHz (from ro.h) */
extern const int bitrates[15];
extern const int framesizes[15];

/* LAME-style VBR quality presets */
const vbr_preset_t vbr_presets[10] = {
    /* V0 - Highest quality */
    { 0, 128, 320, 245 },
    /* V1 */
    { 1, 128, 320, 225 },
    /* V2 - Recommended */
    { 2, 128, 256, 190 },
    /* V3 */
    { 3, 112, 256, 175 },
    /* V4 */
    { 4, 96, 224, 165 },
    /* V5 */
    { 5, 80, 192, 130 },
    /* V6 */
    { 6, 64, 160, 115 },
    /* V7 */
    { 7, 56, 128, 100 },
    /* V8 */
    { 8, 48, 112, 85 },
    /* V9 - Lowest quality */
    { 9, 32, 96, 65 }
};

/* Valid MP3 bitrates for Layer III @ 44.1kHz */
static const int valid_bitrates[] = {
    0, 32, 40, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320
};

void vbr_init(vbr_state_t *vbr)
{
    memset(vbr, 0, sizeof(vbr_state_t));
    vbr->mode = VBR_MODE_OFF;
    vbr->quality = 4;  /* Default to V4 */
    vbr->min_bitrate = 32;
    vbr->max_bitrate = 320;
}

void vbr_set_quality(vbr_state_t *vbr, int quality)
{
    if (quality < 0) quality = 0;
    if (quality > 9) quality = 9;

    vbr->mode = VBR_MODE_VBR;
    vbr->quality = quality;
    vbr->min_bitrate = vbr_presets[quality].min_bitrate;
    vbr->max_bitrate = vbr_presets[quality].max_bitrate;
}

void vbr_set_abr(vbr_state_t *vbr, int target_kbps)
{
    vbr->mode = VBR_MODE_ABR;
    vbr->abr_target = target_kbps;

    /* Set reasonable min/max based on target */
    vbr->min_bitrate = target_kbps / 2;
    if (vbr->min_bitrate < 32) vbr->min_bitrate = 32;

    vbr->max_bitrate = target_kbps * 3 / 2;
    if (vbr->max_bitrate > 320) vbr->max_bitrate = 320;
}

void vbr_set_bitrate_range(vbr_state_t *vbr, int min_kbps, int max_kbps)
{
    if (min_kbps > 0) vbr->min_bitrate = min_kbps;
    if (max_kbps > 0) vbr->max_bitrate = max_kbps;

    /* Ensure min <= max */
    if (vbr->min_bitrate > vbr->max_bitrate) {
        vbr->min_bitrate = vbr->max_bitrate;
    }
}

/* Find bitrate index for a given kbps value */
static int find_bitrate_index(int kbps)
{
    int i;
    for (i = 1; i < 15; i++) {
        if (valid_bitrates[i] == kbps) {
            return i;
        }
    }
    /* Find closest */
    for (i = 1; i < 14; i++) {
        if (valid_bitrates[i] >= kbps) {
            return i;
        }
    }
    return 14; /* 320 kbps */
}

/* Find closest valid bitrate >= target */
static int find_nearest_bitrate(int target, int min_br, int max_br)
{
    int i;
    int best = valid_bitrates[14]; /* Start with max */

    for (i = 1; i < 15; i++) {
        int br = valid_bitrates[i];
        if (br >= min_br && br <= max_br) {
            if (br >= target && br < best) {
                best = br;
            }
        }
    }

    /* If no valid bitrate found >= target, find highest <= max */
    if (best > max_br) {
        for (i = 14; i >= 1; i--) {
            if (valid_bitrates[i] <= max_br && valid_bitrates[i] >= min_br) {
                return valid_bitrates[i];
            }
        }
    }

    return best;
}

int vbr_select_bitrate(vbr_state_t *vbr, float complexity)
{
    int target_kbps;

    if (vbr->mode == VBR_MODE_OFF) {
        /* CBR mode - should not be called */
        return 9; /* 128 kbps default */
    }

    /* Clamp complexity to 0.0 - 1.0 */
    if (complexity < 0.0f) complexity = 0.0f;
    if (complexity > 1.0f) complexity = 1.0f;

    if (vbr->mode == VBR_MODE_VBR) {
        /* VBR: Map complexity to bitrate range */
        float range = (float)(vbr->max_bitrate - vbr->min_bitrate);
        target_kbps = vbr->min_bitrate + (int)(complexity * range);
    } else {
        /* ABR: Adjust based on running average */
        float current_avg = 0;
        if (vbr->abr_frames > 0) {
            current_avg = (float)vbr->abr_bits_used / (float)vbr->abr_frames / 1152.0f * 44100.0f / 1000.0f;
        }

        /* If running below target, allow higher bitrate */
        /* If running above target, use lower bitrate */
        float ratio = (vbr->abr_target > 0 && current_avg > 0) ?
                      (float)vbr->abr_target / current_avg : 1.0f;

        target_kbps = (int)(vbr->abr_target * complexity * ratio);
        if (target_kbps < vbr->min_bitrate) target_kbps = vbr->min_bitrate;
        if (target_kbps > vbr->max_bitrate) target_kbps = vbr->max_bitrate;
    }

    /* Find closest valid bitrate */
    int actual_kbps = find_nearest_bitrate(target_kbps, vbr->min_bitrate, vbr->max_bitrate);
    return find_bitrate_index(actual_kbps);
}

void vbr_update_stats(vbr_state_t *vbr, int bitrate_index, int frame_bytes)
{
    vbr->total_frames++;
    vbr->total_bytes += frame_bytes;
    vbr->total_samples += 1152; /* Samples per frame for Layer III */

    /* Update bitrate histogram */
    if (bitrate_index >= 0 && bitrate_index < 15) {
        vbr->bitrate_hist[bitrate_index]++;
    }

    /* Update ABR tracking */
    vbr->abr_bits_used += frame_bytes * 8;
    vbr->abr_frames++;

    /* Update TOC (seek table) */
    vbr->toc_accumulated += frame_bytes;
}

/* Xing header is placed in the first frame */
/* Frame size for 128kbps @ 44.1kHz = 417 bytes */
/* We'll use the 128kbps frame size for the header frame */
size_t vbr_xing_header_size(void)
{
    /* Frame header (4) + side info (32 for stereo) + Xing data */
    /* Xing: "Xing"(4) + flags(4) + frames(4) + bytes(4) + toc(100) + quality(4) = 120 */
    /* Total in ancillary area: 120 bytes minimum */
    /* Use a full 128kbps frame: 417 bytes */
    return 417;
}

/* Write big-endian 32-bit value */
static void write_be32(uint8_t *buf, uint32_t val)
{
    buf[0] = (val >> 24) & 0xFF;
    buf[1] = (val >> 16) & 0xFF;
    buf[2] = (val >> 8) & 0xFF;
    buf[3] = val & 0xFF;
}

size_t vbr_write_xing_header(uint8_t *buffer, const vbr_state_t *vbr)
{
    size_t offset = 0;
    uint32_t flags = 0;
    int i;

    /* Clear buffer */
    memset(buffer, 0, vbr_xing_header_size());

    /* MP3 frame header for 128kbps, 44.1kHz, stereo */
    /* Sync word + MPEG1 Layer III + no CRC + 128kbps + 44.1kHz + no padding + stereo */
    buffer[0] = 0xFF;
    buffer[1] = 0xFB; /* MPEG1, Layer III, no CRC */
    buffer[2] = 0x90; /* 128kbps, 44.1kHz, no padding */
    buffer[3] = 0x00; /* Stereo, no emphasis */
    offset = 4;

    /* Skip side information (32 bytes for stereo MPEG1) */
    offset += 32;

    /* Xing header ID */
    if (vbr->mode == VBR_MODE_VBR) {
        memcpy(buffer + offset, "Xing", 4);
    } else {
        memcpy(buffer + offset, "Info", 4); /* CBR or ABR uses "Info" */
    }
    offset += 4;

    /* Flags: frames | bytes | toc | quality */
    flags = 0x0F; /* All fields present */
    write_be32(buffer + offset, flags);
    offset += 4;

    /* Number of frames */
    write_be32(buffer + offset, vbr->total_frames);
    offset += 4;

    /* Total bytes */
    write_be32(buffer + offset, vbr->total_bytes);
    offset += 4;

    /* TOC (100 bytes) - seek table */
    /* Each entry represents 1% of file duration */
    /* Value is byte position as percentage of total size (0-255) */
    if (vbr->total_bytes > 0 && vbr->total_frames > 0) {
        uint32_t bytes_per_frame = vbr->total_bytes / vbr->total_frames;
        for (i = 0; i < 100; i++) {
            uint32_t target_frame = (uint32_t)((uint64_t)i * vbr->total_frames / 100);
            uint32_t target_byte = target_frame * bytes_per_frame;
            buffer[offset + i] = (uint8_t)((uint64_t)target_byte * 256 / vbr->total_bytes);
        }
    } else {
        /* Linear TOC as fallback */
        for (i = 0; i < 100; i++) {
            buffer[offset + i] = (uint8_t)(i * 256 / 100);
        }
    }
    offset += 100;

    /* Quality indicator (0-100, where 0 is best) */
    /* Map V0-V9 to 0-100 */
    write_be32(buffer + offset, vbr->quality * 10);
    offset += 4;

    return vbr_xing_header_size();
}

int vbr_write_xing_to_file(FILE *fp, const vbr_state_t *vbr)
{
    uint8_t buffer[512];
    size_t size;

    if (!fp || !vbr) {
        return -1;
    }

    size = vbr_write_xing_header(buffer, vbr);

    if (fwrite(buffer, 1, size, fp) != size) {
        return -1;
    }

    return 0;
}
