/*
 * pem_main.h - Legacy encoder interface
 *
 * Copyright (C) 1998-2025 Mark Phillips. All rights reserved.
 *
 * See LICENSE for terms of use.
 */

#ifndef PEMMAIN_H_
#define PEMMAIN_H_

#ifdef __cplusplus
extern "C"
{
#endif

bool pem_init(int bitrate);
void pem_finish();

#ifdef __cplusplus
}
#endif

#endif /* PEMMAIN_H_ */
