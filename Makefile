# ==============================================================================
# PEM Fixed-Point MP3 Encoder Makefile
# ==============================================================================
#
# Copyright (C) 1998-2025 Mark Phillips. All rights reserved.
#
# Original encoder algorithm: Segher Boessenkool (1998-2002)
# Embedded integration: Interactive Objects, Inc. (2001)
# Modern platform adaptation: Mark Phillips (2025)
#
# See LICENSE for terms of use.
# ==============================================================================
#
# Targets Apple M1/ARM64 and x86_64 macOS

CC = gcc
CFLAGS = -O3 -Wall -Wno-unused-variable -Wno-unused-function -Wno-unused-but-set-variable
CFLAGS += -DSTANDALONE -DDESKTOP
CFLAGS += $(CFLAGS_EXTRA)
LIBS = -lm

# Platform detection
UNAME_S := $(shell uname -s)
UNAME_M := $(shell uname -m)

ifeq ($(UNAME_S),Darwin)
    CFLAGS += -DMACOSX
    ifeq ($(UNAME_M),arm64)
        CFLAGS += -DARM64 -mcpu=apple-m1
    endif
endif

# Directories
PEMLIB_SRC = pemlib/src
PEMLIB_INC = pemlib/include
SRC_DIR = src
TOOLS_DIR = tools
SAMPLES_DIR = samples
BUILD_DIR = build

# Include paths
INCLUDES = -I./include -I./$(PEMLIB_INC) -I./$(PEMLIB_SRC)

# PEM library sources (order matters for dependencies)
# Note: qtab.c and tables.c are excluded as their data is already in ro.c
PEMLIB_SOURCES = $(PEMLIB_SRC)/workspace.c \
                 $(PEMLIB_SRC)/memory.c \
                 $(PEMLIB_SRC)/codec.c \
                 $(PEMLIB_SRC)/fpmp3.c \
                 $(PEMLIB_SRC)/polyphase.c \
                 $(PEMLIB_SRC)/hybrid.c \
                 $(PEMLIB_SRC)/psy.c \
                 $(PEMLIB_SRC)/quant.c \
                 $(PEMLIB_SRC)/code.c \
                 $(PEMLIB_SRC)/count.c \
                 $(PEMLIB_SRC)/huffman.c \
                 $(PEMLIB_SRC)/reservoir.c \
                 $(PEMLIB_SRC)/out.c \
                 $(PEMLIB_SRC)/in.c \
                 $(PEMLIB_SRC)/ro.c \
                 $(PEMLIB_SRC)/id3tag.c \
                 $(PEMLIB_SRC)/vbr.c

MAIN_SOURCES = $(SRC_DIR)/main.c

# Object files
PEMLIB_OBJECTS = $(PEMLIB_SOURCES:.c=.o)
MAIN_OBJECTS = $(MAIN_SOURCES:.c=.o)
ALL_OBJECTS = $(PEMLIB_OBJECTS) $(MAIN_OBJECTS)

# Output executables
TARGET = pem_encode
TOOLS = $(TOOLS_DIR)/wav_generator $(TOOLS_DIR)/wav_analyzer $(TOOLS_DIR)/mp3_info

# ANSI Color Codes
CYAN := \033[0;36m
GREEN := \033[0;32m
YELLOW := \033[0;33m
RED := \033[0;31m
RESET := \033[0m
BOLD := \033[1m

# Default target
all: $(TARGET) tools
	@echo ""
	@echo "$(GREEN)Build complete!$(RESET)"
	@echo "  Encoder: $(YELLOW)./$(TARGET)$(RESET)"
	@echo "  Tools: $(YELLOW)$(TOOLS)$(RESET)"
	@echo ""
	@echo "Usage: ./$(TARGET) [-b bitrate] input.wav output.mp3"

# Build the main encoder
$(TARGET): $(ALL_OBJECTS)
	@echo "$(CYAN)Linking $(TARGET)...$(RESET)"
	$(CC) $(CFLAGS) -o $(TARGET) $(ALL_OBJECTS) $(LIBS)
	@echo "$(GREEN)Built: $(TARGET)$(RESET)"

# Build analysis tools
tools: $(TOOLS)

$(TOOLS_DIR)/wav_generator: $(TOOLS_DIR)/wav_generator.c
	@echo "$(CYAN)Building wav_generator...$(RESET)"
	$(CC) $(CFLAGS) -o $@ $< $(LIBS)

$(TOOLS_DIR)/wav_analyzer: $(TOOLS_DIR)/wav_analyzer.c
	@echo "$(CYAN)Building wav_analyzer...$(RESET)"
	$(CC) $(CFLAGS) -o $@ $< $(LIBS)

$(TOOLS_DIR)/mp3_info: $(TOOLS_DIR)/mp3_info.c
	@echo "$(CYAN)Building mp3_info...$(RESET)"
	$(CC) $(CFLAGS) -o $@ $< $(LIBS)

# Compile source files
%.o: %.c
	@echo "$(CYAN)Compiling $<...$(RESET)"
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

# Clean build artifacts only (preserves test samples)
clean:
	@echo "$(YELLOW)Cleaning build artifacts...$(RESET)"
	rm -f $(ALL_OBJECTS) $(TARGET) $(TOOLS)
	rm -rf $(BUILD_DIR)
	@echo "$(GREEN)Clean complete$(RESET)"

# Clean everything including generated samples
distclean: clean
	@echo "$(YELLOW)Cleaning generated samples...$(RESET)"
	rm -f $(SAMPLES_DIR)/*.wav $(SAMPLES_DIR)/*.mp3
	rm -rf $(SAMPLES_DIR)/comparison $(SAMPLES_DIR)/reference
	rm -rf $(SAMPLES_DIR)/roundtrip $(SAMPLES_DIR)/test
	rm -f .DS_Store $(SAMPLES_DIR)/.DS_Store
	@echo "$(GREEN)Distclean complete$(RESET)"

# Generate test WAV files
generate-samples: $(TOOLS_DIR)/wav_generator
	@echo "$(CYAN)Generating test WAV samples...$(RESET)"
	@mkdir -p $(SAMPLES_DIR)
	$(TOOLS_DIR)/wav_generator --sine 440 5 $(SAMPLES_DIR)/sine_440hz.wav
	$(TOOLS_DIR)/wav_generator --sine 1000 5 $(SAMPLES_DIR)/sine_1000hz.wav
	$(TOOLS_DIR)/wav_generator --sweep 20 20000 10 $(SAMPLES_DIR)/sweep_20_20k.wav
	$(TOOLS_DIR)/wav_generator --noise 5 $(SAMPLES_DIR)/white_noise.wav
	$(TOOLS_DIR)/wav_generator --square 440 5 $(SAMPLES_DIR)/square_440hz.wav
	$(TOOLS_DIR)/wav_generator --silence 5 $(SAMPLES_DIR)/silence.wav
	@echo "$(GREEN)Generated test samples in $(SAMPLES_DIR)/$(RESET)"

# Encode all test samples
encode-samples: $(TARGET) generate-samples
	@echo "$(CYAN)Encoding test samples...$(RESET)"
	@for wav in $(SAMPLES_DIR)/*.wav; do \
		if [ -f "$$wav" ]; then \
			base=$$(basename "$$wav" .wav); \
			echo "  Encoding $$base.wav -> $$base.mp3"; \
			./$(TARGET) -b 128 -q "$$wav" "$(SAMPLES_DIR)/$$base.mp3"; \
		fi; \
	done
	@echo "$(GREEN)Encoding complete$(RESET)"

# Run the full test suite
test: $(TARGET) $(TOOLS) encode-samples
	@echo ""
	@echo "$(CYAN)========================================$(RESET)"
	@echo "$(CYAN)    PEM Encoder Test Suite$(RESET)"
	@echo "$(CYAN)========================================$(RESET)"
	@echo ""
	@echo "$(BOLD)1. Analyzing generated MP3 files$(RESET)"
	@for mp3 in $(SAMPLES_DIR)/*.mp3; do \
		if [ -f "$$mp3" ]; then \
			echo ""; \
			echo "$(YELLOW)$$(basename $$mp3):$(RESET)"; \
			./$(TOOLS_DIR)/mp3_info "$$mp3" 2>/dev/null || echo "  Analysis failed"; \
		fi; \
	done
	@echo ""
	@echo "$(GREEN)========================================$(RESET)"
	@echo "$(GREEN)    Test Suite Complete$(RESET)"
	@echo "$(GREEN)========================================$(RESET)"

# Quick test with a single file
quick-test: $(TARGET)
	@echo "$(CYAN)Quick test: encoding sine wave$(RESET)"
	@mkdir -p $(SAMPLES_DIR)
	@if [ ! -f "$(SAMPLES_DIR)/quick_test.wav" ]; then \
		echo "Generating quick test WAV..."; \
		$(MAKE) -s $(TOOLS_DIR)/wav_generator; \
		$(TOOLS_DIR)/wav_generator --sine 440 2 $(SAMPLES_DIR)/quick_test.wav; \
	fi
	./$(TARGET) -s $(SAMPLES_DIR)/quick_test.wav $(SAMPLES_DIR)/quick_test.mp3
	@echo ""
	@echo "$(GREEN)Quick test complete!$(RESET)"
	@echo "Output: $(SAMPLES_DIR)/quick_test.mp3"

# Convert MP3 files from mp3-fixed to WAV, encode, compare
roundtrip-test: $(TARGET) $(TOOLS)
	@echo "$(CYAN)========================================$(RESET)"
	@echo "$(CYAN)    Roundtrip Quality Test$(RESET)"
	@echo "$(CYAN)========================================$(RESET)"
	@echo ""
	@echo "This test converts MP3 samples to WAV, re-encodes them"
	@echo "with pem_encode, and compares audio quality."
	@echo ""
	@if [ ! -d "../mp3-fixed/samples" ]; then \
		echo "$(RED)Error: ../mp3-fixed/samples not found$(RESET)"; \
		exit 1; \
	fi
	@mkdir -p $(SAMPLES_DIR)/reference $(SAMPLES_DIR)/roundtrip
	@echo "$(BOLD)1. Converting reference MP3s to WAV$(RESET)"
	@for mp3 in ../mp3-fixed/samples/*.mp3; do \
		if [ -f "$$mp3" ]; then \
			base=$$(basename "$$mp3" .mp3); \
			echo "  Converting $$base.mp3 -> WAV"; \
			ffmpeg -y -i "$$mp3" -ar 44100 -ac 2 -sample_fmt s16 \
				"$(SAMPLES_DIR)/reference/$$base.wav" 2>/dev/null; \
		fi; \
	done
	@echo ""
	@echo "$(BOLD)2. Re-encoding with pem_encode$(RESET)"
	@for wav in $(SAMPLES_DIR)/reference/*.wav; do \
		if [ -f "$$wav" ]; then \
			base=$$(basename "$$wav" .wav); \
			echo "  Encoding $$base.wav -> MP3"; \
			./$(TARGET) -b 128 -q "$$wav" "$(SAMPLES_DIR)/roundtrip/$$base.mp3"; \
		fi; \
	done
	@echo ""
	@echo "$(BOLD)3. Converting roundtrip MP3s back to WAV$(RESET)"
	@for mp3 in $(SAMPLES_DIR)/roundtrip/*.mp3; do \
		if [ -f "$$mp3" ]; then \
			base=$$(basename "$$mp3" .mp3); \
			echo "  Converting $$base.mp3 -> WAV"; \
			ffmpeg -y -i "$$mp3" -ar 44100 -ac 2 -sample_fmt s16 \
				"$(SAMPLES_DIR)/roundtrip/$$base.wav" 2>/dev/null; \
		fi; \
	done
	@echo ""
	@echo "$(GREEN)Roundtrip test files generated$(RESET)"
	@echo "Reference WAVs: $(SAMPLES_DIR)/reference/"
	@echo "Roundtrip files: $(SAMPLES_DIR)/roundtrip/"

# Install the encoder
install: $(TARGET)
	@echo "$(CYAN)Installing pem_encode to /usr/local/bin$(RESET)"
	install -m 755 $(TARGET) /usr/local/bin/
	@echo "$(GREEN)Installed!$(RESET)"

# Show help
help:
	@echo "$(BOLD)PEM Fixed-Point MP3 Encoder Build System$(RESET)"
	@echo ""
	@echo "$(BOLD)Build Targets:$(RESET)"
	@echo "  $(YELLOW)all$(RESET)              - Build encoder and tools (default)"
	@echo "  $(YELLOW)$(TARGET)$(RESET)        - Build encoder only"
	@echo "  $(YELLOW)tools$(RESET)            - Build analysis tools only"
	@echo ""
	@echo "$(BOLD)Test Targets:$(RESET)"
	@echo "  $(YELLOW)quick-test$(RESET)       - Quick encode test (2s sine wave)"
	@echo "  $(YELLOW)test$(RESET)             - Run full test suite"
	@echo "  $(YELLOW)roundtrip-test$(RESET)   - Quality comparison with mp3-fixed samples"
	@echo "  $(YELLOW)generate-samples$(RESET) - Generate test WAV files only"
	@echo "  $(YELLOW)encode-samples$(RESET)   - Generate and encode all test samples"
	@echo ""
	@echo "$(BOLD)Maintenance:$(RESET)"
	@echo "  $(YELLOW)clean$(RESET)            - Remove build artifacts (keeps samples)"
	@echo "  $(YELLOW)distclean$(RESET)        - Remove all generated files"
	@echo "  $(YELLOW)install$(RESET)          - Install to /usr/local/bin"
	@echo "  $(YELLOW)help$(RESET)             - Show this message"

.PHONY: all clean distclean tools test quick-test generate-samples encode-samples roundtrip-test install help
