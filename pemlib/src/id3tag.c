/*
 * id3tag.c - ID3 tag writing support
 *
 * Copyright (C) 1998-2025 Mark Phillips. All rights reserved.
 *
 * This file implements ID3v1 and ID3v2.3 tag writing for MP3 files.
 *
 * ID3v1 Tag Structure (128 bytes at end of file):
 *   Offset  0:   "TAG" (3 bytes)
 *   Offset  3:   Title (30 bytes, space-padded)
 *   Offset 33:   Artist (30 bytes, space-padded)
 *   Offset 63:   Album (30 bytes, space-padded)
 *   Offset 93:   Year (4 bytes, ASCII)
 *   Offset 97:   Comment (28 bytes for v1.1)
 *   Offset 125:  Zero byte (marker for v1.1)
 *   Offset 126:  Track number (1 byte)
 *   Offset 127:  Genre code (1 byte)
 *
 * ID3v2.3 Tag Structure (at start of file):
 *   Header (10 bytes):
 *     "ID3" (3 bytes)
 *     Version (2 bytes: major, revision)
 *     Flags (1 byte)
 *     Size (4 bytes, synchsafe integer)
 *   Frames (variable):
 *     Frame ID (4 bytes, e.g., "TIT2")
 *     Size (4 bytes)
 *     Flags (2 bytes)
 *     Data (variable)
 *
 * See LICENSE for terms of use.
 */

#include <string.h>
#include <stdlib.h>
#include "id3tag.h"

/* Synchsafe integer encoding for ID3v2 */
static void encode_synchsafe(uint8_t *out, uint32_t value)
{
    out[0] = (value >> 21) & 0x7F;
    out[1] = (value >> 14) & 0x7F;
    out[2] = (value >> 7) & 0x7F;
    out[3] = value & 0x7F;
}

/* Write a big-endian 32-bit integer */
static void write_be32(uint8_t *out, uint32_t value)
{
    out[0] = (value >> 24) & 0xFF;
    out[1] = (value >> 16) & 0xFF;
    out[2] = (value >> 8) & 0xFF;
    out[3] = value & 0xFF;
}

/* Copy string with truncation and null-termination */
static void safe_strcpy(char *dst, const char *src, size_t max_len)
{
    if (!src) {
        dst[0] = '\0';
        return;
    }
    strncpy(dst, src, max_len);
    dst[max_len] = '\0';
}

/* Copy string with space padding (for ID3v1) */
static void pad_strcpy(uint8_t *dst, const char *src, size_t len)
{
    size_t src_len = src ? strlen(src) : 0;
    if (src_len > len) src_len = len;

    memset(dst, ' ', len);
    if (src && src_len > 0) {
        memcpy(dst, src, src_len);
    }
}

void id3_tag_init(id3_tag_t *tag)
{
    memset(tag, 0, sizeof(id3_tag_t));
    tag->genre = ID3V1_GENRE_UNKNOWN;
}

void id3_tag_set_title(id3_tag_t *tag, const char *title)
{
    safe_strcpy(tag->title, title, ID3V1_TITLE_LEN);
}

void id3_tag_set_artist(id3_tag_t *tag, const char *artist)
{
    safe_strcpy(tag->artist, artist, ID3V1_ARTIST_LEN);
}

void id3_tag_set_album(id3_tag_t *tag, const char *album)
{
    safe_strcpy(tag->album, album, ID3V1_ALBUM_LEN);
}

void id3_tag_set_year(id3_tag_t *tag, const char *year)
{
    safe_strcpy(tag->year, year, ID3V1_YEAR_LEN);
}

void id3_tag_set_comment(id3_tag_t *tag, const char *comment)
{
    safe_strcpy(tag->comment, comment, ID3V1_COMMENT_LEN);
}

void id3_tag_set_track(id3_tag_t *tag, uint8_t track)
{
    tag->track = track;
}

void id3_tag_set_genre(id3_tag_t *tag, uint8_t genre)
{
    tag->genre = genre;
}

int id3v1_write(FILE *fp, const id3_tag_t *tag)
{
    uint8_t buffer[ID3V1_TAG_SIZE];

    if (!fp || !tag) {
        return -1;
    }

    memset(buffer, 0, ID3V1_TAG_SIZE);

    /* Tag identifier */
    memcpy(buffer, "TAG", 3);

    /* Title, artist, album (space-padded) */
    pad_strcpy(buffer + 3, tag->title, 30);
    pad_strcpy(buffer + 33, tag->artist, 30);
    pad_strcpy(buffer + 63, tag->album, 30);

    /* Year (space-padded or null) */
    if (tag->year[0]) {
        pad_strcpy(buffer + 93, tag->year, 4);
    }

    /* Comment (28 bytes for v1.1) */
    pad_strcpy(buffer + 97, tag->comment, 28);

    /* ID3v1.1 track number */
    buffer[125] = 0;  /* Marker for v1.1 */
    buffer[126] = tag->track;

    /* Genre */
    buffer[127] = tag->genre;

    if (fwrite(buffer, 1, ID3V1_TAG_SIZE, fp) != ID3V1_TAG_SIZE) {
        return -1;
    }

    return 0;
}

/* Write a text frame (TIT2, TPE1, TALB, etc.) */
static size_t write_text_frame(uint8_t *buffer, const char *frame_id,
                               const char *text)
{
    size_t text_len;
    size_t frame_size;

    if (!text || !text[0]) {
        return 0;
    }

    text_len = strlen(text);
    frame_size = 1 + text_len;  /* Encoding byte + text (no null terminator in ID3v2.3) */

    /* Frame header */
    memcpy(buffer, frame_id, 4);         /* Frame ID */
    write_be32(buffer + 4, frame_size);  /* Size (not synchsafe in v2.3) */
    buffer[8] = 0;                       /* Flags byte 1 */
    buffer[9] = 0;                       /* Flags byte 2 */

    /* Frame data */
    buffer[10] = 0;                      /* Encoding: ISO-8859-1 */
    memcpy(buffer + 11, text, text_len); /* Text content */

    return 10 + frame_size;
}

/* Write TRCK (track number) frame */
static size_t write_track_frame(uint8_t *buffer, uint8_t track)
{
    char track_str[8];

    if (track == 0) {
        return 0;
    }

    snprintf(track_str, sizeof(track_str), "%d", track);
    return write_text_frame(buffer, "TRCK", track_str);
}

/* Write TCON (content type/genre) frame */
static size_t write_genre_frame(uint8_t *buffer, uint8_t genre)
{
    char genre_str[8];

    if (genre == ID3V1_GENRE_UNKNOWN) {
        return 0;
    }

    /* Format: "(nn)" where nn is the genre number */
    snprintf(genre_str, sizeof(genre_str), "(%d)", genre);
    return write_text_frame(buffer, "TCON", genre_str);
}

size_t id3v2_size(const id3_tag_t *tag)
{
    size_t size = 10;  /* Header size */

    if (tag->title[0])  size += 10 + 1 + strlen(tag->title);
    if (tag->artist[0]) size += 10 + 1 + strlen(tag->artist);
    if (tag->album[0])  size += 10 + 1 + strlen(tag->album);
    if (tag->year[0])   size += 10 + 1 + strlen(tag->year);
    if (tag->comment[0]) size += 10 + 1 + strlen(tag->comment);
    if (tag->track)     size += 10 + 1 + 3;  /* Max "255" */
    if (tag->genre != ID3V1_GENRE_UNKNOWN) size += 10 + 1 + 5;  /* Max "(255)" */

    /* Add padding to align to 2KB boundary */
    size = ((size + 2047) / 2048) * 2048;

    return size;
}

int id3v2_render(uint8_t *buffer, size_t buffer_size, const id3_tag_t *tag)
{
    size_t offset = 10;  /* Start after header */
    size_t total_size;
    size_t data_size;

    if (!buffer || !tag) {
        return -1;
    }

    total_size = id3v2_size(tag);
    if (buffer_size < total_size) {
        return -1;
    }

    /* Clear buffer */
    memset(buffer, 0, total_size);

    /* Write frames */
    offset += write_text_frame(buffer + offset, "TIT2", tag->title);
    offset += write_text_frame(buffer + offset, "TPE1", tag->artist);
    offset += write_text_frame(buffer + offset, "TALB", tag->album);
    offset += write_text_frame(buffer + offset, "TYER", tag->year);
    offset += write_text_frame(buffer + offset, "COMM", tag->comment);
    offset += write_track_frame(buffer + offset, tag->track);
    offset += write_genre_frame(buffer + offset, tag->genre);

    /* Calculate data size (excluding header) */
    data_size = total_size - 10;

    /* Write header */
    memcpy(buffer, "ID3", 3);
    buffer[3] = 3;   /* Version 2.3 */
    buffer[4] = 0;   /* Revision 0 */
    buffer[5] = 0;   /* Flags: no unsync, no extended header, not experimental */
    encode_synchsafe(buffer + 6, data_size);

    return (int)total_size;
}

int id3v2_write(FILE *fp, const id3_tag_t *tag)
{
    uint8_t *buffer;
    size_t size;
    int result;

    if (!fp || !tag) {
        return -1;
    }

    size = id3v2_size(tag);
    buffer = (uint8_t *)malloc(size);
    if (!buffer) {
        return -1;
    }

    result = id3v2_render(buffer, size, tag);
    if (result < 0) {
        free(buffer);
        return -1;
    }

    if (fwrite(buffer, 1, size, fp) != size) {
        free(buffer);
        return -1;
    }

    free(buffer);
    return 0;
}
