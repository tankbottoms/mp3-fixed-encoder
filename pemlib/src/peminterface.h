/*
 * peminterface.h - eCos embedded platform interface
 *
 * Copyright (C) 1998-2025 Mark Phillips. All rights reserved.
 *
 * Original algorithm: Segher Boessenkool (1998-2002)
 * Embedded integration: Interactive Objects, Inc. (2001)
 * Modern platform adaptation: Mark Phillips (2025)
 *
 * This header provides the interface for eCos-based embedded systems.
 * It includes eCos kernel headers and defines flash memory addresses
 * for audio sample storage on the EP7312 target platform.
 *
 * Note: This file is not used on modern desktop platforms.
 *
 * See LICENSE for terms of use.
 */

#ifndef __PEMINTERFACE_H__
#define __PEMINTERFACE_H__

#include <cyg/kernel/kapi.h>
#include <cyg/infra/diag.h>
#include <cyg/hal/hal_arch.h>

#ifdef __cplusplus
extern "C" {
#endif
#include "layer3.h"
#include "types.h"
#include "param.h"
#include "polyphase.h"
#include "psy.h"
#include "hybrid.h"
#include "code.h"
#include "huffman.h"
#include "tables.h"
#include "out.h"
#ifdef __cplusplus
};
#endif

/* Flash memory addresses for EP7312 GDB stub testing. */
#define FLASH_SAMPLES_START (char*)0xe0208000
#define FLASH_SAMPLES_LENGTH  705600

#endif /* __PEMINTERFACE_H__ */
