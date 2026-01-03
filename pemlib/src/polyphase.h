/*
 * polyphase.h - Polyphase analysis filter bank interface
 *
 * Copyright (C) 1998-2025 Mark Phillips. All rights reserved.
 *
 * See LICENSE for terms of use.
 */

#ifndef __POLYPHASE_H__
#define __POLYPHASE_H__

#include "types.h"

void init_polyphase(void);
void polyphase(const short *x, fixed16 *sa, fixed16 *sb);

#endif /* __POLYPHASE_H__ */
