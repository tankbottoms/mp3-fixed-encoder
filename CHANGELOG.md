# Changelog

All notable changes to this project will be documented in this file.

## [0.1.0] - 2026-01-02

### Added

- Initial standalone port of PEM encoder from adotcorporation/dadio codebase
- CLI interface with file-based I/O (replacing stdin/stdout)
- Progress bar display during encoding
- Encoding statistics output (-s flag)
- Quiet mode for scripting (-q flag)
- Apple M1/ARM64 optimized build support
- Compatibility headers for standalone build:
  - eresult.h - Error result type definitions
  - codec_workspace.h - Memory workspace allocation
- WAV generator tool with signal types:
  - Sine wave at specified frequency
  - Frequency sweep (chirp)
  - Square wave
  - White noise
  - Silence
  - Impulse
  - Multi-tone test signal
- WAV analyzer tool reporting:
  - Peak and RMS levels
  - DC offset
  - Dynamic range
  - Silence and clipping detection
- MP3 info tool displaying:
  - MPEG version and layer
  - Channel mode
  - Bitrate (CBR/VBR detection)
  - Frame count and duration
  - ID3 tag information
- Quality comparison shell script
- Comprehensive Makefile with targets:
  - generate-samples
  - encode-samples
  - test
  - quick-test
  - roundtrip-test

### Fixed

- GET2h macro for modern C compilers (removed lvalue cast)
- FAST_FUNC inline function definitions (use static inline)
- Missing includes in in.c (stdlib.h, string.h)
- Duplicate symbol errors from tables.c and qtab.c
- ERESULT success check in main.c (use FAILED() macro)

### Known Issues

- Encoder produces bitstreams with "big_values too large!" errors
- Decoders (ffmpeg, mpg123, mp3-fixed) cannot decode output correctly
- Root cause is likely in side information or reservoir handling

### Technical Notes

- Removed qtab.c and tables.c from build (data in ro.c)
- Changed FAST_FUNC to always use static inline
- Platform-specific workspace allocation replaces FAST_DECODE_ADDR
