/* mp3_debug.c - Debug tool to parse MP3 side information
 * Parses a single MP3 frame and displays all side info fields
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

/* Bit reader state */
typedef struct {
    const uint8_t *data;
    int pos;  /* Current bit position */
} bitreader_t;

static void br_init(bitreader_t *br, const uint8_t *data) {
    br->data = data;
    br->pos = 0;
}

static uint32_t br_read(bitreader_t *br, int bits) {
    uint32_t result = 0;
    while (bits > 0) {
        int byte_pos = br->pos / 8;
        int bit_pos = br->pos % 8;
        int avail = 8 - bit_pos;
        int take = (bits < avail) ? bits : avail;
        uint8_t mask = ((1 << take) - 1) << (avail - take);
        uint8_t val = (br->data[byte_pos] & mask) >> (avail - take);
        result = (result << take) | val;
        bits -= take;
        br->pos += take;
    }
    return result;
}

int main(int argc, char **argv) {
    FILE *fp;
    uint8_t header[4];
    uint8_t sideinfo[32];
    bitreader_t br;
    int main_data_begin;
    int private_bits;
    int scfsi[2][4];
    int gr, ch;

    if (argc < 2) {
        fprintf(stderr, "Usage: %s <mp3_file>\n", argv[0]);
        return 1;
    }

    fp = fopen(argv[1], "rb");
    if (!fp) {
        fprintf(stderr, "Cannot open %s\n", argv[1]);
        return 1;
    }

    /* Read and parse first frame header */
    if (fread(header, 1, 4, fp) != 4) {
        fprintf(stderr, "Cannot read header\n");
        fclose(fp);
        return 1;
    }

    /* Verify sync word */
    if ((header[0] != 0xff) || ((header[1] & 0xe0) != 0xe0)) {
        fprintf(stderr, "Not a valid MP3 frame (sync=0x%02x%02x)\n", header[0], header[1]);
        fclose(fp);
        return 1;
    }

    printf("Frame Header (4 bytes): %02x %02x %02x %02x\n",
           header[0], header[1], header[2], header[3]);

    int version = (header[1] >> 3) & 3;
    int layer = (header[1] >> 1) & 3;
    int protection = header[1] & 1;
    int bitrate_idx = (header[2] >> 4) & 0xf;
    int sample_rate_idx = (header[2] >> 2) & 3;
    int padding = (header[2] >> 1) & 1;
    int mode = (header[3] >> 6) & 3;
    int mode_ext = (header[3] >> 4) & 3;

    printf("  MPEG version: %d (0=2.5, 2=2, 3=1)\n", version);
    printf("  Layer: %d (1=III, 2=II, 3=I)\n", layer);
    printf("  Protection: %d (1=no CRC, 0=CRC)\n", protection);
    printf("  Bitrate index: %d\n", bitrate_idx);
    printf("  Sample rate index: %d\n", sample_rate_idx);
    printf("  Padding: %d\n", padding);
    printf("  Channel mode: %d (0=stereo, 1=joint, 2=dual, 3=mono)\n", mode);
    printf("  Mode extension: %d\n", mode_ext);

    if (version != 3 || layer != 1) {
        fprintf(stderr, "Only MPEG-1 Layer III supported\n");
        fclose(fp);
        return 1;
    }

    int nch = (mode == 3) ? 1 : 2;
    printf("  Channels: %d\n", nch);
    printf("\n");

    /* Read side information (32 bytes for stereo, 17 for mono) */
    int sideinfo_size = (nch == 2) ? 32 : 17;
    if (fread(sideinfo, 1, sideinfo_size, fp) != sideinfo_size) {
        fprintf(stderr, "Cannot read side info\n");
        fclose(fp);
        return 1;
    }

    printf("Side Information (%d bytes):\n", sideinfo_size);
    printf("  Raw bytes: ");
    for (int i = 0; i < sideinfo_size; i++) {
        printf("%02x ", sideinfo[i]);
    }
    printf("\n\n");

    br_init(&br, sideinfo);

    /* Parse side info */
    main_data_begin = br_read(&br, 9);
    printf("main_data_begin: %d\n", main_data_begin);

    if (nch == 2) {
        private_bits = br_read(&br, 3);
    } else {
        private_bits = br_read(&br, 5);
    }
    printf("private_bits: %d\n", private_bits);

    /* SCFSI - Scale factor selection information */
    for (ch = 0; ch < nch; ch++) {
        for (int i = 0; i < 4; i++) {
            scfsi[ch][i] = br_read(&br, 1);
        }
        printf("scfsi[ch%d]: %d %d %d %d\n", ch,
               scfsi[ch][0], scfsi[ch][1], scfsi[ch][2], scfsi[ch][3]);
    }

    printf("\nGranule Information:\n");

    /* Granule info for each granule and channel */
    for (gr = 0; gr < 2; gr++) {
        for (ch = 0; ch < nch; ch++) {
            printf("\nGranule %d, Channel %d:\n", gr, ch);

            int part2_3_length = br_read(&br, 12);
            int big_values = br_read(&br, 9);
            int global_gain = br_read(&br, 8);
            int scalefac_compress = br_read(&br, 4);
            int window_switching_flag = br_read(&br, 1);

            printf("  part2_3_length: %d bits\n", part2_3_length);
            printf("  big_values: %d (max valid: 288)\n", big_values);
            if (big_values > 288) {
                printf("  *** ERROR: big_values > 288! ***\n");
            }
            printf("  global_gain: %d\n", global_gain);
            printf("  scalefac_compress: %d\n", scalefac_compress);
            printf("  window_switching_flag: %d\n", window_switching_flag);

            if (window_switching_flag) {
                int block_type = br_read(&br, 2);
                int mixed_block_flag = br_read(&br, 1);
                int table_select0 = br_read(&br, 5);
                int table_select1 = br_read(&br, 5);
                int subblock_gain0 = br_read(&br, 3);
                int subblock_gain1 = br_read(&br, 3);
                int subblock_gain2 = br_read(&br, 3);

                printf("  block_type: %d\n", block_type);
                printf("  mixed_block_flag: %d\n", mixed_block_flag);
                printf("  table_select: [%d, %d]\n", table_select0, table_select1);
                printf("  subblock_gain: [%d, %d, %d]\n",
                       subblock_gain0, subblock_gain1, subblock_gain2);
            } else {
                int table_select0 = br_read(&br, 5);
                int table_select1 = br_read(&br, 5);
                int table_select2 = br_read(&br, 5);
                int region0_count = br_read(&br, 4);
                int region1_count = br_read(&br, 3);

                printf("  table_select: [%d, %d, %d]\n",
                       table_select0, table_select1, table_select2);
                printf("  region0_count: %d\n", region0_count);
                printf("  region1_count: %d\n", region1_count);
            }

            int preflag = br_read(&br, 1);
            int scalefac_scale = br_read(&br, 1);
            int count1table_select = br_read(&br, 1);

            printf("  preflag: %d\n", preflag);
            printf("  scalefac_scale: %d\n", scalefac_scale);
            printf("  count1table_select: %d\n", count1table_select);
        }
    }

    printf("\nBit position after side info: %d (expected: %d)\n",
           br.pos, sideinfo_size * 8);

    fclose(fp);
    return 0;
}
