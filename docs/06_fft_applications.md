# FFT Applications Beyond Audio Compression

The Fast Fourier Transform (FFT) that forms the core of MP3 encoding has applications far beyond audio compression. This document explores how the same fixed-point FFT optimizations used in this encoder apply to scientific instrumentation.

## The FFT in MP3 Encoding

The MP3 encoder uses transform-based compression:

1. **Time-domain input**: 1152 PCM audio samples per frame
2. **Frequency-domain analysis**: Polyphase filter bank + MDCT
3. **Perceptual coding**: Allocate bits based on psychoacoustic masking
4. **Huffman encoding**: Entropy coding of quantized coefficients

The computational core is the Modified Discrete Cosine Transform (MDCT), which is mathematically related to the FFT. Both decompose signals into frequency components.

## Insect Species Identification via Wing Beat Analysis

A compelling application of embedded FFT processing is automated insect identification, as demonstrated by Mullen et al. (2016) in their optical sensor system.

### The Problem

Different insect species have characteristic wing beat frequencies:

| Species | Frequency Range | Notes |
|---------|-----------------|-------|
| Mosquitoes (Culicidae) | 300-600 Hz | Varies by species, sex, temperature |
| House flies (Musca domestica) | 100-200 Hz | Lower frequency, irregular |
| Honey bees (Apis mellifera) | 130-250 Hz | Loaded vs. unloaded flight differs |
| Fruit flies (Drosophila) | 200-250 Hz | Small amplitude |
| Citrus psyllid (D. citri) | 187 +/- 26 Hz | Agricultural pest |

### The Solution: Optical FFT Analysis

The Mullen system uses:

1. **Tracking laser**: Illuminates flying insect in a detection volume
2. **Photodiode**: Measures oscillating light intensity as wings beat
3. **Signal conditioning**:
   - High-pass filter (remove DC offset)
   - Low-pass Butterworth at 2 kHz (remove noise)
4. **FFT via Welch's method**: Transforms time-domain signal to frequency spectrum
5. **Classification**: Match spectral signature to species database

### FFT Implementation Requirements

The Welch overlapped periodogram method requires:

```
P(f) = (1/K) * sum(|FFT(w[n] * x[n])|^2)
```

Where:
- `K` = number of overlapping segments
- `w[n]` = window function (typically Hanning)
- `x[n]` = input signal segment

For real-time species identification in field deployments:
- **Low power**: Battery operation in remote locations
- **Small memory**: Microcontroller-class devices
- **Fixed-point**: No floating-point unit available
- **Deterministic timing**: Real-time classification

These requirements mirror exactly the constraints of the PEM MP3 encoder.

### Spectral Features for Classification

The FFT analysis reveals:

| Feature | Description | Discriminative Power |
|---------|-------------|---------------------|
| Fundamental frequency (f0) | Primary wing beat rate | High (species-specific) |
| Harmonics (2f0, 3f0, ...) | Integer multiples | Medium (wing shape) |
| Harmonic ratios | Relative amplitudes | High (body size) |
| Spectral width | Frequency stability | Medium (flight mode) |

The "spectral fingerprint" combines these features for species classification.

### Results from Mullen et al.

The optical sensor achieved:

| Metric | Value |
|--------|-------|
| D. citri detection | 187 +/- 26 Hz fundamental |
| An. stephensi (mosquito) | Distinct male/female signatures |
| Classification accuracy | Species-level discrimination |
| Field deployment | Solar-powered, autonomous |

### Connection to MP3 Encoding

The MP3 encoder's fixed-point transforms can be repurposed:

| MP3 Component | Insect Application |
|---------------|-------------------|
| Polyphase filter bank | Bandpass filtering to wing beat range |
| MDCT | Spectral analysis of periodic signal |
| `costab[]` tables | Shared with Welch FFT implementation |
| Fixed-point arithmetic | Identical requirements |

A single embedded processor can run both audio encoding and insect classification using shared transform code.

## Other Scientific Applications

Fixed-point FFT implementations enable:

### Biomedical Signal Processing
- ECG analysis (heart rhythm at 0.5-40 Hz)
- EEG brain wave classification (0.5-100 Hz)
- Pulse oximetry signal extraction

### Industrial Monitoring
- Vibration analysis for bearing wear detection
- Motor current signature analysis
- Acoustic emission monitoring

### Environmental Sensing
- Seismic event detection
- Underwater acoustic monitoring
- Weather radar signal processing

### Agricultural Technology
- Crop disease detection via spectral imaging
- Soil moisture sensing
- Pollinator activity monitoring

## Implementation Considerations

### Memory Requirements

For a 512-point FFT (typical for insect wing beat analysis):

| Component | Size |
|-----------|------|
| Twiddle factors (sin/cos) | 2 KB |
| Input buffer | 1 KB |
| Output buffer | 2 KB |
| Working space | 2 KB |
| **Total** | **~7 KB** |

Fits easily in the ~40 KB RAM footprint of the MP3 encoder.

### Computational Cost

| Operation | Cycles (ARM7) |
|-----------|---------------|
| 512-point FFT | ~50,000 |
| Window function | ~2,000 |
| Magnitude calculation | ~5,000 |
| Peak detection | ~1,000 |
| **Total per analysis** | **~60,000** |

At 74 MHz, this allows ~1,200 analyses per second, far exceeding the ~50 Hz update rate needed for insect tracking.

## References

1. Mullen, E.R., et al. (2016). "Optical sensors for the detection of flying insects." *Optics Express*, 24(11), 11828. doi:10.1364/OE.24.011828

2. ISO/IEC 11172-3:1993. "Information technology -- Coding of moving pictures and associated audio for digital storage media at up to about 1,5 Mbit/s -- Part 3: Audio"

3. Welch, P.D. (1967). "The use of fast Fourier transform for the estimation of power spectra." *IEEE Transactions on Audio and Electroacoustics*, 15(2), 70-73.

4. Cirrus Logic EP7312 Data Sheet. "32-bit ARM720T Core Processor."
