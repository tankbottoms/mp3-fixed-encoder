#ifndef __FPMP3_H__
#define __FPMP3_H__

#ifdef __cplusplus
extern "C" {
#endif

/* Standard CBR encoding (44.1kHz) */
long fpmp3_start(int bitrate);

/* Extended initialization with sample rate */
long fpmp3_start_ex(int bitrate, int sample_rate);

long fpmp3_encode(short *pcm, unsigned char *mp3out, unsigned int *bytesout);

long fpmp3_finish(unsigned char *mp3out, unsigned int *bytesout);

/* VBR support functions */

/*
 * Set the bitrate for the next frame (for VBR encoding).
 * Call this before fpmp3_encode() to change bitrate per frame.
 * bitrate: One of 32, 40, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320
 * Returns: 0 on success, error code on failure
 */
long fpmp3_set_bitrate(int bitrate);

/*
 * Get the audio complexity of the last encoded frame.
 * Call this after fpmp3_encode() to determine bitrate for next frame.
 * Returns: Complexity value 0.0 (simple/silence) to 1.0 (complex)
 */
float fpmp3_get_complexity(void);

/*
 * Get the size of the last encoded frame in bytes.
 * Returns: Frame size in bytes
 */
int fpmp3_get_frame_size(void);

#ifdef __cplusplus
}
#endif

#endif//__FPMP3_H__
