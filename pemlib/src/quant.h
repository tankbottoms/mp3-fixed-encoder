/*
 * quant.h - Quantization and rate control interface
 *
 * Copyright (C) 1998-2025 Mark Phillips. All rights reserved.
 *
 * See LICENSE for terms of use.
 */

#ifndef __QUANT_H__
#define __QUANT_H__

int quant(int ch, int mean_bits, struct granule_t *gi);
void init_quant(void);

#endif /* __QUANT_H__ */
