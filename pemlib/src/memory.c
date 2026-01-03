/*
 * memory.c - Memory management and allocation
 *
 * Copyright (C) 1998-2025 Mark Phillips. All rights reserved.
 *
 * Original algorithm: Segher Boessenkool (1998-2002)
 * Embedded integration: Interactive Objects, Inc. (2001)
 * Modern platform adaptation: Mark Phillips (2025)
 *
 * This file provides the memory structures used by the encoder.
 *
 * On embedded targets (PEM_USE_SRAM defined):
 *   - sram_p points to dedicated fast SRAM region
 *   - the_fake_sram is not compiled in
 *
 * On desktop/modern platforms:
 *   - the_fake_sram provides the working memory
 *   - sram_p is set to point to it during initialization
 *
 * See LICENSE for terms of use.
 */

#include "memory.h"

#ifndef PEM_USE_SRAM
struct sram the_fake_sram;
#endif
struct dram dram;

struct sram *sram_p;