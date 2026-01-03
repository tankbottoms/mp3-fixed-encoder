/*
 * workspace.c - Memory workspace allocation for standalone encoder
 *
 * Copyright (C) 1998-2025 Mark Phillips. All rights reserved.
 *
 * Modern platform adaptation: Mark Phillips (2025)
 *
 * Provides dynamic memory allocation for the encoder's working memory,
 * replacing the fixed SRAM addresses used in embedded systems.
 *
 * See LICENSE for terms of use.
 */

#include <stdlib.h>
#include <string.h>
#include "memory.h"

/* Global pointer to the encoder workspace */
void *pem_workspace = NULL;

/* Allocate and initialize the encoder workspace */
void *pem_alloc_workspace(void)
{
    void *ws = malloc(sizeof(struct sram));
    if (ws) {
        memset(ws, 0, sizeof(struct sram));
    }
    pem_workspace = ws;
    return ws;
}

/* Free the encoder workspace */
void pem_free_workspace(void *workspace)
{
    if (workspace) {
        free(workspace);
    }
    if (pem_workspace == workspace) {
        pem_workspace = NULL;
    }
}
