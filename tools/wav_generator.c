/* wav_generator.c - WAV Test Signal Generator
 *
 * Generates various test signals in WAV format for encoder testing:
 * - Sine waves at specified frequencies
 * - Frequency sweeps (chirps)
 * - Square waves
 * - White noise
 * - Silence
 *
 * All output is 16-bit stereo at 44100 Hz.
 *
 * Copyright (C) 2025 Mark Phillips
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>
#include <time.h>

#define SAMPLE_RATE 44100
#define CHANNELS 2
#define BITS_PER_SAMPLE 16
#define PI 3.14159265358979323846

typedef struct {
    char riff_id[4];
    uint32_t file_size;
    char wave_id[4];
    char fmt_id[4];
    uint32_t fmt_size;
    uint16_t audio_format;
    uint16_t num_channels;
    uint32_t sample_rate;
    uint32_t byte_rate;
    uint16_t block_align;
    uint16_t bits_per_sample;
    char data_id[4];
    uint32_t data_size;
} wav_header_t;

static void write_wav_header(FILE *fp, uint32_t num_samples)
{
    wav_header_t header;
    uint32_t data_size = num_samples * CHANNELS * (BITS_PER_SAMPLE / 8);

    memcpy(header.riff_id, "RIFF", 4);
    header.file_size = 36 + data_size;
    memcpy(header.wave_id, "WAVE", 4);
    memcpy(header.fmt_id, "fmt ", 4);
    header.fmt_size = 16;
    header.audio_format = 1;  /* PCM */
    header.num_channels = CHANNELS;
    header.sample_rate = SAMPLE_RATE;
    header.byte_rate = SAMPLE_RATE * CHANNELS * (BITS_PER_SAMPLE / 8);
    header.block_align = CHANNELS * (BITS_PER_SAMPLE / 8);
    header.bits_per_sample = BITS_PER_SAMPLE;
    memcpy(header.data_id, "data", 4);
    header.data_size = data_size;

    fwrite(&header, sizeof(header), 1, fp);
}

static void generate_sine(FILE *fp, double frequency, double duration, double amplitude)
{
    uint32_t num_samples = (uint32_t)(duration * SAMPLE_RATE);
    uint32_t i;
    double phase = 0.0;
    double phase_inc = 2.0 * PI * frequency / SAMPLE_RATE;

    write_wav_header(fp, num_samples);

    for (i = 0; i < num_samples; i++) {
        int16_t sample = (int16_t)(sin(phase) * amplitude * 32767.0);
        /* Write stereo (same signal on both channels) */
        fwrite(&sample, sizeof(int16_t), 1, fp);
        fwrite(&sample, sizeof(int16_t), 1, fp);
        phase += phase_inc;
        if (phase >= 2.0 * PI) {
            phase -= 2.0 * PI;
        }
    }
}

static void generate_sweep(FILE *fp, double start_freq, double end_freq,
                           double duration, double amplitude)
{
    uint32_t num_samples = (uint32_t)(duration * SAMPLE_RATE);
    uint32_t i;
    double phase = 0.0;
    double k = (end_freq - start_freq) / duration;

    write_wav_header(fp, num_samples);

    for (i = 0; i < num_samples; i++) {
        double t = (double)i / SAMPLE_RATE;
        double freq = start_freq + k * t;
        double phase_inc = 2.0 * PI * freq / SAMPLE_RATE;

        int16_t sample = (int16_t)(sin(phase) * amplitude * 32767.0);
        fwrite(&sample, sizeof(int16_t), 1, fp);
        fwrite(&sample, sizeof(int16_t), 1, fp);

        phase += phase_inc;
        if (phase >= 2.0 * PI) {
            phase -= 2.0 * PI;
        }
    }
}

static void generate_square(FILE *fp, double frequency, double duration, double amplitude)
{
    uint32_t num_samples = (uint32_t)(duration * SAMPLE_RATE);
    uint32_t i;
    double phase = 0.0;
    double phase_inc = 2.0 * PI * frequency / SAMPLE_RATE;

    write_wav_header(fp, num_samples);

    for (i = 0; i < num_samples; i++) {
        int16_t sample = (phase < PI) ?
            (int16_t)(amplitude * 32767.0) :
            (int16_t)(-amplitude * 32767.0);
        fwrite(&sample, sizeof(int16_t), 1, fp);
        fwrite(&sample, sizeof(int16_t), 1, fp);
        phase += phase_inc;
        if (phase >= 2.0 * PI) {
            phase -= 2.0 * PI;
        }
    }
}

static void generate_noise(FILE *fp, double duration, double amplitude)
{
    uint32_t num_samples = (uint32_t)(duration * SAMPLE_RATE);
    uint32_t i;

    srand((unsigned int)time(NULL));
    write_wav_header(fp, num_samples);

    for (i = 0; i < num_samples; i++) {
        /* Generate white noise using Box-Muller transform for Gaussian */
        double u1 = (double)rand() / RAND_MAX;
        double u2 = (double)rand() / RAND_MAX;
        if (u1 < 0.0001) u1 = 0.0001;
        double z = sqrt(-2.0 * log(u1)) * cos(2.0 * PI * u2);

        /* Clip and scale */
        z = z * 0.3 * amplitude;
        if (z > 1.0) z = 1.0;
        if (z < -1.0) z = -1.0;

        int16_t sample = (int16_t)(z * 32767.0);
        fwrite(&sample, sizeof(int16_t), 1, fp);
        fwrite(&sample, sizeof(int16_t), 1, fp);
    }
}

static void generate_silence(FILE *fp, double duration)
{
    uint32_t num_samples = (uint32_t)(duration * SAMPLE_RATE);
    uint32_t i;
    int16_t sample = 0;

    write_wav_header(fp, num_samples);

    for (i = 0; i < num_samples; i++) {
        fwrite(&sample, sizeof(int16_t), 1, fp);
        fwrite(&sample, sizeof(int16_t), 1, fp);
    }
}

static void generate_impulse(FILE *fp, double duration, double amplitude)
{
    uint32_t num_samples = (uint32_t)(duration * SAMPLE_RATE);
    uint32_t i;
    int16_t sample;

    write_wav_header(fp, num_samples);

    /* First sample is the impulse */
    sample = (int16_t)(amplitude * 32767.0);
    fwrite(&sample, sizeof(int16_t), 1, fp);
    fwrite(&sample, sizeof(int16_t), 1, fp);

    /* Rest is silence */
    sample = 0;
    for (i = 1; i < num_samples; i++) {
        fwrite(&sample, sizeof(int16_t), 1, fp);
        fwrite(&sample, sizeof(int16_t), 1, fp);
    }
}

static void generate_multitone(FILE *fp, double duration, double amplitude)
{
    /* Generate a multi-frequency test signal: 100, 1000, 5000, 10000 Hz */
    uint32_t num_samples = (uint32_t)(duration * SAMPLE_RATE);
    uint32_t i;
    double freqs[] = {100.0, 1000.0, 5000.0, 10000.0};
    double phases[] = {0.0, 0.0, 0.0, 0.0};
    int num_freqs = 4;

    write_wav_header(fp, num_samples);

    for (i = 0; i < num_samples; i++) {
        double sum = 0.0;
        int j;

        for (j = 0; j < num_freqs; j++) {
            sum += sin(phases[j]);
            phases[j] += 2.0 * PI * freqs[j] / SAMPLE_RATE;
            if (phases[j] >= 2.0 * PI) {
                phases[j] -= 2.0 * PI;
            }
        }

        sum /= num_freqs;  /* Normalize */
        int16_t sample = (int16_t)(sum * amplitude * 32767.0);
        fwrite(&sample, sizeof(int16_t), 1, fp);
        fwrite(&sample, sizeof(int16_t), 1, fp);
    }
}

static void print_usage(const char *prog)
{
    fprintf(stderr, "WAV Test Signal Generator\n\n");
    fprintf(stderr, "Usage: %s <type> [options] <output.wav>\n\n", prog);
    fprintf(stderr, "Signal types:\n");
    fprintf(stderr, "  --sine <freq> <duration>           Sine wave\n");
    fprintf(stderr, "  --sweep <start> <end> <duration>   Frequency sweep\n");
    fprintf(stderr, "  --square <freq> <duration>         Square wave\n");
    fprintf(stderr, "  --noise <duration>                 White noise\n");
    fprintf(stderr, "  --silence <duration>               Digital silence\n");
    fprintf(stderr, "  --impulse <duration>               Impulse signal\n");
    fprintf(stderr, "  --multitone <duration>             Multi-frequency test\n\n");
    fprintf(stderr, "Options:\n");
    fprintf(stderr, "  -a <amplitude>   Amplitude 0.0-1.0 (default: 0.8)\n\n");
    fprintf(stderr, "All durations in seconds. Output is 44.1kHz 16-bit stereo.\n");
}

int main(int argc, char **argv)
{
    FILE *fp;
    const char *output_file = NULL;
    const char *signal_type = NULL;
    double amplitude = 0.8;
    double freq1 = 0, freq2 = 0, duration = 0;
    int i;

    if (argc < 3) {
        print_usage(argv[0]);
        return 1;
    }

    /* Parse arguments */
    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-a") == 0 && i + 1 < argc) {
            amplitude = atof(argv[++i]);
            if (amplitude < 0.0) amplitude = 0.0;
            if (amplitude > 1.0) amplitude = 1.0;
        } else if (strcmp(argv[i], "--sine") == 0) {
            signal_type = "sine";
            if (i + 2 < argc) {
                freq1 = atof(argv[++i]);
                duration = atof(argv[++i]);
            }
        } else if (strcmp(argv[i], "--sweep") == 0) {
            signal_type = "sweep";
            if (i + 3 < argc) {
                freq1 = atof(argv[++i]);
                freq2 = atof(argv[++i]);
                duration = atof(argv[++i]);
            }
        } else if (strcmp(argv[i], "--square") == 0) {
            signal_type = "square";
            if (i + 2 < argc) {
                freq1 = atof(argv[++i]);
                duration = atof(argv[++i]);
            }
        } else if (strcmp(argv[i], "--noise") == 0) {
            signal_type = "noise";
            if (i + 1 < argc) {
                duration = atof(argv[++i]);
            }
        } else if (strcmp(argv[i], "--silence") == 0) {
            signal_type = "silence";
            if (i + 1 < argc) {
                duration = atof(argv[++i]);
            }
        } else if (strcmp(argv[i], "--impulse") == 0) {
            signal_type = "impulse";
            if (i + 1 < argc) {
                duration = atof(argv[++i]);
            }
        } else if (strcmp(argv[i], "--multitone") == 0) {
            signal_type = "multitone";
            if (i + 1 < argc) {
                duration = atof(argv[++i]);
            }
        } else if (argv[i][0] != '-') {
            output_file = argv[i];
        }
    }

    if (signal_type == NULL || output_file == NULL || duration <= 0) {
        print_usage(argv[0]);
        return 1;
    }

    fp = fopen(output_file, "wb");
    if (!fp) {
        fprintf(stderr, "Error: Cannot create '%s'\n", output_file);
        return 1;
    }

    fprintf(stderr, "Generating %s signal (%.2f seconds)...\n", signal_type, duration);

    if (strcmp(signal_type, "sine") == 0) {
        generate_sine(fp, freq1, duration, amplitude);
    } else if (strcmp(signal_type, "sweep") == 0) {
        generate_sweep(fp, freq1, freq2, duration, amplitude);
    } else if (strcmp(signal_type, "square") == 0) {
        generate_square(fp, freq1, duration, amplitude);
    } else if (strcmp(signal_type, "noise") == 0) {
        generate_noise(fp, duration, amplitude);
    } else if (strcmp(signal_type, "silence") == 0) {
        generate_silence(fp, duration);
    } else if (strcmp(signal_type, "impulse") == 0) {
        generate_impulse(fp, duration, amplitude);
    } else if (strcmp(signal_type, "multitone") == 0) {
        generate_multitone(fp, duration, amplitude);
    }

    fclose(fp);
    fprintf(stderr, "Created: %s\n", output_file);
    return 0;
}
