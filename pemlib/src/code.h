/*
 * code.h - Encoder channel processing interface
 *
 * Copyright (C) 1998-2025 Mark Phillips. All rights reserved.
 *
 * See LICENSE for terms of use.
 */

#ifndef __CODE_H__
#define __CODE_H__

void init_code(void);
void code(int ch, struct granule_t *gi, int mean_bits);

#endif /* __CODE_H__ */
