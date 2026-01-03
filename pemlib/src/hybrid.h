/*
 * hybrid.h - MDCT and aliasing butterfly interface
 *
 * Copyright (C) 1998-2025 Mark Phillips. All rights reserved.
 *
 * See LICENSE for terms of use.
 */

#ifndef __HYBRID_H__
#define __HYBRID_H__

#include "types.h"

void mdct(fixed16 *in1, fixed16 *in2, fixed16 *out, int nbands);
void butterfly(int nbands);

#endif /* __HYBRID_H__ */
