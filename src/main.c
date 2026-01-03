/* main.c - PEM Fixed-Point MP3 Encoder CLI
 *
 * A command-line MP3 encoder using the PEM (Portable Embedded MP3) library.
 * This encoder uses fixed-point arithmetic, making it suitable for embedded
 * systems while maintaining good audio quality.
 *
 * Original encoder: Copyright (C) 1998-2002 Segher Boessenkool
 * Copyright (C) 2001 Interactive Objects, Inc.
 * Standalone adaptation: Copyright (C) 2025 Mark Phillips
 *
 * Usage: pem_encode [-b bitrate] [--title ...] input.wav output.mp3
 *
 * Supported bitrates: 32, 40, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320
 * Input: 16-bit stereo WAV at 44100 Hz
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>

#include "fpmp3.h"
#include "codec_workspace.h"
#include "eresult.h"
#include "id3tag.h"
#include "vbr.h"

#define VERSION "1.1.0"
#define SAMPLES_PER_FRAME 1152

/* WAV file header structure */
typedef struct {
    /* RIFF chunk */
    char riff_id[4];          /* "RIFF" */
    uint32_t file_size;       /* File size - 8 */
    char wave_id[4];          /* "WAVE" */

    /* fmt chunk */
    char fmt_id[4];           /* "fmt " */
    uint32_t fmt_size;        /* Format chunk size (16 for PCM) */
    uint16_t audio_format;    /* Audio format (1 = PCM) */
    uint16_t num_channels;    /* Number of channels */
    uint32_t sample_rate;     /* Sample rate */
    uint32_t byte_rate;       /* Bytes per second */
    uint16_t block_align;     /* Bytes per sample frame */
    uint16_t bits_per_sample; /* Bits per sample */

    /* data chunk */
    char data_id[4];          /* "data" */
    uint32_t data_size;       /* Data size in bytes */
} wav_header_t;

/* Encoder statistics */
typedef struct {
    unsigned long frames_encoded;
    unsigned long bytes_in;
    unsigned long bytes_out;
    double encoding_time;
    int bitrate;
} encoder_stats_t;

static void print_usage(const char *prog_name)
{
    fprintf(stderr, "PEM Fixed-Point MP3 Encoder v%s\n", VERSION);
    fprintf(stderr, "Copyright (C) 1998-2002 Segher Boessenkool\n");
    fprintf(stderr, "Copyright (C) 2001 Interactive Objects, Inc.\n\n");
    fprintf(stderr, "Usage: %s [options] input.wav output.mp3\n\n", prog_name);
    fprintf(stderr, "Encoding Options:\n");
    fprintf(stderr, "  -b bitrate   Set CBR encoding bitrate (default: 128)\n");
    fprintf(stderr, "               Valid: 32, 40, 48, 56, 64, 80, 96, 112,\n");
    fprintf(stderr, "                      128, 160, 192, 224, 256, 320 kbps\n");
    fprintf(stderr, "  -V quality   VBR mode with quality 0-9 (0=best, 9=smallest)\n");
    fprintf(stderr, "               Recommended: -V 2 (~190 kbps avg)\n");
    fprintf(stderr, "  --abr rate   ABR mode targeting specified average bitrate\n");
    fprintf(stderr, "  --vbr-min    Minimum bitrate for VBR/ABR (default: preset)\n");
    fprintf(stderr, "  --vbr-max    Maximum bitrate for VBR/ABR (default: preset)\n");
    fprintf(stderr, "  -q           Quiet mode (suppress progress output)\n");
    fprintf(stderr, "  -s           Print statistics after encoding\n");
    fprintf(stderr, "  -h           Show this help message\n\n");
    fprintf(stderr, "ID3 Tag Options:\n");
    fprintf(stderr, "  --title      Set song title\n");
    fprintf(stderr, "  --artist     Set artist name\n");
    fprintf(stderr, "  --album      Set album name\n");
    fprintf(stderr, "  --year       Set year (4 digits)\n");
    fprintf(stderr, "  --comment    Set comment\n");
    fprintf(stderr, "  --track      Set track number (1-255)\n");
    fprintf(stderr, "  --genre      Set genre number (0-255)\n");
    fprintf(stderr, "  --id3v1      Write ID3v1 tag only (default: both v1 and v2)\n");
    fprintf(stderr, "  --id3v2      Write ID3v2 tag only\n");
    fprintf(stderr, "  --no-id3     Don't write any ID3 tags\n");
}

static int validate_bitrate(int bitrate)
{
    static const int valid_bitrates[] = {
        32, 40, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320
    };
    int i;
    for (i = 0; i < 14; i++) {
        if (bitrate == valid_bitrates[i]) {
            return 1;
        }
    }
    return 0;
}

static int read_wav_header(FILE *fp, wav_header_t *header)
{
    unsigned char buf[44];

    if (fread(buf, 1, 44, fp) != 44) {
        return -1;
    }

    /* Parse RIFF header */
    memcpy(header->riff_id, buf, 4);
    header->file_size = buf[4] | (buf[5] << 8) | (buf[6] << 16) | (buf[7] << 24);
    memcpy(header->wave_id, buf + 8, 4);

    /* Parse fmt chunk */
    memcpy(header->fmt_id, buf + 12, 4);
    header->fmt_size = buf[16] | (buf[17] << 8) | (buf[18] << 16) | (buf[19] << 24);
    header->audio_format = buf[20] | (buf[21] << 8);
    header->num_channels = buf[22] | (buf[23] << 8);
    header->sample_rate = buf[24] | (buf[25] << 8) | (buf[26] << 16) | (buf[27] << 24);
    header->byte_rate = buf[28] | (buf[29] << 8) | (buf[30] << 16) | (buf[31] << 24);
    header->block_align = buf[32] | (buf[33] << 8);
    header->bits_per_sample = buf[34] | (buf[35] << 8);

    /* Parse data chunk */
    memcpy(header->data_id, buf + 36, 4);
    header->data_size = buf[40] | (buf[41] << 8) | (buf[42] << 16) | (buf[43] << 24);

    /* Validate header */
    if (memcmp(header->riff_id, "RIFF", 4) != 0) {
        fprintf(stderr, "Error: Not a RIFF file\n");
        return -1;
    }
    if (memcmp(header->wave_id, "WAVE", 4) != 0) {
        fprintf(stderr, "Error: Not a WAVE file\n");
        return -1;
    }
    if (memcmp(header->fmt_id, "fmt ", 4) != 0) {
        fprintf(stderr, "Error: Cannot find fmt chunk\n");
        return -1;
    }

    /* Handle extended format chunks (skip extra bytes) */
    if (header->fmt_size > 16) {
        fseek(fp, header->fmt_size - 16, SEEK_CUR);
        /* Re-read the data chunk header */
        if (fread(buf + 36, 1, 8, fp) != 8) {
            return -1;
        }
        memcpy(header->data_id, buf + 36, 4);
        header->data_size = buf[40] | (buf[41] << 8) | (buf[42] << 16) | (buf[43] << 24);
    }

    /* Skip past unknown chunks to find data */
    while (memcmp(header->data_id, "data", 4) != 0) {
        uint32_t chunk_size = header->data_size;
        fseek(fp, chunk_size, SEEK_CUR);
        if (fread(buf + 36, 1, 8, fp) != 8) {
            fprintf(stderr, "Error: Cannot find data chunk\n");
            return -1;
        }
        memcpy(header->data_id, buf + 36, 4);
        header->data_size = buf[40] | (buf[41] << 8) | (buf[42] << 16) | (buf[43] << 24);
    }

    /* Validate format */
    if (header->audio_format != 1) {
        fprintf(stderr, "Error: Only PCM format supported (got format %d)\n",
                header->audio_format);
        return -1;
    }
    if (header->num_channels != 1 && header->num_channels != 2) {
        fprintf(stderr, "Error: Only mono (1) or stereo (2) channels supported (got %d)\n",
                header->num_channels);
        return -1;
    }
    if (header->sample_rate != 44100 && header->sample_rate != 48000 &&
        header->sample_rate != 32000) {
        fprintf(stderr, "Error: Sample rate must be 32000, 44100, or 48000 Hz (got %d)\n",
                header->sample_rate);
        return -1;
    }
    if (header->bits_per_sample != 16) {
        fprintf(stderr, "Error: Only 16-bit samples supported (got %d)\n",
                header->bits_per_sample);
        return -1;
    }

    return 0;
}

static void print_stats(encoder_stats_t *stats, vbr_state_t *vbr)
{
    double ratio;
    double speed;
    double duration;

    duration = (double)stats->bytes_in / (44100.0 * 4.0); /* 44100 Hz, stereo 16-bit */
    ratio = (stats->bytes_out > 0) ? ((double)stats->bytes_in / stats->bytes_out) : 0;
    speed = (stats->encoding_time > 0) ? (duration / stats->encoding_time) : 0;

    fprintf(stderr, "\n");
    fprintf(stderr, "========================================\n");
    fprintf(stderr, "         Encoding Statistics\n");
    fprintf(stderr, "========================================\n");
    if (vbr && vbr->mode == VBR_MODE_VBR) {
        double avg_bitrate = (duration > 0) ?
            ((double)stats->bytes_out * 8.0 / duration / 1000.0) : 0;
        fprintf(stderr, "  Mode:            VBR -V %d\n", vbr->quality);
        fprintf(stderr, "  Avg bitrate:     %.1f kbps\n", avg_bitrate);
    } else if (vbr && vbr->mode == VBR_MODE_ABR) {
        double avg_bitrate = (duration > 0) ?
            ((double)stats->bytes_out * 8.0 / duration / 1000.0) : 0;
        fprintf(stderr, "  Mode:            ABR ~%d kbps\n", vbr->abr_target);
        fprintf(stderr, "  Actual avg:      %.1f kbps\n", avg_bitrate);
    } else {
        fprintf(stderr, "  Mode:            CBR %d kbps\n", stats->bitrate);
    }
    fprintf(stderr, "  Frames encoded:  %lu\n", stats->frames_encoded);
    fprintf(stderr, "  Input size:      %lu bytes (%.2f MB)\n",
            stats->bytes_in, stats->bytes_in / 1048576.0);
    fprintf(stderr, "  Output size:     %lu bytes (%.2f MB)\n",
            stats->bytes_out, stats->bytes_out / 1048576.0);
    fprintf(stderr, "  Compression:     %.2f:1\n", ratio);
    fprintf(stderr, "  Duration:        %.2f seconds\n", duration);
    fprintf(stderr, "  Encoding time:   %.2f seconds\n", stats->encoding_time);
    fprintf(stderr, "  Speed:           %.2fx realtime\n", speed);
    fprintf(stderr, "========================================\n");
}

/* ID3 tag mode flags */
#define ID3_MODE_NONE  0
#define ID3_MODE_V1    1
#define ID3_MODE_V2    2
#define ID3_MODE_BOTH  3

int main(int argc, char **argv)
{
    FILE *wav_fp = NULL;
    FILE *mp3_fp = NULL;
    wav_header_t wav_header;
    int bitrate = 128;
    int quiet = 0;
    int show_stats = 0;
    const char *input_file = NULL;
    const char *output_file = NULL;
    int i;

    short pcm_buffer[SAMPLES_PER_FRAME * 2]; /* Stereo samples */
    unsigned char mp3_buffer[4096];
    unsigned int bytes_out;
    long result;

    encoder_stats_t stats = {0};
    clock_t start_time, end_time;

    unsigned long total_samples;
    unsigned long samples_processed = 0;
    int last_percent = -1;

    /* ID3 tag settings */
    id3_tag_t id3_tag;
    int id3_mode = ID3_MODE_BOTH;
    int has_id3_data = 0;

    /* VBR settings */
    vbr_state_t vbr_state;
    int vbr_quality = -1;  /* -1 = not set (CBR mode) */
    int abr_target = 0;    /* 0 = not set */
    int vbr_min = 0;       /* 0 = use preset default */
    int vbr_max = 0;       /* 0 = use preset default */
    long xing_header_pos = 0;

    id3_tag_init(&id3_tag);
    vbr_init(&vbr_state);

    /* Parse command line arguments */
    for (i = 1; i < argc; i++) {
        if (argv[i][0] == '-') {
            /* Long options */
            if (argv[i][1] == '-') {
                if (strcmp(argv[i], "--title") == 0 && i + 1 < argc) {
                    id3_tag_set_title(&id3_tag, argv[++i]);
                    has_id3_data = 1;
                } else if (strcmp(argv[i], "--artist") == 0 && i + 1 < argc) {
                    id3_tag_set_artist(&id3_tag, argv[++i]);
                    has_id3_data = 1;
                } else if (strcmp(argv[i], "--album") == 0 && i + 1 < argc) {
                    id3_tag_set_album(&id3_tag, argv[++i]);
                    has_id3_data = 1;
                } else if (strcmp(argv[i], "--year") == 0 && i + 1 < argc) {
                    id3_tag_set_year(&id3_tag, argv[++i]);
                    has_id3_data = 1;
                } else if (strcmp(argv[i], "--comment") == 0 && i + 1 < argc) {
                    id3_tag_set_comment(&id3_tag, argv[++i]);
                    has_id3_data = 1;
                } else if (strcmp(argv[i], "--track") == 0 && i + 1 < argc) {
                    id3_tag_set_track(&id3_tag, (uint8_t)atoi(argv[++i]));
                    has_id3_data = 1;
                } else if (strcmp(argv[i], "--genre") == 0 && i + 1 < argc) {
                    id3_tag_set_genre(&id3_tag, (uint8_t)atoi(argv[++i]));
                    has_id3_data = 1;
                } else if (strcmp(argv[i], "--id3v1") == 0) {
                    id3_mode = ID3_MODE_V1;
                } else if (strcmp(argv[i], "--id3v2") == 0) {
                    id3_mode = ID3_MODE_V2;
                } else if (strcmp(argv[i], "--no-id3") == 0) {
                    id3_mode = ID3_MODE_NONE;
                } else if (strcmp(argv[i], "--abr") == 0 && i + 1 < argc) {
                    abr_target = atoi(argv[++i]);
                    if (abr_target < 32 || abr_target > 320) {
                        fprintf(stderr, "Error: ABR target must be 32-320 kbps\n");
                        return 1;
                    }
                } else if (strcmp(argv[i], "--vbr-min") == 0 && i + 1 < argc) {
                    vbr_min = atoi(argv[++i]);
                } else if (strcmp(argv[i], "--vbr-max") == 0 && i + 1 < argc) {
                    vbr_max = atoi(argv[++i]);
                } else if (strcmp(argv[i], "--help") == 0) {
                    print_usage(argv[0]);
                    return 0;
                } else {
                    fprintf(stderr, "Error: Unknown option %s\n", argv[i]);
                    print_usage(argv[0]);
                    return 1;
                }
                continue;
            }

            /* Short options */
            switch (argv[i][1]) {
                case 'b':
                    if (i + 1 < argc) {
                        bitrate = atoi(argv[++i]);
                    } else {
                        fprintf(stderr, "Error: -b requires an argument\n");
                        return 1;
                    }
                    break;
                case 'V':
                    if (i + 1 < argc) {
                        vbr_quality = atoi(argv[++i]);
                        if (vbr_quality < 0 || vbr_quality > 9) {
                            fprintf(stderr, "Error: VBR quality must be 0-9\n");
                            return 1;
                        }
                    } else {
                        fprintf(stderr, "Error: -V requires an argument\n");
                        return 1;
                    }
                    break;
                case 'q':
                    quiet = 1;
                    break;
                case 's':
                    show_stats = 1;
                    break;
                case 'h':
                    print_usage(argv[0]);
                    return 0;
                default:
                    fprintf(stderr, "Error: Unknown option -%c\n", argv[i][1]);
                    print_usage(argv[0]);
                    return 1;
            }
        } else {
            if (input_file == NULL) {
                input_file = argv[i];
            } else if (output_file == NULL) {
                output_file = argv[i];
            } else {
                fprintf(stderr, "Error: Too many arguments\n");
                print_usage(argv[0]);
                return 1;
            }
        }
    }

    if (input_file == NULL || output_file == NULL) {
        fprintf(stderr, "Error: Input and output files required\n");
        print_usage(argv[0]);
        return 1;
    }

    /* Configure VBR/ABR mode if specified */
    if (vbr_quality >= 0) {
        vbr_set_quality(&vbr_state, vbr_quality);
        if (vbr_min > 0) vbr_set_bitrate_range(&vbr_state, vbr_min, 0);
        if (vbr_max > 0) vbr_set_bitrate_range(&vbr_state, 0, vbr_max);
        /* Start with mid-range bitrate for VBR */
        bitrate = vbr_presets[vbr_quality].target_bitrate;
        /* Find closest valid bitrate */
        if (!validate_bitrate(bitrate)) {
            static const int valid_bitrates[] = {32, 40, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320};
            int j;
            for (j = 0; j < 14; j++) {
                if (valid_bitrates[j] >= bitrate) {
                    bitrate = valid_bitrates[j];
                    break;
                }
            }
        }
    } else if (abr_target > 0) {
        vbr_set_abr(&vbr_state, abr_target);
        if (vbr_min > 0) vbr_set_bitrate_range(&vbr_state, vbr_min, 0);
        if (vbr_max > 0) vbr_set_bitrate_range(&vbr_state, 0, vbr_max);
        bitrate = abr_target;
        /* Find closest valid bitrate */
        if (!validate_bitrate(bitrate)) {
            static const int valid_bitrates[] = {32, 40, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320};
            int j;
            for (j = 0; j < 14; j++) {
                if (valid_bitrates[j] >= bitrate) {
                    bitrate = valid_bitrates[j];
                    break;
                }
            }
        }
    } else {
        /* CBR mode - validate bitrate */
        if (!validate_bitrate(bitrate)) {
            fprintf(stderr, "Error: Invalid bitrate %d kbps\n", bitrate);
            fprintf(stderr, "Valid bitrates: 32, 40, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320\n");
            return 1;
        }
    }

    /* Open input WAV file */
    wav_fp = fopen(input_file, "rb");
    if (!wav_fp) {
        fprintf(stderr, "Error: Cannot open input file '%s'\n", input_file);
        return 1;
    }

    /* Read and validate WAV header */
    if (read_wav_header(wav_fp, &wav_header) != 0) {
        fclose(wav_fp);
        return 1;
    }

    /* Open output MP3 file */
    mp3_fp = fopen(output_file, "wb");
    if (!mp3_fp) {
        fprintf(stderr, "Error: Cannot create output file '%s'\n", output_file);
        fclose(wav_fp);
        return 1;
    }

    /* Write ID3v2 tag at the start of the file if metadata provided */
    if (has_id3_data && (id3_mode & ID3_MODE_V2)) {
        if (id3v2_write(mp3_fp, &id3_tag) != 0) {
            fprintf(stderr, "Warning: Failed to write ID3v2 tag\n");
        }
    }

    /* Reserve space for Xing header if VBR/ABR mode */
    if (vbr_state.mode != VBR_MODE_OFF) {
        xing_header_pos = ftell(mp3_fp);
        /* Write placeholder (will be updated at end) */
        uint8_t xing_placeholder[512];
        memset(xing_placeholder, 0, sizeof(xing_placeholder));
        fwrite(xing_placeholder, 1, vbr_xing_header_size(), mp3_fp);
    }

    /* Allocate encoder workspace */
    if (!pem_alloc_workspace()) {
        fprintf(stderr, "Error: Cannot allocate encoder memory\n");
        fclose(wav_fp);
        fclose(mp3_fp);
        return 1;
    }

    if (!quiet) {
        fprintf(stderr, "PEM Fixed-Point MP3 Encoder v%s\n", VERSION);
        fprintf(stderr, "Encoding: %s -> %s\n", input_file, output_file);
        if (vbr_state.mode == VBR_MODE_VBR) {
            fprintf(stderr, "Mode: VBR -V %d (%d-%d kbps, ~%d kbps avg)\n",
                    vbr_state.quality, vbr_state.min_bitrate, vbr_state.max_bitrate,
                    vbr_presets[vbr_state.quality].target_bitrate);
        } else if (vbr_state.mode == VBR_MODE_ABR) {
            fprintf(stderr, "Mode: ABR ~%d kbps (%d-%d kbps range)\n",
                    vbr_state.abr_target, vbr_state.min_bitrate, vbr_state.max_bitrate);
        } else {
            fprintf(stderr, "Mode: CBR %d kbps\n", bitrate);
        }
        fprintf(stderr, "Sample rate: %d Hz\n", wav_header.sample_rate);
        fprintf(stderr, "\n");
    }

    /* Initialize encoder with sample rate from WAV */
    result = fpmp3_start_ex(bitrate, wav_header.sample_rate);
    if (FAILED(result)) {
        fprintf(stderr, "Error: Failed to initialize encoder (error 0x%08lx)\n", result);
        pem_free_workspace(pem_workspace);
        fclose(wav_fp);
        fclose(mp3_fp);
        return 1;
    }

    stats.bitrate = bitrate;

    /* Calculate total samples based on mono/stereo */
    int is_mono = (wav_header.num_channels == 1);
    int bytes_per_sample = is_mono ? 2 : 4;  /* Mono: 2 bytes, Stereo: 4 bytes per sample */
    total_samples = wav_header.data_size / bytes_per_sample;

    if (!quiet && is_mono) {
        fprintf(stderr, "Input is mono - converting to stereo\n\n");
    }

    start_time = clock();

    /* Temporary buffer for mono input */
    short mono_buffer[SAMPLES_PER_FRAME];

    /* Encode frames */
    while (1) {
        size_t samples_read;
        int is_eof;

        if (is_mono) {
            /* Read mono samples */
            samples_read = fread(mono_buffer, 2, SAMPLES_PER_FRAME, wav_fp);
            is_eof = (samples_read < SAMPLES_PER_FRAME);

            if (samples_read == 0 && is_eof) {
                break;
            }

            /* Convert mono to stereo by duplicating channels */
            for (size_t j = 0; j < samples_read; j++) {
                pcm_buffer[j * 2] = mono_buffer[j];      /* Left channel */
                pcm_buffer[j * 2 + 1] = mono_buffer[j];  /* Right channel (copy) */
            }

            /* Pad remaining samples with silence */
            if (samples_read < SAMPLES_PER_FRAME) {
                memset(pcm_buffer + samples_read * 2, 0,
                       (SAMPLES_PER_FRAME - samples_read) * 4);
            }

            stats.bytes_in += samples_read * 2;  /* Mono input bytes */
        } else {
            /* Read stereo samples */
            samples_read = fread(pcm_buffer, 4, SAMPLES_PER_FRAME, wav_fp);
            is_eof = (samples_read < SAMPLES_PER_FRAME);

            if (samples_read == 0 && is_eof) {
                break;
            }

            /* Pad remaining samples with silence */
            if (samples_read < SAMPLES_PER_FRAME) {
                memset(pcm_buffer + samples_read * 2, 0,
                       (SAMPLES_PER_FRAME - samples_read) * 4);
            }

            stats.bytes_in += samples_read * 4;  /* Stereo input bytes */
        }
        samples_processed += samples_read;

        bytes_out = 0;
        result = fpmp3_encode(pcm_buffer, mp3_buffer, &bytes_out);

        if (FAILED(result)) {
            fprintf(stderr, "\nError: Encoding failed at frame %lu (error 0x%08lx)\n",
                    stats.frames_encoded, result);
            break;
        }

        if (bytes_out > 0) {
            fwrite(mp3_buffer, 1, bytes_out, mp3_fp);
            stats.bytes_out += bytes_out;

            /* Update VBR statistics */
            if (vbr_state.mode != VBR_MODE_OFF) {
                int frame_size = fpmp3_get_frame_size();
                vbr_update_stats(&vbr_state, 0, frame_size);  /* bitrate_index not used currently */
            }
        }

        stats.frames_encoded++;

        /* For VBR/ABR: Get complexity and select bitrate for next frame */
        if (vbr_state.mode != VBR_MODE_OFF) {
            float complexity = fpmp3_get_complexity();
            int br_index = vbr_select_bitrate(&vbr_state, complexity);
            /* Map bitrate index to actual bitrate */
            static const int br_table[] = {0, 32, 40, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320};
            if (br_index >= 1 && br_index <= 14) {
                fpmp3_set_bitrate(br_table[br_index]);
            }
        }

        /* Progress indicator */
        if (!quiet && total_samples > 0) {
            int percent = (int)((samples_processed * 100) / total_samples);
            if (percent != last_percent && percent <= 100) {
                fprintf(stderr, "\rEncoding: %3d%% [", percent);
                int filled = percent / 2;
                for (i = 0; i < 50; i++) {
                    if (i < filled) {
                        fputc('=', stderr);
                    } else if (i == filled) {
                        fputc('>', stderr);
                    } else {
                        fputc(' ', stderr);
                    }
                }
                fprintf(stderr, "] %lu frames", stats.frames_encoded);
                fflush(stderr);
                last_percent = percent;
            }
        }

        if (is_eof) {
            break;
        }
    }

    /* Finish encoding */
    bytes_out = 0;
    fpmp3_finish(mp3_buffer, &bytes_out);
    if (bytes_out > 0) {
        fwrite(mp3_buffer, 1, bytes_out, mp3_fp);
        stats.bytes_out += bytes_out;
    }

    /* Write ID3v1 tag at the end of the file if metadata provided */
    if (has_id3_data && (id3_mode & ID3_MODE_V1)) {
        if (id3v1_write(mp3_fp, &id3_tag) != 0) {
            fprintf(stderr, "Warning: Failed to write ID3v1 tag\n");
        }
    }

    /* Finalize Xing header with actual statistics */
    if (vbr_state.mode != VBR_MODE_OFF && xing_header_pos >= 0) {
        long current_pos = ftell(mp3_fp);
        fseek(mp3_fp, xing_header_pos, SEEK_SET);
        if (vbr_write_xing_to_file(mp3_fp, &vbr_state) != 0) {
            fprintf(stderr, "Warning: Failed to write Xing header\n");
        }
        fseek(mp3_fp, current_pos, SEEK_SET);
    }

    end_time = clock();
    stats.encoding_time = (double)(end_time - start_time) / CLOCKS_PER_SEC;

    if (!quiet) {
        fprintf(stderr, "\rEncoding: 100%% [==================================================] Done!         \n");
    }

    /* Cleanup */
    pem_free_workspace(pem_workspace);
    fclose(wav_fp);
    fclose(mp3_fp);

    if (show_stats) {
        print_stats(&stats, &vbr_state);
    } else if (!quiet) {
        fprintf(stderr, "Successfully encoded %lu frames (%.2f seconds)\n",
                stats.frames_encoded,
                (double)stats.bytes_in / (44100.0 * 4.0));
    }

    return 0;
}
