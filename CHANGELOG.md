# Changelog

All notable changes to this project will be documented in this file.

## [1.2.0] - 2026-01-02

### Added

- Psychoacoustic model tuning system with configurable parameters:
  - Masking threshold offset (controls aggressive vs. transparent masking)
  - Temporal blend (frame-to-frame scalefactor smoothing, 0-100)
  - ATH (Absolute Threshold of Hearing) sensitivity (0-100)
  - Scalefactor multiplier (frequency bit allocation, 0-100)
  - Global gain offset (quantization fine-tuning, -20 to +20)
- Seven tuning presets:
  - `default` - Balanced quality and size
  - `quality` - Less aggressive masking, higher quality
  - `speed` - More aggressive masking, faster encoding
  - `voice` - Optimized for speech content
  - `music` - Optimized for music content
  - `bass` - Enhanced low frequency response
  - `transparent` - Near-transparent quality
- Joint stereo improvements:
  - Five stereo modes: auto, stereo, joint, ms, adaptive
  - Configurable MS stereo threshold (0-100)
  - High-frequency side channel cutoff band (0-21)
  - Stereo width control (0-100)
  - Adaptive MS stereo with per-coefficient decisions
- New CLI options:
  - `--preset P` - Select tuning preset
  - `--ath N` - Set ATH sensitivity
  - `--temporal N` - Set temporal blend factor
  - `--gain N` - Set global gain offset
  - `--stereo-mode M` - Select stereo encoding mode
  - `--ms-threshold N` - Set MS stereo threshold
  - `--stereo-width N` - Set stereo width
- New source files:
  - `pemlib/src/psy_tuning.c` - Tuning implementation
  - `pemlib/src/psy_tuning.h` - Tuning interface header

### Changed

- Version bumped to 1.2.0
- psy.c now uses configurable tuning parameters instead of hardcoded values
- Enhanced MS stereo processing with tunable thresholds

## [1.1.0] - 2026-01-02

### Added

- VBR (Variable Bitrate) encoding support:
  - Per-frame bitrate selection based on audio complexity
  - CLI option: `-V quality` (0=highest quality, 9=lowest)
  - Xing/LAME header for accurate duration and seeking
- ABR (Average Bitrate) mode:
  - Targets specified average bitrate while varying per-frame
  - CLI option: `--abr rate`
  - Supports `--vbr-min` and `--vbr-max` constraints
- LAME-style quality presets (-V 0 through -V 9):
  - V0: 220-320 kbps (transparent quality)
  - V2: 170-210 kbps (high quality default)
  - V4: 140-185 kbps (good quality)
  - V6: 115-150 kbps (acceptable quality)
  - V9: 65-85 kbps (low bitrate)
- Multiple sample rate support:
  - 32 kHz, 44.1 kHz, 48 kHz (MPEG-1 Layer III)
  - Auto-detected from input WAV file
  - Frame size calculated dynamically per sample rate
- ID3 tag support:
  - ID3v1 and ID3v2.3 metadata tags
  - CLI options: --title, --artist, --album, --year, --comment, --track, --genre
  - Control options: --id3v1, --id3v2, --no-id3
- Mono input file support (auto-converted to stereo)
- New source file: `pemlib/src/vbr.c` - VBR/ABR control
- New source file: `pemlib/src/id3tag.c` - ID3 tag writing

### Fixed

- Bitstream encoding issue causing "big_values too large!" errors
  - Root cause: reservoir_end() modified part3_length after out() wrote main data
  - Fix: Removed part3_length modification in reservoir_end()
  - Stuffing bits now correctly become ancillary data

### Changed

- Version bumped to 1.1.0
- Encoder now produces fully decoder-compliant MP3 bitstreams

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

### Known Issues (Fixed in v1.1.0)

- Encoder produces bitstreams with "big_values too large!" errors
- Decoders (ffmpeg, mpg123, mp3-fixed) cannot decode output correctly
- Root cause: reservoir_end() modified part3_length after bitstream was written

### Technical Notes

- Removed qtab.c and tables.c from build (data in ro.c)
- Changed FAST_FUNC to always use static inline
- Platform-specific workspace allocation replaces FAST_DECODE_ADDR
