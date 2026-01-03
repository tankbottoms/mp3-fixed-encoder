/*
 * layer3.h - MPEG Layer III encoder state declarations
 *
 * Copyright (C) 1998-2025 Mark Phillips. All rights reserved.
 *
 * See LICENSE for terms of use.
 */

#ifndef __LAYER3_H__
#define __LAYER3_H__

#include "types.h"

extern /* fixed15 */ short samples[2][1152];
extern fixed16 dctbuf[2][2][576];
extern fixed16 xr[576];
extern short ix[576];
extern unsigned int sxs[18];
extern struct granule_t granule[2][2];
extern int scalefac[21];
extern int slen0, slen1;

#endif /* __LAYER3_H__ */
