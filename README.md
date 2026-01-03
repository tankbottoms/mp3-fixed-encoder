# Fixed-Point MP3 Encoder

A high-efficiency, fixed-point MP3 encoder optimized for embedded systems and resource-constrained environments.

## Table of Contents

- [Quick Start](#quick-start)
- [Overview](#overview)
- [Audio Quality](#audio-quality)
- [Performance](#performance)
- [Historical Context](#historical-context)
- [Applications](#applications)
- [Building](#building)
- [Usage](#usage)
- [Project Structure](#project-structure)
- [Documentation](#documentation)
- [License](#license)

## Quick Start

```bash
# Build encoder and tools
make

# Encode an audio file
./pem_encode input.wav output.mp3

# Run quick test
make quick-test

# Run full test suite
make test
```

## Overview

This encoder implements MPEG-1 Layer III (MP3) audio compression using entirely fixed-point arithmetic. Originally developed for the first generation of portable media players, it achieves real-time encoding on processors as modest as the ARM7TDMI at 74 MHz with only 40 KB of RAM.

### Key Features

- **Fixed-point arithmetic**: No floating-point unit required
- **Minimal memory footprint**: ~40 KB RAM, ~70 KB ROM
- **Real-time capable**: Designed for 1x realtime on 74 MHz ARM7
- **CBR/VBR/ABR modes**: Constant, variable, and average bitrate encoding
- **Quality presets**: LAME-style -V 0 through -V 9 presets
- **Bitrate range**: 32-320 kbps
- **Sample rates**: 32 kHz, 44.1 kHz, 48 kHz (MPEG-1 Layer III)
- **ID3 tags**: ID3v1 and ID3v2.3 metadata support
- **Format**: MPEG-1 Layer III stereo (mono input auto-converted)
- **Modern platform support**: macOS ARM64/x86_64, Linux

### Copyright

```
Copyright (C) 1998-2025 Mark Phillips. All rights reserved.

Original algorithm: Segher Boessenkool (1998-2002)
Embedded integration: Interactive Objects, Inc. (2001)
Modern platform adaptation: Mark Phillips (2025)
```

## Audio Quality

### Objective Measurements

Comparison of original LAME-encoded MP3 versus re-encoding through the PEM fixed-point encoder at 192 kbps:

| Metric | Original (LAME) | PEM Encoder | Difference |
|--------|-----------------|-------------|------------|
| Mean Volume | -23.000 dB | -23.200 dB | 0.200 dB |
| Peak Volume | -4.400 dB | -4.800 dB | 0.400 dB |
| Duration | 29.989 s | 29.944 s | 0.045 s |
| File Size | 703 KB | 701 KB | 0.284% |

### Quality Characteristics

The encoder produces perceptually acceptable output with the following trade-offs:

| Aspect | Characteristic |
|--------|----------------|
| Psychoacoustic model | Simplified energy-based (vs. full FFT) |
| Stereo mode | Mid-side only (no intensity stereo) |
| Block switching | Not implemented (long blocks only) |
| Pre-emphasis | Not implemented |
| Best performance | 128-256 kbps |

## Performance

### Computational Overhead

| Platform | Speed | Notes |
|----------|-------|-------|
| Apple M1 (3.2 GHz) | 680x realtime | 30 seconds encoded in 0.044 s |
| EP7312 (74 MHz ARM7) | ~1x realtime | Original target platform |
| Estimated MIPS | ~60 MIPS | For realtime at 128 kbps |

### Memory Requirements

| Component | Size | Description |
|-----------|------|-------------|
| Code segment | 98 KB | Executable instructions |
| Data segment | 82 KB | Initialized data |
| SRAM working set | 40 KB | Runtime state and buffers |
| ROM tables | 45 KB | Pre-computed constants |

The 40 KB working set fits entirely within the EP7312's 80 KB on-chip SRAM, eliminating external memory access during encoding.

### Encoding Symmetry

Traditional MP3 encoding is 3-10x more expensive than decoding. This encoder achieves near-symmetric performance through:

1. **Simplified psychoacoustic model**: Energy-based vs. FFT-based
2. **Pre-computed tables**: All trigonometric values in ROM
3. **Single-pass quantization**: Binary search vs. iterative optimization
4. **Fixed block size**: No adaptive block switching

## Historical Context

### Target Platform: Cirrus Logic EP7312

The encoder was designed for the EP7312 ARM720T-based system-on-chip, popular in first-generation portable media players (1998-2002).

| Specification | EP7312 |
|---------------|--------|
| Processor | ARM720T @ 74 MHz |
| Architecture | ARMv4T (Thumb) |
| Instruction cache | 8 KB |
| Data cache | None |
| On-chip SRAM | 80 KB |
| External memory | 16-bit SDRAM |

The EP7312 datasheet is included at `docs/ep7312.pdf`.

### Commercial Deployment

This encoder technology was deployed in:

- Portable MP3 players (1999-2003)
- Pocket PC voice recording applications
- Automotive entertainment systems
- Embedded audio logging devices

## Applications

### Beyond Audio Compression

The fixed-point FFT and spectral analysis techniques used in this encoder have applications in scientific instrumentation, particularly where low-power, real-time frequency analysis is required.

### Insect Species Identification

A notable application is optical insect detection, as demonstrated by Mullen et al. (2016). The system uses the same spectral analysis principles:

| Species | Wing Beat Frequency |
|---------|---------------------|
| Mosquitoes (Culicidae) | 300-600 Hz |
| House flies (Musca domestica) | 100-200 Hz |
| Honey bees (Apis mellifera) | 130-250 Hz |
| Citrus psyllid (D. citri) | 187 +/- 26 Hz |

**How it works:**

1. A tracking laser illuminates a flying insect
2. A photodiode measures oscillating light intensity as wings beat
3. Signal conditioning filters noise (high-pass DC removal, 2 kHz low-pass)
4. **FFT via Welch's method** transforms the time-domain signal to frequency
5. Spectral signature matches against species database

The Welch overlapped periodogram method enables species classification with:
- Solar-powered field deployment
- Microcontroller-class processors
- Same fixed-point FFT code as this encoder

The research paper is included at `docs/oe-24-11-11828.pdf`.

### Other Fixed-Point FFT Applications

| Domain | Application |
|--------|-------------|
| Biomedical | ECG rhythm analysis, EEG classification |
| Industrial | Vibration monitoring, motor diagnostics |
| Environmental | Seismic detection, acoustic monitoring |
| Agricultural | Crop disease spectral analysis |

## Building

### Requirements

- GCC or Clang compiler
- Make
- ffmpeg (optional, for testing)

### Build Commands

```bash
# Build encoder and all tools
make

# Build encoder only
make pem_encode

# Build with debug symbols
make CFLAGS_EXTRA="-g"

# Build with sanitizers
make CFLAGS_EXTRA="-fsanitize=address,undefined"

# Show all targets
make help
```

### Make Targets

| Target | Description |
|--------|-------------|
| `all` | Build encoder and tools (default) |
| `pem_encode` | Build encoder only |
| `tools` | Build analysis tools only |
| `quick-test` | Quick encode test (2 second sine wave) |
| `test` | Run full test suite |
| `roundtrip-test` | Quality comparison with mp3-fixed samples |
| `generate-samples` | Generate test WAV files only |
| `encode-samples` | Generate and encode all test samples |
| `clean` | Remove build artifacts (keeps samples) |
| `distclean` | Remove all generated files |
| `install` | Install to /usr/local/bin |

## Usage

### Basic Encoding

```bash
# Encode at default 128 kbps CBR
./pem_encode input.wav output.mp3

# Encode at specific bitrate
./pem_encode -b 192 input.wav output.mp3

# Show statistics after encoding
./pem_encode -s input.wav output.mp3

# Quiet mode (no progress bar)
./pem_encode -q input.wav output.mp3
```

### VBR and ABR Modes

```bash
# VBR with quality preset (0=best, 9=smallest)
./pem_encode -V 2 input.wav output.mp3

# ABR targeting 128 kbps average
./pem_encode --abr 128 input.wav output.mp3

# VBR with bitrate constraints
./pem_encode -V 4 --vbr-min 96 --vbr-max 256 input.wav output.mp3
```

### Quality Presets

| Preset | Bitrate Range | Quality |
|--------|---------------|---------|
| -V 0 | 220-320 kbps | Transparent |
| -V 2 | 170-210 kbps | High (default VBR) |
| -V 4 | 140-185 kbps | Good |
| -V 6 | 115-150 kbps | Acceptable |
| -V 9 | 65-85 kbps | Low bitrate |

### ID3 Tags

```bash
# Add metadata tags
./pem_encode --title "Song Name" --artist "Artist" --album "Album" input.wav output.mp3

# Control tag versions
./pem_encode --id3v2 --no-id3v1 input.wav output.mp3  # ID3v2 only
./pem_encode --id3v1 --no-id3v2 input.wav output.mp3  # ID3v1 only
```

### Supported Bitrates

32, 40, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320 kbps

### Input Requirements

| Parameter | Requirement |
|-----------|-------------|
| Format | WAV (PCM) |
| Channels | 1 (mono) or 2 (stereo) |
| Sample rate | 32000, 44100, or 48000 Hz |
| Bit depth | 16-bit |

### Analysis Tools

```bash
# Generate test signals
./tools/wav_generator --sine 440 5 test.wav
./tools/wav_generator --sweep 20 20000 10 sweep.wav
./tools/wav_generator --noise 5 noise.wav
./tools/wav_generator --multitone 5 multi.wav

# Analyze WAV file
./tools/wav_analyzer input.wav

# Inspect MP3 frame structure
./tools/mp3_info output.mp3
```

## Project Structure

```
mp3-fixed-encoder/
├── pemlib/              # Core encoder library
│   ├── include/         # Public headers
│   │   └── fpmp3.h      # Main API header
│   └── src/             # Implementation
│       ├── codec.c      # Main encoder orchestration
│       ├── polyphase.c  # Polyphase analysis filter bank
│       ├── hybrid.c     # MDCT and aliasing butterfly
│       ├── psy.c        # Psychoacoustic model
│       ├── quant.c      # Quantization and rate control
│       ├── huffman.c    # Huffman encoding tables
│       ├── out.c        # Bitstream formatting
│       ├── reservoir.c  # Bit reservoir management
│       ├── vbr.c        # VBR/ABR bitrate control
│       ├── id3tag.c     # ID3v1/v2 metadata tags
│       └── ro.c         # ROM constant tables
├── src/                 # CLI application
│   └── main.c           # Command-line interface
├── tools/               # Analysis utilities
│   ├── wav_generator.c  # Test signal generator
│   ├── wav_analyzer.c   # WAV file analyzer
│   └── mp3_info.c       # MP3 frame inspector
├── docs/                # Technical documentation
│   ├── iso11172-3/      # MP3 specification excerpts
│   ├── 02_mp3_algorithm.md
│   ├── 03_fixed_point_math.md
│   ├── 04_optimization_generic.md
│   ├── 05_encoder_optimizations.md
│   ├── 06_fft_applications.md
│   ├── ep7312.pdf       # Target processor datasheet
│   └── oe-24-11-11828.pdf  # Insect detection paper
├── Makefile
├── README.md
├── TODO.md
└── LICENSE
```

## Documentation

Technical documentation is available in the `docs/` directory:

| Document | Description |
|----------|-------------|
| `05_encoder_optimizations.md` | Encoder-specific optimizations and design decisions |
| `06_fft_applications.md` | FFT applications beyond audio (insect detection) |
| `iso11172-3/` | ISO MPEG-1 Audio specification excerpts |
| `ep7312.pdf` | Cirrus Logic EP7312 processor datasheet |
| `oe-24-11-11828.pdf` | Mullen et al. optical insect detection paper |

## License

This software is provided under a non-commercial license. See [LICENSE](LICENSE) for full terms.

**Summary:**
- Personal and educational use permitted
- Commercial use requires separate license
- No redistribution without permission
- Attribution required

For commercial licensing inquiries, contact Mark Phillips through the repository.

## References

1. ISO/IEC 11172-3:1993 - Information technology - Coding of moving pictures and associated audio for digital storage media at up to about 1.5 Mbit/s - Part 3: Audio

2. Mullen, E.R., et al. (2016). "Optical sensors for the detection of flying insects." *Optics Express*, 24(11), 11828.

3. Cirrus Logic EP7312 Data Sheet - 32-bit ARM720T Core Processor

4. Davis, P. (1998). "Fixed-point DSP for Audio Applications." *Embedded Systems Programming*.
