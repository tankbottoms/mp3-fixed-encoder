/*
 * out.h - Bitstream output interface
 *
 * Copyright (C) 1998-2025 Mark Phillips. All rights reserved.
 *
 * See LICENSE for terms of use.
 */

#ifndef __OUT_H__
#define __OUT_H__

void out_init(void);
void out_fini(void);
void out_start(void);
void out(struct granule_t *gi);
void out_done(int padding);

#endif /* __OUT_H__ */
