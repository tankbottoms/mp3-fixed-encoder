/*
 * id3tag.h - ID3 tag writing support
 *
 * Copyright (C) 1998-2025 Mark Phillips. All rights reserved.
 *
 * This file provides functions to write ID3v1 and ID3v2 tags to MP3 files.
 *
 * ID3v1: 128-byte tag at the end of the file
 * ID3v2: Variable-length header at the start of the file
 *
 * See LICENSE for terms of use.
 */

#ifndef ID3TAG_H_
#define ID3TAG_H_

#include <stdint.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Maximum lengths for ID3v1 fields (null-terminated) */
#define ID3V1_TITLE_LEN    30
#define ID3V1_ARTIST_LEN   30
#define ID3V1_ALBUM_LEN    30
#define ID3V1_YEAR_LEN     4
#define ID3V1_COMMENT_LEN  28  /* 28 for v1.1 with track number */
#define ID3V1_TAG_SIZE     128

/* ID3v1 genre codes (partial list - common genres) */
#define ID3V1_GENRE_BLUES       0
#define ID3V1_GENRE_CLASSIC_ROCK 1
#define ID3V1_GENRE_COUNTRY     2
#define ID3V1_GENRE_DANCE       3
#define ID3V1_GENRE_DISCO       4
#define ID3V1_GENRE_FUNK        5
#define ID3V1_GENRE_GRUNGE      6
#define ID3V1_GENRE_HIPHOP      7
#define ID3V1_GENRE_JAZZ        8
#define ID3V1_GENRE_METAL       9
#define ID3V1_GENRE_NEWAGE      10
#define ID3V1_GENRE_OLDIES      11
#define ID3V1_GENRE_OTHER       12
#define ID3V1_GENRE_POP         13
#define ID3V1_GENRE_RNB         14
#define ID3V1_GENRE_RAP         15
#define ID3V1_GENRE_REGGAE      16
#define ID3V1_GENRE_ROCK        17
#define ID3V1_GENRE_TECHNO      18
#define ID3V1_GENRE_ELECTRONIC  52
#define ID3V1_GENRE_UNKNOWN     255

/* ID3 tag structure */
typedef struct id3_tag {
    char title[ID3V1_TITLE_LEN + 1];
    char artist[ID3V1_ARTIST_LEN + 1];
    char album[ID3V1_ALBUM_LEN + 1];
    char year[ID3V1_YEAR_LEN + 1];
    char comment[ID3V1_COMMENT_LEN + 1];
    uint8_t track;        /* Track number (0 = not set, 1-255) */
    uint8_t genre;        /* Genre code */
} id3_tag_t;

/*
 * Initialize an ID3 tag structure with empty values.
 */
void id3_tag_init(id3_tag_t *tag);

/*
 * Set ID3 tag fields.
 * Strings are copied and truncated if necessary.
 */
void id3_tag_set_title(id3_tag_t *tag, const char *title);
void id3_tag_set_artist(id3_tag_t *tag, const char *artist);
void id3_tag_set_album(id3_tag_t *tag, const char *album);
void id3_tag_set_year(id3_tag_t *tag, const char *year);
void id3_tag_set_comment(id3_tag_t *tag, const char *comment);
void id3_tag_set_track(id3_tag_t *tag, uint8_t track);
void id3_tag_set_genre(id3_tag_t *tag, uint8_t genre);

/*
 * Write ID3v1 tag to an open file.
 * The file position should be at the end of the MP3 data.
 * Returns 0 on success, -1 on error.
 */
int id3v1_write(FILE *fp, const id3_tag_t *tag);

/*
 * Write ID3v2.3 tag header and frames to a buffer.
 * Returns the number of bytes written, or -1 on error.
 * The buffer should be at least 256 bytes for typical tags.
 */
int id3v2_render(uint8_t *buffer, size_t buffer_size, const id3_tag_t *tag);

/*
 * Write ID3v2.3 tag to an open file.
 * The file should be positioned at the start.
 * Returns 0 on success, -1 on error.
 */
int id3v2_write(FILE *fp, const id3_tag_t *tag);

/*
 * Get the rendered size of an ID3v2 tag without writing it.
 * Returns the size in bytes including header and padding.
 */
size_t id3v2_size(const id3_tag_t *tag);

#ifdef __cplusplus
}
#endif

#endif /* ID3TAG_H_ */
