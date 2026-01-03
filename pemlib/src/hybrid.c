/*
 * hybrid.c - MDCT and aliasing reduction butterfly
 *
 * Copyright (C) 1998-2025 Mark Phillips. All rights reserved.
 *
 * Original algorithm: Segher Boessenkool (1998-2002)
 * Embedded integration: Interactive Objects, Inc. (2001)
 * LP64 fixes and modern platform adaptation: Mark Phillips (2025)
 *
 * This file implements the Modified Discrete Cosine Transform (MDCT) and
 * the aliasing reduction butterfly operations for MP3 encoding. The MDCT
 * transforms 576 time-domain samples from the polyphase filter bank into
 * frequency-domain coefficients for quantization.
 *
 * Key optimizations:
 *   - Fixed-point arithmetic using cfm() for 32x32->64->high32 multiply
 *   - Pre-computed cosine tables (costab) for DCT butterflies
 *   - Aliasing reduction coefficients (ca, cs) stored in ROM
 *
 * See LICENSE for terms of use.
 */

#include <math.h>
#define PI M_PI
#include "hybrid.h"
#include "memory.h"

#include <stdint.h>

#ifdef BIGENDIAN
union ll {
	int64_t x;
	struct {
		int32_t hi;
		int32_t lo;
	} xx;
};
#else
union ll {
	int64_t x;
	struct {
		int32_t lo;
		int32_t hi;
	} xx;
};
#endif


/* extern */ inline int cfm(int a1, int a2) {
	/* Fixed-point multiply returning high 32 bits of 64-bit product */
	int64_t product = (int64_t)a1 * a2;
	return (int32_t)(product >> 32);
}


#define cfmh(alo, ahi, a1, a2) do { \
	int64_t _prod = (int64_t)(a1) * (a2); \
	ahi = (int32_t)(_prod >> 32); \
	alo = (int32_t)(_prod & 0xFFFFFFFF); \
} while(0)


#define cfma(alo, ahi, a1, a2) do { \
	int64_t _acc = ((int64_t)(uint32_t)(alo)) | ((int64_t)(ahi) << 32); \
	_acc += (int64_t)(a1) * (a2); \
	ahi = (int32_t)(_acc >> 32); \
	alo = (int32_t)(_acc & 0xFFFFFFFF); \
} while(0)


#define window_bf_0(i) \
_F_ do { \
	int a, b; \
	b = -cfm(iwinn[i],   band0[(i)*32]) + cfm(iwinn[i+9], band0[(17-i)*32]); \
	a =  cfm(iwinn[i+9], band1[(i)*32]) + cfm(iwinn[i],   band1[(17-i)*32]); \
	work[i]   = cfm(a, itan0[i]) + (b >> 1); \
	work[i+9] = cfm(b, itan0[i]) - (a >> 1); \
} while (0)


#define window_bf_1a(i) \
_F_ do { \
	int a, b; \
	b = -cfm(iwinn[i],   band0[(i)*32]) - cfm(iwinn[i+9], band0[(17-i)*32]); \
	a =  cfm(iwinn[i+9], band1[(i)*32]) - cfm(iwinn[i],   band1[(17-i)*32]); \
	work[i]   = cfm(a, itan0[i]) + (b >> 1); \
	work[i+9] = cfm(b, itan0[i]) - (a >> 1); \
} while (0)


#define window_bf_1b(i) \
_F_ do { \
	int a, b; \
	b =  cfm(iwinn[i],   band0[(i)*32]) + cfm(iwinn[i+9], band0[(17-i)*32]); \
	a = -cfm(iwinn[i+9], band1[(i)*32]) + cfm(iwinn[i],   band1[(17-i)*32]); \
	work[i]   = cfm(a, itan0[i]) + (b >> 1); \
	work[i+9] = cfm(b, itan0[i]) - (a >> 1); \
} while (0)


static void mdct_window_0(const fixed16 *band0, const fixed16 *band1, fixed16 *work)
{
#ifdef PEM_DEBUG
{
	static int dbg_cnt = 0;
	if (dbg_cnt < 2) {
		/* Trace one window calculation manually */
		int a, b;
		int i = 0;
		fprintf(stderr, "    window_bf_0[%d]: band0[0]=%d band0[%d]=%d band1[0]=%d band1[%d]=%d\n",
			i, band0[0], (17-i)*32, band0[(17-i)*32], band1[0], (17-i)*32, band1[(17-i)*32]);
		fprintf(stderr, "    iwinn[%d]=%d iwinn[%d]=%d itan0[%d]=%d\n",
			i, iwinn[i], i+9, iwinn[i+9], i, itan0[i]);
		/* Manual computation */
		long long prod1 = (long long)iwinn[i] * band0[0];
		long long prod2 = (long long)iwinn[i+9] * band0[(17-i)*32];
		long long prod3 = (long long)iwinn[i+9] * band1[0];
		long long prod4 = (long long)iwinn[i] * band1[(17-i)*32];
		fprintf(stderr, "    prod1=%lld prod2=%lld prod3=%lld prod4=%lld\n", prod1, prod2, prod3, prod4);
		b = -cfm(iwinn[i], band0[0]) + cfm(iwinn[i+9], band0[(17-i)*32]);
		a = cfm(iwinn[i+9], band1[0]) + cfm(iwinn[i], band1[(17-i)*32]);
		fprintf(stderr, "    a=%d b=%d\n", a, b);
		fprintf(stderr, "    cfm(a, itan0[%d])=%d\n", i, cfm(a, itan0[i]));
		work[i]   = cfm(a, itan0[i]) + (b >> 1);
		work[i+9] = cfm(b, itan0[i]) - (a >> 1);
		fprintf(stderr, "    work[%d]=%d work[%d]=%d\n", i, work[i], i+9, work[i+9]);
		dbg_cnt++;
	}
}
#endif
	window_bf_0(0);
	window_bf_0(1);
	window_bf_0(2);
	window_bf_0(3);
	window_bf_0(4);
	window_bf_0(5);
	window_bf_0(6);
	window_bf_0(7);
	window_bf_0(8);
}


static void mdct_window_1(const fixed16 *band0, const fixed16 *band1, fixed16 *work)
{
	window_bf_1a(0);
	window_bf_1b(1);
	window_bf_1a(2);
	window_bf_1b(3);
	window_bf_1a(4);
	window_bf_1b(5);
	window_bf_1a(6);
	window_bf_1b(7);
	window_bf_1a(8);
}


#ifdef PEM_ARM_ASM

#define ADD(d, a, b) __asm__ __volatile__( \
	"\tadd	%0, %1, %2" \
	: "=r"(d) \
	: "r"(a), "r"(b) \
)


#define ADD_SR1(d, a, b) __asm__ __volatile__( \
	"\tadd	%0, %1, %2, asr #1" \
	: "=r"(d) \
	: "r"(a), "r"(b) \
)


#define SUB(d, a, b) __asm__ __volatile__( \
	"\tsub	%0, %1, %2" \
	: "=r"(d) \
	: "r"(a), "r"(b) \
)


#define SUB_SR1(d, a, b) __asm__ __volatile__( \
	"\tsub	%0, %1, %2, asr #1" \
	: "=r"(d) \
	: "r"(a), "r"(b) \
)


#define RSB_SR1(d, a, b) __asm__ __volatile__( \
	"\trsb	%0, %1, %2, asr #1" \
	: "=r"(d) \
	: "r"(a), "r"(b) \
)


#define NEG(d) __asm__ __volatile__( \
	"\trsb	%0, %0, #0" \
	: "+r"(d) \
)


#define SMULL(dlo, dhi, a, b) __asm__ __volatile__( \
	"\tsmull	%0, %1, %2, %3" \
	: "=r"(dlo), "=r"(dhi) \
	: "r"(a), "r"(b) \
)


#define SMLAL(dlo, dhi, a, b) __asm__ __volatile__( \
	"\tsmlal	%0, %1, %2, %3" \
	: "+r"(dlo), "+r"(dhi) \
	: "r"(a), "r"(b) \
)


#define OUT1(n, a) __asm__ __volatile__( \
	"\tstr	%0, [%1, %2]" \
	: \
	: "r"(a), "r"(out), "i"(4*n) \
)


#define LOAD3(x, a, b, c) __asm__ __volatile__ ( \
	"\tldmia	%3, {%0, %1, %2}" \
	: "=r"(a), "=r"(b), "=r"(c) \
	: "r"(x) \
); \


#define LOAD4(x, a, b, c, d) __asm__ __volatile__ ( \
	"\tldmia	%4, {%0, %1, %2, %3}" \
	: "=r"(a), "=r"(b), "=r"(c), "=r"(d) \
	: "r"(x) \
); \


#define LOAD9(x, a, b, c, d, e, f, g, h, i) __asm__ __volatile__ ( \
	"\tldmia	%9, {%0, %1, %2, %3, %4, %5, %6, %7, %8}" \
	: "=r"(a), "=r"(b), "=r"(c), "=r"(d), "=r"(e), "=r"(f), "=r"(g), "=r"(h), "=r"(i) \
	: "r"(x) \
); \


static void mdct_core_0(const fixed16 *work, int *out)
{
	int r0, r1, r2, r3;

	register int r4 __asm__("r2");
	register int r5 __asm__("r3");
	register int r6 __asm__("r4");
	register int r7 __asm__("r5");
	register int r8 __asm__("r6");
	register int r9 __asm__("r7");
	register int r10 __asm__("r8");
	register int r11 __asm__("r9");
	register int r12 __asm__("r10");
//don't touch r11; _can_ use r12, however.

	// 1st part

	LOAD9(work, r4, r5, r6, r7, r8, r9, r10, r11, r12);
//	r4 = work[0];
//	r5 = work[1];
//	r6 = work[2];
//	r7 = work[3];
//	r8 = work[4];
//	r9 = work[5];
//	r10 = work[6];
//	r11 = work[7];
//	r12 = work[8];

	ADD(r4, r4, r12);					// 4 5 6 7 8 9 10 11
	ADD(r5, r5, r11);					// 4 5 6 7 8 9 10
	ADD(r6, r6, r10);					// 4 5 6 7 8 9
	ADD(r7, r7, r9);					// 4 5 6 7 8

	SUB(r0, r5, r8);					// 0 4 5 6 7 8
	ADD(r1, r4, r6);					// 0 1 4 5 6 7 8
	SUB(r1, r1, r7);					// 0 1 4 5 6 7 8
	SUB(r2, r1, r0);					// 0 1 2 4 5 6 7 8
_F_	r2 = r2 >> 1;						// 0 1 4 5 6 7 8
	OUT1(17, r2);

	ADD_SR1(r5, r8, r5);					// 0 1 4 5 6 7
	ADD_SR1(r0, r0, r1);					// 0 4 5 6 7

	LOAD3(trig+1, r8, r9, r10);

	SMULL(r11, r2, r8, r6);					// 0 2 4 5 6 7 8 9 10 11	low is r11
	SMLAL(r11, r2, r9, r7);					// 0 2 4 5 6 7 8 9 10 11	low is r11
	SMLAL(r11, r2, r10, r4);				// 0 2 4 5 6 7 8 9 10		low is r11
	RSB_SR1(r2, r2, r5);					// 0 2 4 5 6 7 8 9 10
	NEG(r10);						// 0 2 4 5 6 7 8 9 10
	SMULL(r11, r1, r8, r7);					// 0 1 2 4 5 6 7 8 9 10 11	low is r11
	SMLAL(r11, r1, r9, r4);					// 0 1 2 4 5 6 7 8 9 10 11	low is r11
	SMLAL(r11, r1, r10, r6);				// 0 1 2 4 5 6 7 8 9 10		low is r11
	ADD_SR1(r1, r1, r5);					// 0 1 2 4 5 6 7 8 9 10
	NEG(r9);						// 0 1 2 4 5 6 7 8 9 10
	SMULL(r11, r3, r8, r4);					// 0 1 2 3 5 6 7 9 10 11	low is r11
	SMLAL(r11, r3, r9, r6);					// 0 1 2 3 5 7 10 11		low is r11
	SMLAL(r11, r3, r10, r7);				// 0 1 2 3 5			low is r11
	SUB_SR1(r3, r3, r5);					// 0 1 2 3

	// 2nd part
		
	LOAD4(work+9, r4, r5, r6, r7);
	LOAD4(work+14, r8, r9, r10, r11);

	SUB(r4, r11, r4);					// 0 1 2 3 4 5 6 7 8 9 10
	SUB(r5, r10, r5);					// 0 1 2 3 4 5 6 7 8 9
	SUB(r6, r9, r6);					// 0 1 2 3 4 5 6 7 8
	SUB(r7, r8, r7);					// 0 1 2 3 4 5 6 7

_F_	r8 = trig[0];						// 0 1 2 3 4 5 6 7 8

	SUB(r9, r4, r6);					// 0 1 2 3 4 5 6 7 8 9
	SUB(r9, r9, r7);					// 0 1 2 3 4 5 6 7 8 9
	SMULL(r11, r9, r8, r9);					// 0 1 2 3 4 5 6 7 8 9		low is r11
	SUB_SR1(r10, r9, r0);					// 0 1 2 3 4 5 6 7 8 9 10
	ADD_SR1(r9, r9, r0);					// 1 2 3 4 5 6 7 8 9 10

	OUT1(5, r9);
	OUT1(6, r10);						// 1 2 3 4 5 6 7 8

	SMULL(r11, r5, r8, r5);					// 1 2 3 4 5 6 7		low is r11

	LOAD3(trig+4, r8, r9, r10)

	SMULL(r11, r0, r8, r6);					// 0 1 2 3 4 5 6 7 8 9 10 11	low is r11
	SMLAL(r11, r0, r9, r7);					// 0 1 2 3 4 5 6 7 8 9 10 11	low is r11
	SMLAL(r11, r0, r10, r4);				// 0 1 2 3 4 5 6 7 8 9 10	low is r11
	SUB(r0, r0, r5);					// 0 1 2 3 4 5 6 7 8 9 10
	ADD(r11, r0, r3);					// 0 1 2 3 4 5 6 7 8 9 10 11
	SUB(r12, r0, r3);					// 1 2 4 5 6 7 8 9 10 11 12
	OUT1(13, r11);
	OUT1(14, r12);						// 1 2 4 5 6 7 8 9 10

	NEG(r9);						// 1 2 4 5 6 7 8 9 10
	SMULL(r11, r0, r8, r4);					// 0 1 2 4 5 6 7 8 9 10 11	low is r11
	SMLAL(r11, r0, r9, r6);					// 0 1 2 4 5 6 7 8 9 10 11	low is r11
	SMLAL(r11, r0, r10, r7);				// 0 1 2 4 5 6 7 8 9 10		low is r11
	ADD(r0, r0, r5);					// 0 1 2 4 5 6 7 8 9 10
	ADD(r11, r0, r1);					// 0 1 2 4 5 6 7 8 9 10 11
	SUB(r12, r0, r1);					// 2 4 5 6 7 8 9 10 11 12

	OUT1(1, r11);
	OUT1(2, r12);						// 2 4 5 6 7 8 9 10

	NEG(r10);						// 2 4 5 6 7 8 9 10
	SMULL(r11, r0, r8, r7);					// 0 2 4 5 6 9 10 11		low is r11
	SMLAL(r11, r0, r9, r4);					// 0 2 5 6 10 11		low is r11
	SMLAL(r11, r0, r10, r6);				// 0 2 5			low is r11
	SUB(r0, r0, r5);					// 0 2
	ADD(r11, r0, r2);					// 0 2 11
	SUB(r12, r0, r2);					// 11 12
	OUT1(9, r11);
	OUT1(10, r12);						// - o -
}


static void mdct_core_1(const fixed16 *work, fixed16 *out)
{
	int r0, r1, r2, r3;

	register int r4 __asm__("r2");
	register int r5 __asm__("r3");
	register int r6 __asm__("r4");
	register int r7 __asm__("r5");
	register int r8 __asm__("r6");
	register int r9 __asm__("r7");
	register int r10 __asm__("r8");
	register int r11 __asm__("r9");
	register int r12 __asm__("r10");
//	int r10;
//	int r11;
//	int r12;

	LOAD9(work+9, r4, r5, r6, r7, r8, r9, r10, r11, r12);
//	r4 = work[9];
//	r5 = work[10];
//	r6 = work[11];
//	r7 = work[12];
//	r8 = work[13];
//	r9 = work[14];
//	r10 = work[15];
//	r11 = work[16];
//	r12 = work[17];

	ADD(r4, r4, r12);
	ADD(r5, r5, r11);
	ADD(r6, r6, r10);
	ADD(r7, r7, r9);

	ADD(r0, r5, r8);
	ADD(r1, r4, r6);
	ADD(r1, r1, r7);
	ADD(r2, r0, r1);
_F_	out[0]  = r2 >> 1;

	RSB_SR1(r0, r0, r1);
	SUB_SR1(r5, r8, r5);

	LOAD3(trig+1, r8, r9, r10);

	NEG(r9);
	SMULL(r11, r1, r4, r8);
	SMLAL(r11, r1, r6, r9);
	SMLAL(r11, r1, r7, r10);
	SUB_SR1(r1, r1, r5);
	SMULL(r11, r2, r6, r8);
	SMLAL(r11, r2, r7, r9);
	SMLAL(r11, r2, r4, r10);
	RSB_SR1(r2, r2, r5);
	SMULL(r11, r3, r7, r8);
	SMLAL(r11, r3, r4, r9);
	SMLAL(r11, r3, r6, r10);
	RSB_SR1(r3, r3, r5);

	LOAD4(work, r4, r5, r6, r7);
	LOAD4(work+5, r8, r9, r10, r11);
//	r4 = work[0];
//	r5 = work[1];
//	r6 = work[2];
//	r7 = work[3];
//	r8 = work[5];
//	r9 = work[6];
//	r10 = work[7];
//	r11 = work[8];

	SUB(r4, r4, r11);
	SUB(r5, r5, r10);
	SUB(r6, r6, r9);
	SUB(r7, r7, r8);

_F_	r8 = trig[0];

	SUB(r9, r6, r4);
	SUB(r9, r9, r7);
	SMULL(r10, r9, r8, r9);
	RSB_SR1(r10, r9, r0);
	ADD_SR1(r9, r9, r0);
_F_	out[11] = r9;
_F_	out[12] = r10;

	SMULL(r10, r5, r8, r5);

	LOAD3(trig+4, r8, r9, r10);

	SMULL(r11, r0, r7, r8);
	SMLAL(r11, r0, r4, r9);
	SMLAL(r11, r0, r6, r10);
	SUB(r0, r0, r5);
	ADD(r11, r2, r0);
	SUB(r12, r2, r0);
_F_	out[7]  = r11;
_F_	out[8]  = r12;

	NEG(r9);
	SMULL(r11, r0, r6, r8);
	SMLAL(r11, r0, r7, r9);
	SMLAL(r11, r0, r4, r10);
	ADD(r0, r0, r5);
	SUB(r11, r1, r0);
	ADD(r12, r1, r0);
_F_	out[3]  = r11;
_F_	out[4]  = r12;

	NEG(r10);
	SMULL(r11, r0, r4, r8);
	SMLAL(r11, r0, r6, r9);
	SMLAL(r11, r0, r7, r10);
	SUB(r0, r5, r0);
	ADD(r11, r3, r0);
	SUB(r12, r3, r0);
_F_	out[15] = r11;
_F_	out[16] = r12;
_F_
}

#else

static void mdct_core_0(const fixed16 *work, fixed16 *out)
{
	int r0, r1, r2, r3, r4, r5, r6, r7, r8, r9, r10, r11, r12;

	// 1st part

_F_	r4 = work[0];
_F_	r5 = work[1];
_F_	r6 = work[2];
_F_	r7 = work[3];
_F_	r8 = work[4];
_F_	r9 = work[5];
_F_	r10 = work[6];
_F_	r11 = work[7];
_F_	r12 = work[8];						// 4 5 6 7 8 9 10 11 12

_F_	r4 = r4 + r12;						// 4 5 6 7 8 9 10 11
_F_	r5 = r5 + r11;						// 4 5 6 7 8 9 10
_F_	r6 = r6 + r10;						// 4 5 6 7 8 9
_F_	r7 = r7 + r9;						// 4 5 6 7 8

_F_	r0 = r5 - r8;						// 0 4 5 6 7 8
_F_	r1 = r4 + r6;						// 0 1 4 5 6 7 8
_F_	r1 = r1 - r7;						// 0 1 4 5 6 7 8
_F_	r2 = r1 - r0;						// 0 1 2 4 5 6 7 8
_F_	out[17] = r2 >> 1;					// 0 1 4 5 6 7 8

_F_	r5 = (r5 >> 1) + r8;					// 0 1 4 5 6 7
_F_	r0 = r0 + (r1 >> 1);					// 0 4 5 6 7

_F_	r8 = trig[1];						// 0 4 5 6 7 8
_F_	r9 = trig[2];						// 0 4 5 6 7 8 9
_F_	r10 = trig[3];						// 0 4 5 6 7 8 9 10

_F_	cfmh(r11, r2, r6, r8);					// 0 2 4 5 6 7 8 9 10 11	low is r11
_F_	cfma(r11, r2, r7, r9);					// 0 2 4 5 6 7 8 9 10 11	low is r11
_F_	cfma(r11, r2, r4, r10);					// 0 2 4 5 6 7 8 9 10		low is r11
_F_	r2 = (r5 >> 1) - r2;					// 0 2 4 5 6 7 8 9 10
_F_	r10 = -r10;						// 0 2 4 5 6 7 8 9 10
_F_	cfmh(r11, r1, r7, r8);					// 0 1 2 4 5 6 7 8 9 10 11	low is r11
_F_	cfma(r11, r1, r4, r9);					// 0 1 2 4 5 6 7 8 9 10 11	low is r11
_F_	cfma(r11, r1, r6, r10);					// 0 1 2 4 5 6 7 8 9 10		low is r11
_F_	r1 = (r5 >> 1) + r1;					// 0 1 2 4 5 6 7 8 9 10
_F_	r9 = -r9;						// 0 1 2 4 5 6 7 8 9 10
_F_	cfmh(r11, r3, r4, r8);					// 0 1 2 3 5 6 7 9 10 11	low is r11
_F_	cfma(r11, r3, r6, r9);					// 0 1 2 3 5 7 10 11		low is r11
_F_	cfma(r11, r3, r7, r10);					// 0 1 2 3 5			low is r11
_F_	r3 = r3 - (r5 >> 1);					// 0 1 2 3

	// 2nd part
		
_F_	r4 = work[9];
_F_	r5 = work[10];
_F_	r6 = work[11];
_F_	r7 = work[12];
_F_	r8 = work[14];
_F_	r9 = work[15];
_F_	r10 = work[16];
_F_	r11 = work[17];						// 0 1 2 3 4 5 6 7 8 9 10 11

_F_	r4 = r11 - r4;						// 0 1 2 3 4 5 6 7 8 9 10
_F_	r5 = r10 - r5;						// 0 1 2 3 4 5 6 7 8 9
_F_	r6 = r9 - r6;						// 0 1 2 3 4 5 6 7 8
_F_	r7 = r8 - r7;						// 0 1 2 3 4 5 6 7

_F_	r8 = trig[0];						// 0 1 2 3 4 5 6 7 8

_F_	r9 = r4 - r6;						// 0 1 2 3 4 5 6 7 8 9
_F_	r9 = r9 - r7;						// 0 1 2 3 4 5 6 7 8 9
_F_	r9 = cfm(r9, r8);						// 0 1 2 3 4 5 6 7 8 9		low is r11
_F_	r10 = r9 - (r0 >> 1);						// 0 1 2 3 4 5 6 7 8 9 10
_F_	r9 = (r0 >> 1) + r9;						// 1 2 3 4 5 6 7 8 9 10

_F_	out[5]  = r9;
_F_	out[6]  = r10;						// 1 2 3 4 5 6 7 8

_F_	r5 = cfm(r5, r8);						// 1 2 3 4 5 6 7		low is r11

_F_	r8 = trig[4];						// 1 2 3 4 5 6 7 8
_F_	r9 = trig[5];						// 1 2 3 4 5 6 7 8 9
_F_	r10 = trig[6];						// 1 2 3 4 5 6 7 8 9 10

_F_	cfmh(r11, r0, r6, r8);						// 0 1 2 3 4 5 6 7 8 9 10 11	low is r11
_F_	cfma(r11, r0, r7, r9);					// 0 1 2 3 4 5 6 7 8 9 10 11	low is r11
_F_	cfma(r11, r0, r4, r10);					// 0 1 2 3 4 5 6 7 8 9 10	low is r11
_F_	r0 = r0 - r5;						// 0 1 2 3 4 5 6 7 8 9 10
_F_	r11 = r0 + r3;						// 0 1 2 3 4 5 6 7 8 9 10 11
_F_	r12 = r0 - r3;						// 1 2 4 5 6 7 8 9 10 11 12
_F_	out[13] = r11;
_F_	out[14] = r12;						// 1 2 4 5 6 7 8 9 10

_F_	r9 = -r9;						// 1 2 4 5 6 7 8 9 10
_F_	cfmh(r11, r0, r4, r8);						// 0 1 2 4 5 6 7 8 9 10 11	low is r11
_F_	cfma(r11, r0, r6, r9);					// 0 1 2 4 5 6 7 8 9 10 11	low is r11
_F_	cfma(r11, r0, r7, r10);					// 0 1 2 4 5 6 7 8 9 10		low is r11
_F_	r0 = r5 + r0;						// 0 1 2 4 5 6 7 8 9 10
_F_	r11 = r0 + r1;						// 0 1 2 4 5 6 7 8 9 10 11
_F_	r12 = r0 - r1;						// 2 4 5 6 7 8 9 10 11 12

_F_	out[1]  = r11;
_F_	out[2]  = r12;						// 2 4 5 6 7 8 9 10

_F_	r10 = -r10;						// 2 4 5 6 7 8 9 10
_F_	cfmh(r11, r0, r7, r8);						// 0 2 4 5 6 9 10 11		low is r11
_F_	cfma(r11, r0, r4, r9);					// 0 2 5 6 10 11		low is r11
_F_	cfma(r11, r0, r6, r10);					// 0 2 5			low is r11
_F_	r0 = r0 - r5;						// 0 2
_F_	r11 = r0 + r2;						// 0 2 11
_F_	r12 = r0 - r2;						// 11 12
_F_	out[9]  = r11;
_F_	out[10] = r12;						// - o -
_F_
}


static void mdct_core_1(const fixed16 *work, fixed16 *out)
{
	int r0, r1, r2, r3, r4, r5, r6, r7, r8, r9, r10, r11, r12;

_F_	r4 = work[9];
_F_	r5 = work[10];
_F_	r6 = work[11];
_F_	r7 = work[12];
_F_	r8 = work[13];
_F_	r9 = work[14];
_F_	r10 = work[15];
_F_	r11 = work[16];
_F_	r12 = work[17];

_F_	r4 = r12 + r4;
_F_	r5 = r11 + r5;
_F_	r6 = r10 + r6;
_F_	r7 = r9 + r7;

_F_	r0 = r5 + r8;
_F_	r1 = r4 + r6;
_F_	r1 = r1 + r7;
_F_	r2 = r1 + r0;
_F_	out[0]  = r2 >> 1;

_F_	r0 = (r1 >> 1) - r0;
_F_	r5 = (r8 >> 1) - (r5 >> 2);

_F_	r8 = trig[1];
_F_	r9 = trig[2];
_F_	r10 = trig[3];

_F_	r9 = -r9;
_F_	cfmh(r11, r1, r4, r8);
_F_	cfma(r11, r1, r6, r9);
_F_	cfma(r11, r1, r7, r10);
_F_	r1 = r1 - r5;
_F_	cfmh(r11, r2, r6, r8);
_F_	cfma(r11, r2, r7, r9);
_F_	cfma(r11, r2, r4, r10);
_F_	r2 = r5 - r2;
_F_	cfmh(r11, r3, r7, r8);
_F_	cfma(r11, r3, r4, r9);
_F_	cfma(r11, r3, r6, r10);
_F_	r3 = r5 - r3;

_F_	r4 = work[0];
_F_	r5 = work[1];
_F_	r6 = work[2];
_F_	r7 = work[3];
_F_	r8 = work[5];
_F_	r9 = work[6];
_F_	r10 = work[7];
_F_	r11 = work[8];

_F_	r4 = r4 - r11;
_F_	r5 = r5 - r10;
_F_	r6 = r6 - r9;
_F_	r7 = r7 - r8;

_F_	r8 = trig[0];

_F_	r9 = r6 - r4;
_F_	r9 = r9 - r7;
_F_	r9 = cfm(r9, r8);
_F_	r10 = (r0 >> 1) - r9;
_F_	r9 = (r0 >> 1) + r9;
_F_	out[11] = r9;
_F_	out[12] = r10;

_F_	r5 = cfm(r5, r8);

_F_	r8 = trig[4];
_F_	r9 = trig[5];
_F_	r10 = trig[6];

_F_	cfmh(r11, r0, r7, r8);
_F_	cfma(r11, r0, r4, r9);
_F_	cfma(r11, r0, r6, r10);
_F_	r0 = r0 - r5;
_F_	r11 = r2 + r0;
_F_	r12 = r2 - r0;
_F_	out[7]  = r11;
_F_	out[8]  = r12;

_F_	r9 = -r9;
_F_	cfmh(r11, r0, r6, r8);
_F_	cfma(r11, r0, r7, r9);
_F_	cfma(r11, r0, r4, r10);
_F_	r0 = r0 + r5;
_F_	r11  = r1 - r0;
_F_	r12  = r1 + r0;
_F_	out[3]  = r11;
_F_	out[4]  = r12;

_F_	r10 = -r10;
_F_	cfmh(r11, r0, r4, r8);
_F_	cfma(r11, r0, r6, r9);
_F_	cfma(r11, r0, r7, r10);
_F_	r0 =  r5 - r0;
_F_	r11 = r3 + r0;
_F_	r12 = r3 - r0;
_F_	out[15] = r11;
_F_	out[16] = r12;
_F_
}

#endif


static void mdct_one(const fixed16 *band0, const fixed16 *band1, fixed16 *out, int reverseflag)
{
	fixed16 work[18];
	int i;

	/* Initialize work to zero to detect if window function doesn't execute */
	for (i = 0; i < 18; i++) work[i] = 0xDEADBEEF;

#ifdef PEM_DEBUG
	static int mdct_one_dbg = 0;
	if (mdct_one_dbg < 2) {
		fprintf(stderr, "  mdct_one[%d] BEFORE window: work[0]=%d band0[0]=%d band1[0]=%d reverseflag=%d\n",
			mdct_one_dbg, work[0], band0[0], band1[0], reverseflag);
	}
#endif

	if (!reverseflag)
		mdct_window_0(band0, band1, work);
	else
		mdct_window_1(band0, band1, work);

#ifdef PEM_DEBUG
	if (mdct_one_dbg < 2) {
		fprintf(stderr, "  mdct_one[%d] AFTER window: work[0..8] = %d %d %d %d %d %d %d %d %d\n",
			mdct_one_dbg, work[0], work[1], work[2], work[3], work[4], work[5], work[6], work[7], work[8]);
		mdct_one_dbg++;
	}
#endif

	mdct_core_0(work, out);
	mdct_core_1(work, out);
}


void mdct(fixed16 *in1, fixed16 *in2, fixed16 *out, int nbands)
{
	int i;

	for (i = 0; i < nbands; i++)
		mdct_one(in1+i, in2+i, out+18*i, i&1);
}


void butterfly(int nbands)
{
	int j, k;

#ifdef PEM_DEBUG
	static int bf_debug = 0;
	int max_before = 0, max_after = 0;
	if (bf_debug < 2) {
		for (int i = 0; i < 576; i++) {
			if (FASTVAR(xr)[i] > max_before) max_before = FASTVAR(xr)[i];
			if (FASTVAR(xr)[i] < -max_before) max_before = -FASTVAR(xr)[i];
		}
		fprintf(stderr, "    butterfly START nbands=%d cs[0]=%ld (0x%lx) ca[0]=%ld (0x%lx) max_xr=%d\n",
			nbands, (long)cs[0], (unsigned long)cs[0], (long)ca[0], (unsigned long)ca[0], max_before);
	}
#endif

//static profile_data pd; profile_start(&pd);
	for (k = 0; k < 8; k++) {
		fixed16 css = cs[k];
		fixed16 caa = -ca[k];
		fixed16 *p = &FASTVAR(xr)[17 - k];
		fixed16 *q = &FASTVAR(xr)[18 + k];

		for (j = 0; j < nbands; j++) {
			fixed16 bu = fm16(*p, css) - fm16(*q, caa);
			fixed16 bd = fm16(*q, css) + fm16(*p, caa);
#ifdef PEM_DEBUG
			if (bf_debug < 2 && k == 0 && j < 3) {
				fprintf(stderr, "      k=%d j=%d: *p=%d *q=%d css=%d caa=%d -> bu=%d bd=%d\n",
					k, j, *p, *q, css, caa, bu, bd);
			}
#endif
//if ((p-FASTVAR(xr) >= 288) && (q-FASTVAR(xr) >= 288)) fprintf(stderr, "bf oor: %d %d -> %d %d\n", k, j, p-FASTVAR(xr), q-FASTVAR(xr));
			*p = bu;
			*q = bd;
			p += 18;
			q += 18;
		}
	}

#ifdef PEM_DEBUG
	if (bf_debug < 2) {
		for (int i = 0; i < 576; i++) {
			if (FASTVAR(xr)[i] > max_after) max_after = FASTVAR(xr)[i];
			if (FASTVAR(xr)[i] < -max_after) max_after = -FASTVAR(xr)[i];
		}
		fprintf(stderr, "    butterfly END max_xr=%d\n", max_after);
		bf_debug++;
	}
#endif
//profile_end(&pd, "---------------> butt");
}

