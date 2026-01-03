/*
 * in.h - Audio input interface
 *
 * Copyright (C) 1998-2025 Mark Phillips. All rights reserved.
 *
 * See LICENSE for terms of use.
 */

#ifndef __IN_H__
#define __IN_H__

#include "types.h"

int in_open(char type);
void in_close(void);
void in_do(short *x, int n);

/* Byte-swap utilities for big-endian platforms. */
void swap2(unsigned short *x);
void swap4(unsigned long *x);

#endif /* __IN_H__ */
