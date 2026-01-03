/* mp3_info.c - MP3 File Analyzer
 *
 * Parses and displays MP3 file information:
 * - Frame headers and bitrate
 * - MPEG version and layer
 * - Channel mode
 * - Sampling rate
 * - Frame count and duration
 * - ID3 tag information
 *
 * Copyright (C) 2025 Mark Phillips
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

/* MPEG Audio version table */
static const char *mpeg_versions[] = {
    "MPEG Version 2.5", "Reserved", "MPEG Version 2", "MPEG Version 1"
};

/* Layer table */
static const char *layers[] = {
    "Reserved", "Layer III", "Layer II", "Layer I"
};

/* Channel mode table */
static const char *channel_modes[] = {
    "Stereo", "Joint Stereo", "Dual Channel", "Mono"
};

/* Bitrate tables (kbps) */
static const int bitrates_v1_l1[] = {0,32,64,96,128,160,192,224,256,288,320,352,384,416,448,-1};
static const int bitrates_v1_l2[] = {0,32,48,56,64,80,96,112,128,160,192,224,256,320,384,-1};
static const int bitrates_v1_l3[] = {0,32,40,48,56,64,80,96,112,128,160,192,224,256,320,-1};
static const int bitrates_v2_l1[] = {0,32,48,56,64,80,96,112,128,144,160,176,192,224,256,-1};
static const int bitrates_v2_l23[] = {0,8,16,24,32,40,48,56,64,80,96,112,128,144,160,-1};

/* Sample rate tables (Hz) */
static const int sample_rates_v1[] = {44100, 48000, 32000, 0};
static const int sample_rates_v2[] = {22050, 24000, 16000, 0};
static const int sample_rates_v25[] = {11025, 12000, 8000, 0};

typedef struct {
    int version;        /* 0=2.5, 2=2, 3=1 */
    int layer;          /* 1=III, 2=II, 3=I */
    int bitrate;        /* kbps */
    int sample_rate;    /* Hz */
    int channel_mode;   /* 0-3 */
    int padding;
    int frame_size;     /* bytes */
} mp3_frame_t;

static int parse_frame_header(uint32_t header, mp3_frame_t *frame)
{
    int version_id, layer_id, bitrate_idx, sample_rate_idx;
    const int *bitrate_table;
    const int *sample_rate_table;

    /* Check sync word (11 bits) */
    if ((header & 0xFFE00000) != 0xFFE00000) {
        return -1;
    }

    version_id = (header >> 19) & 0x03;
    layer_id = (header >> 17) & 0x03;
    bitrate_idx = (header >> 12) & 0x0F;
    sample_rate_idx = (header >> 10) & 0x03;

    /* Reserved values */
    if (version_id == 1 || layer_id == 0 || bitrate_idx == 15 || sample_rate_idx == 3) {
        return -1;
    }

    frame->version = version_id;
    frame->layer = layer_id;
    frame->channel_mode = (header >> 6) & 0x03;
    frame->padding = (header >> 9) & 0x01;

    /* Get bitrate */
    if (version_id == 3) { /* MPEG 1 */
        if (layer_id == 3) bitrate_table = bitrates_v1_l1;
        else if (layer_id == 2) bitrate_table = bitrates_v1_l2;
        else bitrate_table = bitrates_v1_l3;
        sample_rate_table = sample_rates_v1;
    } else { /* MPEG 2 or 2.5 */
        if (layer_id == 3) bitrate_table = bitrates_v2_l1;
        else bitrate_table = bitrates_v2_l23;
        if (version_id == 2) sample_rate_table = sample_rates_v2;
        else sample_rate_table = sample_rates_v25;
    }

    frame->bitrate = bitrate_table[bitrate_idx];
    frame->sample_rate = sample_rate_table[sample_rate_idx];

    if (frame->bitrate <= 0 || frame->sample_rate <= 0) {
        return -1;
    }

    /* Calculate frame size */
    if (layer_id == 3) { /* Layer I */
        frame->frame_size = (12 * frame->bitrate * 1000 / frame->sample_rate + frame->padding) * 4;
    } else { /* Layer II or III */
        int samples_per_frame = (version_id == 3) ? 1152 : 576;
        frame->frame_size = samples_per_frame / 8 * frame->bitrate * 1000 / frame->sample_rate + frame->padding;
    }

    return 0;
}

static int skip_id3v2(FILE *fp)
{
    unsigned char buf[10];
    uint32_t tag_size;

    if (fread(buf, 1, 10, fp) != 10) {
        return 0;
    }

    if (memcmp(buf, "ID3", 3) == 0) {
        /* ID3v2 tag present */
        tag_size = ((buf[6] & 0x7F) << 21) |
                   ((buf[7] & 0x7F) << 14) |
                   ((buf[8] & 0x7F) << 7) |
                   (buf[9] & 0x7F);
        fseek(fp, tag_size, SEEK_CUR);
        return 10 + tag_size;
    } else {
        /* No ID3v2, rewind */
        fseek(fp, -10, SEEK_CUR);
        return 0;
    }
}

static void check_id3v1(FILE *fp)
{
    unsigned char buf[128];
    long current_pos = ftell(fp);

    fseek(fp, -128, SEEK_END);
    if (fread(buf, 1, 128, fp) == 128) {
        if (memcmp(buf, "TAG", 3) == 0) {
            char title[31] = {0};
            char artist[31] = {0};
            char album[31] = {0};
            char year[5] = {0};

            memcpy(title, buf + 3, 30);
            memcpy(artist, buf + 33, 30);
            memcpy(album, buf + 63, 30);
            memcpy(year, buf + 93, 4);

            /* Trim trailing spaces */
            int i;
            for (i = 29; i >= 0 && title[i] == ' '; i--) title[i] = 0;
            for (i = 29; i >= 0 && artist[i] == ' '; i--) artist[i] = 0;
            for (i = 29; i >= 0 && album[i] == ' '; i--) album[i] = 0;

            if (title[0] || artist[0] || album[0]) {
                printf("\nID3v1 Tag:\n");
                if (title[0]) printf("  Title:  %s\n", title);
                if (artist[0]) printf("  Artist: %s\n", artist);
                if (album[0]) printf("  Album:  %s\n", album);
                if (year[0]) printf("  Year:   %s\n", year);
            }
        }
    }

    fseek(fp, current_pos, SEEK_SET);
}

int main(int argc, char **argv)
{
    FILE *fp;
    unsigned char buf[4];
    uint32_t header;
    mp3_frame_t frame;
    int frame_count = 0;
    int id3v2_size;
    long file_size;
    int vbr = 0;
    int first_bitrate = 0;
    long total_bitrate = 0;
    int min_bitrate = 999999;
    int max_bitrate = 0;

    if (argc < 2) {
        fprintf(stderr, "MP3 File Analyzer\n\n");
        fprintf(stderr, "Usage: %s <input.mp3>\n", argv[0]);
        return 1;
    }

    fp = fopen(argv[1], "rb");
    if (!fp) {
        fprintf(stderr, "Error: Cannot open '%s'\n", argv[1]);
        return 1;
    }

    /* Get file size */
    fseek(fp, 0, SEEK_END);
    file_size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    /* Skip ID3v2 tag if present */
    id3v2_size = skip_id3v2(fp);

    /* Find and parse frames */
    while (fread(buf, 1, 4, fp) == 4) {
        header = (buf[0] << 24) | (buf[1] << 16) | (buf[2] << 8) | buf[3];

        if (parse_frame_header(header, &frame) == 0) {
            frame_count++;
            total_bitrate += frame.bitrate;

            if (frame.bitrate < min_bitrate) min_bitrate = frame.bitrate;
            if (frame.bitrate > max_bitrate) max_bitrate = frame.bitrate;

            if (first_bitrate == 0) {
                first_bitrate = frame.bitrate;
            } else if (frame.bitrate != first_bitrate) {
                vbr = 1;
            }

            /* Seek to next frame */
            if (frame.frame_size > 4) {
                fseek(fp, frame.frame_size - 4, SEEK_CUR);
            }
        } else {
            /* Not a valid frame header, seek back 3 bytes and try again */
            fseek(fp, -3, SEEK_CUR);
        }

        /* Safety limit */
        if (frame_count > 10000000) break;
    }

    if (frame_count == 0) {
        fprintf(stderr, "Error: No valid MP3 frames found\n");
        fclose(fp);
        return 1;
    }

    /* Calculate duration */
    int samples_per_frame = (frame.version == 3) ? 1152 : 576;
    double duration = (double)frame_count * samples_per_frame / frame.sample_rate;
    int avg_bitrate = (int)(total_bitrate / frame_count);

    /* Print results */
    printf("\n");
    printf("========================================\n");
    printf("       MP3 File Analysis Report\n");
    printf("========================================\n");
    printf("\n");
    printf("File: %s\n", argv[1]);
    printf("Size: %ld bytes (%.2f MB)\n", file_size, file_size / 1048576.0);
    printf("\n");
    printf("Audio Properties:\n");
    printf("  Format:       %s, %s\n", mpeg_versions[frame.version], layers[frame.layer]);
    printf("  Channels:     %s\n", channel_modes[frame.channel_mode]);
    printf("  Sample Rate:  %d Hz\n", frame.sample_rate);
    printf("  Duration:     %.2f seconds\n", duration);
    printf("\n");
    printf("Bitrate Information:\n");
    printf("  Mode:         %s\n", vbr ? "Variable (VBR)" : "Constant (CBR)");
    printf("  Average:      %d kbps\n", avg_bitrate);
    if (vbr) {
        printf("  Minimum:      %d kbps\n", min_bitrate);
        printf("  Maximum:      %d kbps\n", max_bitrate);
    }
    printf("\n");
    printf("Frame Information:\n");
    printf("  Total Frames: %d\n", frame_count);
    printf("  Frame Size:   %d bytes (last frame)\n", frame.frame_size);
    if (id3v2_size > 0) {
        printf("  ID3v2 Tag:    %d bytes\n", id3v2_size);
    }
    printf("\n");

    /* Check for ID3v1 tag */
    check_id3v1(fp);

    printf("========================================\n");

    fclose(fp);
    return 0;
}
