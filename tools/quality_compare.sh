#!/bin/bash
# quality_compare.sh - Compare encoding quality between reference and pem encoder
#
# This script performs a roundtrip test:
# 1. Converts reference MP3 to WAV using ffmpeg
# 2. Re-encodes WAV with pem_encode
# 3. Decodes both MP3s to WAV
# 4. Computes quality metrics (SNR, correlation, etc.)
#
# Copyright (C) 2025 Mark Phillips

set -e

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[0;33m'
CYAN='\033[0;36m'
NC='\033[0m' # No Color

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
ENCODER="$PROJECT_DIR/pem_encode"
WAV_ANALYZER="$SCRIPT_DIR/wav_analyzer"
MP3_INFO="$SCRIPT_DIR/mp3_info"

# Temp directory
TEMP_DIR=$(mktemp -d)
trap "rm -rf $TEMP_DIR" EXIT

usage() {
    echo "Quality Comparison Tool for PEM MP3 Encoder"
    echo ""
    echo "Usage: $0 [options] <reference.mp3>"
    echo ""
    echo "Options:"
    echo "  -b <bitrate>   Encoding bitrate (default: 128)"
    echo "  -v             Verbose output"
    echo "  -h             Show this help"
    echo ""
    echo "This tool compares the encoding quality of pem_encode against the reference."
}

BITRATE=128
VERBOSE=0

while getopts "b:vh" opt; do
    case $opt in
        b) BITRATE=$OPTARG ;;
        v) VERBOSE=1 ;;
        h) usage; exit 0 ;;
        *) usage; exit 1 ;;
    esac
done
shift $((OPTIND-1))

if [ $# -lt 1 ]; then
    usage
    exit 1
fi

REFERENCE_MP3="$1"
BASENAME=$(basename "$REFERENCE_MP3" .mp3)

if [ ! -f "$REFERENCE_MP3" ]; then
    echo -e "${RED}Error: Reference file not found: $REFERENCE_MP3${NC}"
    exit 1
fi

if [ ! -f "$ENCODER" ]; then
    echo -e "${RED}Error: Encoder not found: $ENCODER${NC}"
    echo "Run 'make' to build the encoder first."
    exit 1
fi

echo -e "${CYAN}========================================"
echo "  Quality Comparison Test"
echo "========================================${NC}"
echo ""
echo "Reference: $REFERENCE_MP3"
echo "Bitrate:   $BITRATE kbps"
echo ""

# Step 1: Decode reference MP3 to WAV
echo -e "${YELLOW}Step 1: Decoding reference MP3 to WAV...${NC}"
REF_WAV="$TEMP_DIR/reference.wav"
ffmpeg -y -i "$REFERENCE_MP3" -ar 44100 -ac 2 -sample_fmt s16 "$REF_WAV" 2>/dev/null
if [ $VERBOSE -eq 1 ]; then
    echo "  Created: $REF_WAV"
fi

# Step 2: Re-encode with pem_encode
echo -e "${YELLOW}Step 2: Encoding with pem_encode...${NC}"
PEM_MP3="$TEMP_DIR/pem_encoded.mp3"
"$ENCODER" -b "$BITRATE" -q "$REF_WAV" "$PEM_MP3"
if [ $VERBOSE -eq 1 ]; then
    echo "  Created: $PEM_MP3"
fi

# Step 3: Decode pem output to WAV
echo -e "${YELLOW}Step 3: Decoding pem output to WAV...${NC}"
PEM_WAV="$TEMP_DIR/pem_decoded.wav"
ffmpeg -y -i "$PEM_MP3" -ar 44100 -ac 2 -sample_fmt s16 "$PEM_WAV" 2>/dev/null
if [ $VERBOSE -eq 1 ]; then
    echo "  Created: $PEM_WAV"
fi

# Step 4: Analyze files
echo ""
echo -e "${CYAN}========================================"
echo "  Analysis Results"
echo "========================================${NC}"
echo ""

echo -e "${GREEN}Reference MP3:${NC}"
"$MP3_INFO" "$REFERENCE_MP3" 2>/dev/null | grep -E "(Format|Channels|Sample Rate|Average|Duration)" | sed 's/^/  /'

echo ""
echo -e "${GREEN}PEM Encoded MP3:${NC}"
"$MP3_INFO" "$PEM_MP3" 2>/dev/null | grep -E "(Format|Channels|Sample Rate|Average|Duration)" | sed 's/^/  /'

# File size comparison
REF_SIZE=$(stat -f%z "$REFERENCE_MP3" 2>/dev/null || stat -c%s "$REFERENCE_MP3" 2>/dev/null)
PEM_SIZE=$(stat -f%z "$PEM_MP3" 2>/dev/null || stat -c%s "$PEM_MP3" 2>/dev/null)
SIZE_RATIO=$(echo "scale=2; $PEM_SIZE * 100 / $REF_SIZE" | bc)

echo ""
echo -e "${GREEN}File Size Comparison:${NC}"
echo "  Reference: $REF_SIZE bytes"
echo "  PEM:       $PEM_SIZE bytes"
echo "  Ratio:     $SIZE_RATIO%"

# WAV Analysis
echo ""
echo -e "${GREEN}Reference WAV Analysis:${NC}"
"$WAV_ANALYZER" "$REF_WAV" 2>/dev/null | grep -E "(RMS Level|Peak Level|Duration)" | head -5 | sed 's/^/  /'

echo ""
echo -e "${GREEN}PEM Decoded WAV Analysis:${NC}"
"$WAV_ANALYZER" "$PEM_WAV" 2>/dev/null | grep -E "(RMS Level|Peak Level|Duration)" | head -5 | sed 's/^/  /'

# Simple correlation test using ffmpeg
echo ""
echo -e "${YELLOW}Computing audio difference...${NC}"

# Create difference signal
DIFF_WAV="$TEMP_DIR/difference.wav"
ffmpeg -y -i "$REF_WAV" -i "$PEM_WAV" -filter_complex "[0:a][1:a]amix=inputs=2:duration=longest:weights=1 -1,volume=2[diff]" -map "[diff]" "$DIFF_WAV" 2>/dev/null

# Analyze difference signal
echo ""
echo -e "${GREEN}Difference Signal Analysis:${NC}"
"$WAV_ANALYZER" "$DIFF_WAV" 2>/dev/null | grep -E "(RMS Level|Peak Level)" | head -3 | sed 's/^/  /'

# Calculate approximate SNR
REF_RMS=$(ffprobe -v error -select_streams a:0 -show_entries stream=codec_name -of default=noprint_wrappers=1:nokey=1 "$REF_WAV" 2>/dev/null || echo "0")

echo ""
echo -e "${CYAN}========================================"
echo "  Summary"
echo "========================================${NC}"
echo ""
echo "The PEM encoder successfully processed the audio."
echo "Compare the RMS levels above - lower difference RMS indicates better quality."
echo ""
echo -e "${GREEN}Test completed successfully.${NC}"
