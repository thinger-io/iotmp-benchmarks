#!/usr/bin/env bash
# ═══════════════════════════════════════════════════════════════════════
# ESP-IDF Encoding Benchmark Builder
#
# Builds PSON, NanoCBOR, and TinyCBOR encoding benchmarks for ESP32 and
# compares binary sizes. All projects encode/decode the same data structure.
#
# Requirements:
#   - Docker
#   - NanoCBOR cloned in cbor/components/nanocbor/nanocbor/
#     (run: cd cbor/components/nanocbor && git clone https://github.com/bergzand/NanoCBOR.git nanocbor)
#   - TinyCBOR cloned in tinycbor/components/tinycbor/tinycbor/
#     (run: cd tinycbor/components/tinycbor && git clone https://github.com/intel/tinycbor.git tinycbor)
#
# Usage:
#   ./build.sh              # Build all
#   ./build.sh pson         # Build only PSON
#   ./build.sh cbor         # Build only CBOR (NanoCBOR)
#   ./build.sh tinycbor     # Build only CBOR (TinyCBOR)
# ═══════════════════════════════════════════════════════════════════════
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
IDF_DOCKER="espressif/idf:v5.4"

# Projects to build (all by default, or from args)
if [ $# -gt 0 ]; then
    PROJECTS=("$@")
else
    PROJECTS=(pson cbor tinycbor)
fi

echo "════════════════════════════════════════════════════════════════"
echo "  ESP-IDF Encoding Benchmark Builder"
echo "  Target: ESP32 | Framework: ESP-IDF v5.4 | Docker: $IDF_DOCKER"
echo "  Projects: ${PROJECTS[*]}"
echo "════════════════════════════════════════════════════════════════"

# ─── Pre-flight checks ───────────────────────────────────────────────

for proj in "${PROJECTS[@]}"; do
    if [ ! -d "$SCRIPT_DIR/$proj" ]; then
        echo "ERROR: Directory $SCRIPT_DIR/$proj does not exist"
        exit 1
    fi
done

# Check NanoCBOR is cloned if building cbor
for proj in "${PROJECTS[@]}"; do
    if [ "$proj" = "cbor" ]; then
        NANOCBOR_DIR="$SCRIPT_DIR/cbor/components/nanocbor/nanocbor"
        if [ ! -d "$NANOCBOR_DIR/src" ]; then
            echo ""
            echo "NanoCBOR not found. Cloning..."
            git clone https://github.com/bergzand/NanoCBOR.git "$NANOCBOR_DIR"
        fi
    fi
    if [ "$proj" = "tinycbor" ]; then
        TINYCBOR_DIR="$SCRIPT_DIR/tinycbor/components/tinycbor/tinycbor"
        if [ ! -d "$TINYCBOR_DIR/src" ]; then
            echo ""
            echo "TinyCBOR not found. Cloning..."
            git clone https://github.com/intel/tinycbor.git "$TINYCBOR_DIR"
        fi
    fi
done

# ─── Build function ──────────────────────────────────────────────────

PSON_SIZE="—"
CBOR_SIZE="—"
TINYCBOR_SIZE="—"

build_encoding() {
    local name=$1
    local dir="$SCRIPT_DIR/$name"

    echo ""
    echo "=== Building $name ==="

    docker run --rm \
        -v "$dir:/project" \
        -w /project \
        $IDF_DOCKER bash -c "
            rm -rf build sdkconfig
            idf.py set-target esp32
            idf.py build 2>&1
            echo '=== SIZE ==='
            idf.py size 2>&1
            echo '=== SIZE-COMPONENTS ==='
            idf.py size-components 2>&1
        " 2>&1 | tee "$SCRIPT_DIR/${name}_encoding_build.log"

    # Extract image size
    local size
    size=$(grep "Total image size" "$SCRIPT_DIR/${name}_encoding_build.log" 2>/dev/null | grep -o '[0-9]* bytes' || echo "FAILED")

    if [ "$name" = "pson" ]; then PSON_SIZE="$size"; fi
    if [ "$name" = "cbor" ]; then CBOR_SIZE="$size"; fi
    if [ "$name" = "tinycbor" ]; then TINYCBOR_SIZE="$size"; fi

    if [ "$size" != "FAILED" ]; then
        echo "  OK: $name: $size"
    else
        echo "  FAILED: $name (check ${name}_encoding_build.log)"
    fi
}

# ─── Build each project ──────────────────────────────────────────────

for proj in "${PROJECTS[@]}"; do
    case $proj in
        pson)
            build_encoding "pson"
            ;;
        cbor)
            build_encoding "cbor"
            ;;
        tinycbor)
            build_encoding "tinycbor"
            ;;
        *)
            echo "Unknown project: $proj (expected: pson, cbor, tinycbor)"
            ;;
    esac
done

# ─── Count source lines ──────────────────────────────────────────────

count_lines() {
    local file=$1
    if [ -f "$file" ]; then
        wc -l < "$file" | tr -d ' '
    else
        echo "—"
    fi
}

PSON_MAIN_LINES=$(count_lines "$SCRIPT_DIR/pson/main/main.c")
PSON_ENC_LINES=$(count_lines "$SCRIPT_DIR/pson/main/pson_encode.h")
PSON_DEC_LINES=$(count_lines "$SCRIPT_DIR/pson/main/pson_decode.h")
CBOR_MAIN_LINES=$(count_lines "$SCRIPT_DIR/cbor/main/main.c")
TINYCBOR_MAIN_LINES=$(count_lines "$SCRIPT_DIR/tinycbor/main/main.c")

# ─── Results summary ─────────────────────────────────────────────────

{
echo ""
echo "════════════════════════════════════════════════════════════════"
echo "  ENCODING BENCHMARK — BUILD RESULTS"
echo "════════════════════════════════════════════════════════════════"
echo ""
echo "  Environment:"
echo "    Docker:    $IDF_DOCKER"
echo "    Target:    ESP32"
echo "    Payload:   {\"temperature\": 23.5, \"humidity\": 60, \"pressure\": 1013, \"label\": \"outdoor\"}"
echo ""

printf "  %-10s  %15s  %10s  %s\n" "Encoding" "Image Size" "main.c" "Notes"
printf "  %-10s  %15s  %10s  %s\n" "────────" "──────────" "──────" "─────"
printf "  %-10s  %15s  %10s  %s\n" "PSON" "$PSON_SIZE" "${PSON_MAIN_LINES} lines" "Standalone encoder+decoder (${PSON_ENC_LINES}+${PSON_DEC_LINES} lines)"
printf "  %-10s  %15s  %10s  %s\n" "CBOR" "$CBOR_SIZE" "${CBOR_MAIN_LINES} lines" "NanoCBOR library"
printf "  %-10s  %15s  %10s  %s\n" "TinyCBOR" "$TINYCBOR_SIZE" "${TINYCBOR_MAIN_LINES} lines" "TinyCBOR (Intel) library"

echo ""
echo "  PSON standalone implementation:"
echo "    pson_encode.h:  ${PSON_ENC_LINES} lines"
echo "    pson_decode.h:  ${PSON_DEC_LINES} lines"
echo "    Total:          $((${PSON_ENC_LINES:-0} + ${PSON_DEC_LINES:-0})) lines (full encode+decode)"
echo ""
echo "  Generated: $(date -u '+%Y-%m-%d %H:%M UTC')"

} 2>&1 | tee "$SCRIPT_DIR/encoding_results.txt"
