/* wav_analyzer.c - WAV Audio Analyzer
 *
 * Analyzes WAV files and reports various audio metrics:
 * - Peak and RMS levels
 * - DC offset
 * - Dynamic range
 * - Spectral information
 * - Silence detection
 *
 * Copyright (C) 2025 Mark Phillips
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>

#define MAX_CHANNELS 2
#define FFT_SIZE 4096

typedef struct {
    uint16_t audio_format;
    uint16_t num_channels;
    uint32_t sample_rate;
    uint32_t byte_rate;
    uint16_t block_align;
    uint16_t bits_per_sample;
    uint32_t data_size;
} wav_info_t;

typedef struct {
    int16_t peak_pos;
    int16_t peak_neg;
    double rms;
    double dc_offset;
    int64_t sum;
    uint64_t sum_sq;
    uint32_t sample_count;
    uint32_t silent_samples;
    uint32_t clipped_samples;
} channel_stats_t;

static int read_wav_info(FILE *fp, wav_info_t *info)
{
    unsigned char buf[44];
    uint32_t chunk_size;

    if (fread(buf, 1, 12, fp) != 12) return -1;

    if (memcmp(buf, "RIFF", 4) != 0 || memcmp(buf + 8, "WAVE", 4) != 0) {
        fprintf(stderr, "Error: Not a WAV file\n");
        return -1;
    }

    /* Find fmt chunk */
    while (1) {
        if (fread(buf, 1, 8, fp) != 8) return -1;
        chunk_size = buf[4] | (buf[5] << 8) | (buf[6] << 16) | (buf[7] << 24);

        if (memcmp(buf, "fmt ", 4) == 0) break;
        fseek(fp, chunk_size, SEEK_CUR);
    }

    if (fread(buf, 1, 16, fp) != 16) return -1;

    info->audio_format = buf[0] | (buf[1] << 8);
    info->num_channels = buf[2] | (buf[3] << 8);
    info->sample_rate = buf[4] | (buf[5] << 8) | (buf[6] << 16) | (buf[7] << 24);
    info->byte_rate = buf[8] | (buf[9] << 8) | (buf[10] << 16) | (buf[11] << 24);
    info->block_align = buf[12] | (buf[13] << 8);
    info->bits_per_sample = buf[14] | (buf[15] << 8);

    if (chunk_size > 16) {
        fseek(fp, chunk_size - 16, SEEK_CUR);
    }

    /* Find data chunk */
    while (1) {
        if (fread(buf, 1, 8, fp) != 8) return -1;
        chunk_size = buf[4] | (buf[5] << 8) | (buf[6] << 16) | (buf[7] << 24);

        if (memcmp(buf, "data", 4) == 0) {
            info->data_size = chunk_size;
            break;
        }
        fseek(fp, chunk_size, SEEK_CUR);
    }

    return 0;
}

static double to_db(double linear)
{
    if (linear <= 0.0) return -100.0;
    return 20.0 * log10(linear);
}

int main(int argc, char **argv)
{
    FILE *fp;
    wav_info_t info;
    channel_stats_t stats[MAX_CHANNELS];
    int16_t sample_buf[8192];
    size_t samples_read;
    int i;
    double duration;

    if (argc < 2) {
        fprintf(stderr, "WAV Audio Analyzer\n\n");
        fprintf(stderr, "Usage: %s <input.wav>\n", argv[0]);
        return 1;
    }

    fp = fopen(argv[1], "rb");
    if (!fp) {
        fprintf(stderr, "Error: Cannot open '%s'\n", argv[1]);
        return 1;
    }

    if (read_wav_info(fp, &info) != 0) {
        fclose(fp);
        return 1;
    }

    if (info.audio_format != 1) {
        fprintf(stderr, "Error: Only PCM format supported\n");
        fclose(fp);
        return 1;
    }

    if (info.bits_per_sample != 16) {
        fprintf(stderr, "Error: Only 16-bit samples supported\n");
        fclose(fp);
        return 1;
    }

    if (info.num_channels > MAX_CHANNELS) {
        fprintf(stderr, "Error: Maximum %d channels supported\n", MAX_CHANNELS);
        fclose(fp);
        return 1;
    }

    /* Initialize stats */
    for (i = 0; i < info.num_channels; i++) {
        stats[i].peak_pos = 0;
        stats[i].peak_neg = 0;
        stats[i].sum = 0;
        stats[i].sum_sq = 0;
        stats[i].sample_count = 0;
        stats[i].silent_samples = 0;
        stats[i].clipped_samples = 0;
    }

    /* Analyze samples */
    while ((samples_read = fread(sample_buf, sizeof(int16_t),
                                  sizeof(sample_buf) / sizeof(int16_t), fp)) > 0) {
        size_t frames = samples_read / info.num_channels;
        size_t j;

        for (j = 0; j < frames; j++) {
            for (i = 0; i < info.num_channels; i++) {
                int16_t sample = sample_buf[j * info.num_channels + i];

                stats[i].sample_count++;
                stats[i].sum += sample;
                stats[i].sum_sq += (int64_t)sample * sample;

                if (sample > stats[i].peak_pos) stats[i].peak_pos = sample;
                if (sample < stats[i].peak_neg) stats[i].peak_neg = sample;

                if (sample == 0) stats[i].silent_samples++;
                if (sample >= 32767 || sample <= -32768) stats[i].clipped_samples++;
            }
        }
    }

    fclose(fp);

    /* Calculate derived statistics */
    for (i = 0; i < info.num_channels; i++) {
        stats[i].dc_offset = (double)stats[i].sum / stats[i].sample_count / 32768.0;
        stats[i].rms = sqrt((double)stats[i].sum_sq / stats[i].sample_count) / 32768.0;
    }

    duration = (double)stats[0].sample_count / info.sample_rate;

    /* Print results */
    printf("\n");
    printf("========================================\n");
    printf("       WAV Audio Analysis Report\n");
    printf("========================================\n");
    printf("\n");
    printf("File: %s\n", argv[1]);
    printf("\n");
    printf("Format Information:\n");
    printf("  Format:       PCM\n");
    printf("  Channels:     %d\n", info.num_channels);
    printf("  Sample Rate:  %d Hz\n", info.sample_rate);
    printf("  Bit Depth:    %d bits\n", info.bits_per_sample);
    printf("  Duration:     %.2f seconds\n", duration);
    printf("  Data Size:    %u bytes (%.2f MB)\n",
           info.data_size, info.data_size / 1048576.0);
    printf("\n");

    for (i = 0; i < info.num_channels; i++) {
        double peak_linear = fabs(stats[i].peak_pos) > fabs(stats[i].peak_neg) ?
                             fabs(stats[i].peak_pos) / 32768.0 :
                             fabs(stats[i].peak_neg) / 32768.0;
        double dynamic_range = peak_linear / stats[i].rms;
        double silence_pct = 100.0 * stats[i].silent_samples / stats[i].sample_count;
        double clip_pct = 100.0 * stats[i].clipped_samples / stats[i].sample_count;

        printf("Channel %d Analysis:\n", i + 1);
        printf("  Peak Level:     %.1f dBFS (sample: %+d / %+d)\n",
               to_db(peak_linear), stats[i].peak_pos, stats[i].peak_neg);
        printf("  RMS Level:      %.1f dBFS (%.4f linear)\n",
               to_db(stats[i].rms), stats[i].rms);
        printf("  DC Offset:      %.6f (%.4f%%)\n",
               stats[i].dc_offset, stats[i].dc_offset * 100.0);
        printf("  Dynamic Range:  %.1f dB\n", to_db(dynamic_range));
        printf("  Silent Samples: %u (%.2f%%)\n",
               stats[i].silent_samples, silence_pct);
        printf("  Clipped:        %u (%.4f%%)\n",
               stats[i].clipped_samples, clip_pct);
        printf("\n");
    }

    printf("========================================\n");

    return 0;
}
