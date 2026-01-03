/* codec_workspace.h - Memory workspace definitions for standalone build
 *
 * In the original embedded system, FAST_DECODE_ADDR pointed to a fixed
 * memory region (SRAM) for fast access. For desktop builds, we use
 * dynamically allocated memory instead.
 *
 * Copyright (C) 2001 Interactive Objects, Inc.
 * Standalone adaptation (C) 2025 Mark Phillips
 */

#ifndef __CODEC_WORKSPACE_H__
#define __CODEC_WORKSPACE_H__

#include <stdlib.h>

/* For desktop/standalone builds, FAST_DECODE_ADDR is set dynamically
 * via pem_alloc_workspace() before calling pem_init().
 * This extern declaration allows codec.c to reference the allocated memory.
 */
extern void *pem_workspace;

#define FAST_DECODE_ADDR pem_workspace

/* Function to allocate the encoder workspace */
void *pem_alloc_workspace(void);
void pem_free_workspace(void *workspace);

#endif /* __CODEC_WORKSPACE_H__ */
