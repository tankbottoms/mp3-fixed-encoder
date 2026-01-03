/*
 * huffman.h - Huffman encoding table structures
 *
 * Copyright (C) 1998-2025 Mark Phillips. All rights reserved.
 *
 * Defines the huffcodetab structure used for Huffman table metadata.
 *
 * See LICENSE for terms of use.
 */

#ifndef __HUFFMAN_H__
#define __HUFFMAN_H__

struct huffcodetab {
  int len;       /* Table dimension (len x len entries). */
  int linbits;   /* Extra bits for escape codes. */
  int linmax;    /* Maximum value encodable with linbits. */
};

void init_hbits(void);

#endif /* __HUFFMAN_H__ */
