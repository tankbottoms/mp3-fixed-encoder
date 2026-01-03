# Encoder-Specific Optimizations

This document describes the optimizations that make the PEM fixed-point MP3 encoder suitable for resource-constrained embedded systems.

## Encoding vs. Decoding Complexity

Traditional MP3 encoding is significantly more computationally expensive than decoding:

| Operation | Typical Ratio | This Implementation |
|-----------|---------------|---------------------|
| Standard encoder | 3-4x decode time | ~1x decode time |
| High-quality encoder | 10-20x decode time | ~1x decode time |
| Psychoacoustic analysis | 40-60% of encode time | Simplified model |

The PEM encoder achieves near-symmetric encode/decode performance through aggressive algorithmic simplification while maintaining acceptable audio quality for embedded applications.

## Key Optimizations

### 1. Fixed-Point Arithmetic Throughout

All floating-point operations are replaced with 32-bit integer arithmetic:

```c
typedef int32_t fixed16;   /* Q16.16 fixed-point format */
typedef int64_t fixed64_32; /* Q32.32 for intermediate products */

/* Multiplication with 16-bit fractional precision */
static inline fixed16 fm16(fixed16 a, fixed16 b) {
    return ((int64_t)a * b) >> 16;
}
```

**Benefits:**
- Eliminates FPU requirements (most embedded CPUs lack FPU)
- Predictable cycle counts for real-time guarantees
- Reduced silicon area in custom implementations

### 2. Simplified Psychoacoustic Model

The ISO 11172-3 psychoacoustic model is computationally expensive. This encoder uses a simplified approach:

| Full ISO Model | PEM Encoder |
|----------------|-------------|
| 512-point FFT per granule | Energy-based band estimation |
| Spreading function convolution | Pre-computed spreading weights |
| Temporal masking | Granule-to-granule energy tracking |
| Tonality estimation | Omitted (assume noise-like) |

The `psy.c` module implements:
- **Energy band calculation**: Sum of squared samples per scalefactor band
- **Logarithmic energy**: Table-driven log2 approximation
- **Spreading function**: Pre-computed `sbew[]` weights applied directly
- **Threshold calculation**: Fixed offset from spread energy

### 3. Pre-Computed Tables in ROM

Large lookup tables are stored in read-only memory (`ro.c`):

| Table | Size | Purpose |
|-------|------|---------|
| `gaintab[256]` | 1 KB | Quantization step sizes |
| `qtab[8192]` | 32 KB | Quantization thresholds |
| `costab[]` | 2 KB | DCT cosine values |
| `ht[]` | 4 KB | Huffman code tables |
| `sbo[]`, `sbew[]` | 256 B | Scalefactor band boundaries |

Total ROM: ~45 KB of constant data, enabling RAM-constrained operation.

### 4. Optimized MDCT Implementation

The Modified Discrete Cosine Transform uses:
- **Windowing**: Pre-computed `iwinn[]` window coefficients
- **Butterfly operations**: Radix-2 decomposition with table lookup
- **Aliasing reduction**: Pre-computed `ca[]` and `cs[]` coefficients

The `hybrid.c` implementation processes 576 samples per granule with:
- 18 MDCT operations of 32 points each
- Aliasing reduction butterflies between subbands
- All operations in fixed-point with overflow protection

### 5. Efficient Huffman Encoding

The `hbits[34][16][16]` table pre-computes:
- Huffman code length for each (x, y) pair
- Sign bit requirements
- Linbits for extended range values

This enables O(1) bit counting during rate control iterations.

### 6. Single-Pass Quantization

Rather than iterative rate-distortion optimization:

1. Estimate initial gain from psychoacoustic model
2. Binary search for target bit count (6-8 iterations typical)
3. Use `limit_nonzero` tracking to skip zero regions

The `quant.c` module typically converges in 6-8 quantization attempts per granule.

## Memory Requirements

### RAM Usage (SRAM)

| Component | Size | Description |
|-----------|------|-------------|
| `granule[2][2]` | 256 B | Granule side information |
| `dctbuf[2][2][576]` | 9.2 KB | Polyphase filter output |
| `xr[576]` | 2.3 KB | MDCT coefficients |
| `ix[576]` | 1.2 KB | Quantized values |
| `polybuf[2112]` | 4.2 KB | Polyphase filter state |
| `mainbitter[4096]` | 16 KB | Bitstream buffer |
| Miscellaneous | ~5 KB | Working variables |

**Total RAM: ~40 KB**

### ROM Usage

| Component | Size |
|-----------|------|
| Code | ~25 KB |
| Constant tables | ~45 KB |
| **Total ROM** | **~70 KB** |

## Target Platform: Cirrus Logic EP7312

The encoder was designed for the EP7312 ARM720T-based SoC:

| Specification | EP7312 |
|---------------|--------|
| CPU | ARM720T @ 74 MHz |
| Instruction cache | 8 KB |
| Data cache | None |
| On-chip SRAM | 80 KB |
| External memory | 16-bit SDRAM |

The 40 KB RAM requirement fits comfortably in the EP7312's 80 KB SRAM, enabling real-time encoding without external memory access penalties.

## Performance Characteristics

Measured on Apple M1 (for reference):

| Metric | Value |
|--------|-------|
| Encoding speed | 500-600x realtime |
| Per-frame time | ~0.05 ms |
| Memory bandwidth | ~2 MB/s input |

Estimated on EP7312 @ 74 MHz:

| Metric | Value |
|--------|-------|
| Target speed | 1.0-1.2x realtime |
| Per-frame time | ~24 ms |
| Cycles/sample | ~1600 |

## Trade-offs

The optimizations involve quality trade-offs:

| Aspect | Impact |
|--------|--------|
| Psychoacoustic precision | Reduced; may audible on complex passages |
| Stereo imaging | MS stereo only; no intensity stereo |
| Low bitrate performance | Degraded below 96 kbps |
| Pre-echo handling | Minimal; no block switching |

These trade-offs are acceptable for:
- Voice recording applications
- Low-complexity audio content
- Real-time encoding requirements
- Battery-powered devices

## Comparison with Reference Encoders

| Encoder | Quality (PEAQ) | Speed | RAM |
|---------|---------------|-------|-----|
| LAME (preset standard) | Excellent | 10x RT | 2 MB |
| LAME (preset fast) | Very Good | 30x RT | 1 MB |
| Shine (fixed-point) | Good | 5x RT | 200 KB |
| **PEM Encoder** | Acceptable | 1x RT | 40 KB |

The PEM encoder targets the "minimum viable quality" point for maximum resource efficiency.
