/* eresult.h - Compatibility layer for standalone pem encoder
 *
 * This header provides ERESULT type definitions compatible with the original
 * dadio/Interactive Objects codebase, allowing the pem encoder to build
 * standalone on desktop platforms.
 *
 * Copyright (C) 1998-2002 Segher Boessenkool
 * Copyright (C) 2001 Interactive Objects, Inc.
 * Standalone adaptation (C) 2025 Mark Phillips
 */

#ifndef __ERESULT_H__
#define __ERESULT_H__

/* ERESULT is a 32-bit error code with the following structure:
 * Bits 31-30: Severity (00=success, 01=info, 10=warning, 11=error)
 * Bits 29-16: Facility/Type code
 * Bits 15-0:  Error code
 */

typedef long ERESULT;

#define SEVERITY_SUCCESS 0
#define SEVERITY_INFO    1
#define SEVERITY_WARNING 2
#define SEVERITY_FAILED  3

#define MAKE_ERESULT(severity, type_id, code) \
    (((severity) << 30) | ((type_id) << 16) | (code))

#define SUCCEEDED(x) (((x) >> 30) == SEVERITY_SUCCESS)
#define FAILED(x)    (((x) >> 30) == SEVERITY_FAILED)

/* Standard success code */
#define ERESULT_SUCCESS 0

#endif /* __ERESULT_H__ */
