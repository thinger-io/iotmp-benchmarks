#!/usr/bin/env bash
# ═══════════════════════════════════════════════════════════════════════
# ESP-IDF Protocol Benchmark Builder
#
# Builds IOTMP, MQTT, HTTP, and CoAP protocol benchmarks for ESP32 and
# compares binary sizes + component breakdowns. All programs implement
# the same IoT functionality (sensor, actuator, uptime, reboot).
#
# Requirements:
#   - Docker
#   - For IOTMP: iotmp-espidf and iotmp-embedded libraries on host
#
# Usage:
#   ./build.sh              # Build all
#   ./build.sh iotmp        # Build only IOTMP
#   ./build.sh mqtt coap    # Build MQTT and CoAP
# ═══════════════════════════════════════════════════════════════════════
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
IDF_DOCKER="espressif/idf:v5.4"

# ─── Configurable host paths for IOTMP libraries ─────────────────────
# Override these environment variables or edit the defaults below.
IOTMP_ESPIDF_LIB="${IOTMP_ESPIDF_LIB:-}"
IOTMP_EMBEDDED_LIB="${IOTMP_EMBEDDED_LIB:-}"

# ─── Validate IOTMP library paths ────────────────────────────────────
needs_iotmp=false
for arg in "$@"; do [ "$arg" = "iotmp" ] && needs_iotmp=true; done
[ $# -eq 0 ] && needs_iotmp=true

if $needs_iotmp; then
    if [ -z "$IOTMP_ESPIDF_LIB" ] || [ -z "$IOTMP_EMBEDDED_LIB" ]; then
        echo "ERROR: IOTMP library paths not set."
        echo "Clone the IOTMP libraries and set:"
        echo "  export IOTMP_ESPIDF_LIB=/path/to/iotmp-espidf"
        echo "  export IOTMP_EMBEDDED_LIB=/path/to/iotmp-embedded"
        echo ""
        echo "Or skip IOTMP:  ./build.sh mqtt http coap"
        exit 1
    fi
    if [ ! -d "$IOTMP_ESPIDF_LIB" ] || [ ! -d "$IOTMP_EMBEDDED_LIB" ]; then
        echo "ERROR: IOTMP library paths not found:"
        echo "  IOTMP_ESPIDF_LIB=$IOTMP_ESPIDF_LIB"
        echo "  IOTMP_EMBEDDED_LIB=$IOTMP_EMBEDDED_LIB"
        exit 1
    fi
fi

# ─── Projects to build ───────────────────────────────────────────────
if [ $# -gt 0 ]; then
    PROJECTS=""
    for arg in "$@"; do
        PROJECTS="$PROJECTS $arg"
    done
else
    PROJECTS="iotmp mqtt http coap"
fi

echo "════════════════════════════════════════════════════════════════"
echo "  ESP-IDF Protocol Benchmark Builder"
echo "  Target: ESP32 | Framework: ESP-IDF v5.4 | Docker: $IDF_DOCKER"
echo "  Projects:$PROJECTS"
echo "════════════════════════════════════════════════════════════════"

# ─── Pre-flight checks ───────────────────────────────────────────────

for proj in $PROJECTS; do
    if [ ! -d "$SCRIPT_DIR/$proj" ]; then
        echo "ERROR: Directory $SCRIPT_DIR/$proj does not exist"
        exit 1
    fi
done

# Check IOTMP library paths if building iotmp
for proj in $PROJECTS; do
    if [ "$proj" = "iotmp" ]; then
        if [ ! -d "$IOTMP_ESPIDF_LIB" ]; then
            echo "ERROR: IOTMP ESP-IDF library not found at: $IOTMP_ESPIDF_LIB"
            echo "  Set IOTMP_ESPIDF_LIB to the correct path."
            exit 1
        fi
        if [ ! -d "$IOTMP_EMBEDDED_LIB" ]; then
            echo "ERROR: IOTMP embedded library not found at: $IOTMP_EMBEDDED_LIB"
            echo "  Set IOTMP_EMBEDDED_LIB to the correct path."
            exit 1
        fi
        echo "  IOTMP libraries:"
        echo "    iotmp-espidf:    $IOTMP_ESPIDF_LIB"
        echo "    iotmp-embedded:  $IOTMP_EMBEDDED_LIB"
    fi
done

# ─── Result variables (simple bash variables, macOS 3.x compatible) ──

IOTMP_TOTAL_SIZE=""
MQTT_TOTAL_SIZE=""
HTTP_TOTAL_SIZE=""
COAP_TOTAL_SIZE=""

IOTMP_PROTO_KB=""
MQTT_PROTO_KB=""
HTTP_PROTO_KB=""
COAP_PROTO_KB=""

IOTMP_TLS_KB=""
MQTT_TLS_KB=""
HTTP_TLS_KB=""
COAP_TLS_KB=""

# ─── Build function ──────────────────────────────────────────────────

build_protocol() {
    local name=$1
    local dir="$SCRIPT_DIR/$name"
    local logfile="$SCRIPT_DIR/${name}_build.log"

    echo ""
    echo "=== Building $name ==="

    # Construct Docker volume mounts
    local docker_volumes="-v $dir:/project"

    if [ "$name" = "iotmp" ]; then
        # Mount IOTMP libraries into the container at /components/
        docker_volumes="$docker_volumes -v $IOTMP_ESPIDF_LIB:/components/iotmp-espidf:ro"
        docker_volumes="$docker_volumes -v $IOTMP_EMBEDDED_LIB:/components/iotmp-embedded:ro"
    fi

    docker run --rm \
        $docker_volumes \
        -w /project \
        $IDF_DOCKER bash -c "
            rm -rf build sdkconfig
            idf.py set-target esp32
            idf.py build 2>&1
            echo '=== SIZE OUTPUT ==='
            idf.py size 2>&1
            echo '=== SIZE-COMPONENTS OUTPUT ==='
            idf.py size-components 2>&1
        " 2>&1 | tee "$logfile"

    # ── Extract total image size ──
    local total_size
    total_size=$(grep "Total image size" "$logfile" 2>/dev/null | grep -o '[0-9]* bytes' | head -1 || echo "")

    if [ -z "$total_size" ]; then
        echo "  FAILED: $name (check $logfile)"
        return 1
    fi

    echo "  Total image size: $total_size"

    # ── Extract component sizes ──
    # Protocol library size
    local proto_size=""
    case $name in
        iotmp)
            # Look for libiotmp-espidf.a in size-components output
            proto_size=$(grep "libiotmp-espidf.a" "$logfile" 2>/dev/null | awk '{print $2}' | head -1 || echo "")
            ;;
        mqtt)
            # Look for libmqtt.a or libesp_mqtt.a
            proto_size=$(grep -E "lib(esp_)?mqtt\.a" "$logfile" 2>/dev/null | awk '{print $2}' | head -1 || echo "")
            ;;
        http)
            # Look for libesp_http_client.a
            proto_size=$(grep "libesp_http_client.a" "$logfile" 2>/dev/null | awk '{print $2}' | head -1 || echo "")
            ;;
        coap)
            # Look for libcoap or liblibcoap
            proto_size=$(grep -i "libcoap" "$logfile" 2>/dev/null | awk '{print $2}' | head -1 || echo "")
            ;;
    esac

    # mbedTLS size
    local tls_size
    tls_size=$(grep "libmbedtls.a" "$logfile" 2>/dev/null | awk '{print $2}' | head -1 || echo "")

    # Format sizes to KB
    local proto_kb=""
    if [ -n "$proto_size" ] && [ "$proto_size" != "0" ]; then
        proto_kb=$(echo "$proto_size" | awk '{printf "%.0f", $1/1024}')
    fi

    local tls_kb=""
    if [ -n "$tls_size" ] && [ "$tls_size" != "0" ]; then
        tls_kb=$(echo "$tls_size" | awk '{printf "%.0f", $1/1024}')
    fi

    echo "  Protocol library: ${proto_kb:-?} KB"
    echo "  TLS (mbedTLS):    ${tls_kb:-?} KB"

    # Store results in the appropriate variables
    local size_num
    size_num=$(echo "$total_size" | grep -o '[0-9]*')

    case $name in
        iotmp)
            IOTMP_TOTAL_SIZE="$size_num"
            IOTMP_PROTO_KB="$proto_kb"
            IOTMP_TLS_KB="$tls_kb"
            ;;
        mqtt)
            MQTT_TOTAL_SIZE="$size_num"
            MQTT_PROTO_KB="$proto_kb"
            MQTT_TLS_KB="$tls_kb"
            ;;
        http)
            HTTP_TOTAL_SIZE="$size_num"
            HTTP_PROTO_KB="$proto_kb"
            HTTP_TLS_KB="$tls_kb"
            ;;
        coap)
            COAP_TOTAL_SIZE="$size_num"
            COAP_PROTO_KB="$proto_kb"
            COAP_TLS_KB="$tls_kb"
            ;;
    esac
}

# ─── Build each project ──────────────────────────────────────────────

for proj in $PROJECTS; do
    case $proj in
        iotmp|mqtt|http|coap)
            build_protocol "$proj"
            ;;
        *)
            echo "Unknown project: $proj (expected: iotmp, mqtt, http, coap)"
            ;;
    esac
done

# ─── Count application lines of code ─────────────────────────────────

count_lines() {
    local file=$1
    if [ -f "$file" ]; then
        wc -l < "$file" | tr -d ' '
    else
        echo "?"
    fi
}

IOTMP_LOC=$(count_lines "$SCRIPT_DIR/iotmp/main/main.cpp")
MQTT_LOC=$(count_lines "$SCRIPT_DIR/mqtt/main/main.c")
HTTP_LOC=$(count_lines "$SCRIPT_DIR/http/main/main.c")
COAP_LOC=$(count_lines "$SCRIPT_DIR/coap/main/main.c")

# ─── Format helper ───────────────────────────────────────────────────

fmt_size() {
    local val=$1
    if [ -n "$val" ] && [ "$val" != "?" ]; then
        printf "%'d B" "$val" 2>/dev/null || printf "%d B" "$val"
    else
        echo "—"
    fi
}

# ─── Results summary ─────────────────────────────────────────────────

RESULTS_FILE="$SCRIPT_DIR/results.txt"

{
echo ""
echo "════════════════════════════════════════════════════════════════"
echo "  PROTOCOL BENCHMARK — BUILD RESULTS"
echo "════════════════════════════════════════════════════════════════"
echo ""
echo "  Environment:"
echo "    Docker:    $IDF_DOCKER"
echo "    Target:    ESP32"
echo ""
echo "  ── Total Image Size ──"
echo ""
printf "  %-10s  %15s  %8s  %s\n" "Protocol" "Image Size" "LOC" "Library"
printf "  %-10s  %15s  %8s  %s\n" "────────" "──────────" "───" "───────"
printf "  %-10s  %15s  %8s  %s\n" "IOTMP"  "$(fmt_size "$IOTMP_TOTAL_SIZE")" "$IOTMP_LOC" "iotmp-espidf + iotmp-embedded"
printf "  %-10s  %15s  %8s  %s\n" "MQTT"   "$(fmt_size "$MQTT_TOTAL_SIZE")"  "$MQTT_LOC"  "esp_mqtt (built-in) + cJSON"
printf "  %-10s  %15s  %8s  %s\n" "HTTP"   "$(fmt_size "$HTTP_TOTAL_SIZE")"  "$HTTP_LOC"  "esp_http_client (built-in) + cJSON"
printf "  %-10s  %15s  %8s  %s\n" "CoAP"   "$(fmt_size "$COAP_TOTAL_SIZE")" "$COAP_LOC"  "libcoap (espressif/coap component)"
echo ""
echo "  ── Component Breakdown ──"
echo ""
printf "  %-20s  %8s  %8s  %8s  %8s\n" "Component" "IOTMP" "MQTT" "CoAP" "HTTP"
printf "  %-20s  %8s  %8s  %8s  %8s\n" "────────────────────" "────────" "────────" "────────" "────────"
printf "  %-20s  %8s  %8s  %8s  %8s\n" "Protocol library" \
    "${IOTMP_PROTO_KB:+${IOTMP_PROTO_KB} KB}" \
    "${MQTT_PROTO_KB:+${MQTT_PROTO_KB} KB}" \
    "${COAP_PROTO_KB:+${COAP_PROTO_KB} KB}" \
    "${HTTP_PROTO_KB:+${HTTP_PROTO_KB} KB}"
printf "  %-20s  %8s  %8s  %8s  %8s\n" "TLS/Crypto (mbedTLS)" \
    "${IOTMP_TLS_KB:+~${IOTMP_TLS_KB} KB}" \
    "${MQTT_TLS_KB:+~${MQTT_TLS_KB} KB}" \
    "${COAP_TLS_KB:+~${COAP_TLS_KB} KB}" \
    "${HTTP_TLS_KB:+~${HTTP_TLS_KB} KB}"
echo ""
echo "  Generated: $(date -u '+%Y-%m-%d %H:%M UTC')"
echo ""
} 2>&1 | tee "$RESULTS_FILE"

echo "Results saved to: $RESULTS_FILE"
