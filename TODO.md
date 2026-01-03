# TODO - PEM Fixed-Point MP3 Encoder

## Low Priority

- [ ] Add psychoacoustic model tuning options
- [ ] Implement joint stereo improvements

## Completed

- [x] Add VBR (Variable Bitrate) support
  - Per-frame bitrate selection based on audio complexity from psychoacoustic model
  - CLI option: -V quality (0=highest quality, 9=lowest)
  - Xing/LAME header written for VBR files (enables accurate duration/seeking)
- [x] Add ABR (Average Bitrate) mode
  - Targets specified average bitrate while varying per-frame
  - CLI option: --abr rate
  - Supports --vbr-min and --vbr-max constraints
- [x] Add LAME-style quality presets (-V 0 through -V 9)
  - V0: 220-320 kbps (transparent quality)
  - V2: 170-210 kbps (high quality default)
  - V4: 140-185 kbps (good quality)
  - V6: 115-150 kbps (acceptable quality)
  - V9: 65-85 kbps (low bitrate)
- [x] Add 48kHz and 32kHz sample rate support
  - MPEG1 Layer III sample rates: 32kHz, 44.1kHz, 48kHz
  - Auto-detected from input WAV file
  - Frame size calculated dynamically per sample rate

- [x] Fix bitstream encoding issue causing "big_values too large!" errors in decoders
  - Root cause: reservoir_end() modified part3_length AFTER out() wrote main data
  - Fix: Removed part3_length modification in reservoir_end() (reservoir.c)
  - Stuffing bits now correctly become ancillary data at frame end
- [x] Implement ID3 tag writing (ID3v1 and ID3v2)
  - Added id3tag.c/h with full ID3v1 and ID3v2.3 support
  - CLI options: --title, --artist, --album, --year, --comment, --track, --genre
  - Control options: --id3v1, --id3v2, --no-id3
- [x] Add support for mono input files
  - Mono WAV files are automatically converted to stereo by duplicating channels
- [x] Port pem encoder source to standalone build
- [x] Create compatibility headers for standalone build
- [x] Create Makefile for Apple M1/ARM64
- [x] Create file-based CLI interface
- [x] Create wav_generator tool
- [x] Create wav_analyzer tool
- [x] Create mp3_info tool
- [x] Fix inline function compilation issues
- [x] Fix GET2h macro for modern C compilers
