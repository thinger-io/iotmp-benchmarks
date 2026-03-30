# Protocol Benchmarks (ESP32)

Compares full IoT protocol stacks on real embedded hardware (ESP32) using ESP-IDF v5.4. All programs implement the same functionality:

- Connect to WiFi and authenticate with server/broker
- Define an LED actuator (input resource / command handler)
- Define an environment sensor (output: temperature + humidity)
- Define an uptime resource
- Define a reboot action
- Handle communication in the main loop

## Protocols tested

| Directory | Protocol | Library |
|---|---|---|
| `iotmp/` | IOTMP | [iotmp-espidf](https://github.com/thinger-io/iotmp-espidf) + [iotmp-embedded](https://github.com/thinger-io/iotmp-embedded) |
| `mqtt/` | MQTT 3.1.1 | esp_mqtt (ESP-IDF built-in) + cJSON |
| `coap/` | CoAP | libcoap (espressif/coap component) |
| `http/` | HTTP | esp_http_client (ESP-IDF built-in) + cJSON |

## Results

| Protocol | Image Size | Lines of Code | Library |
|---|---:|---:|---|
| **IOTMP** | **867,040 B** | **48** | iotmp-espidf + iotmp-embedded |
| MQTT | 907,776 B | 122 | esp_mqtt (built-in) + cJSON |
| HTTP | 909,348 B | 109 | esp_http_client (built-in) + cJSON |
| CoAP | 952,896 B | 128 | libcoap (espressif/coap component) |

### Component Breakdown

| Component | IOTMP | MQTT | CoAP | HTTP |
|---|---:|---:|---:|---:|
| Protocol library | 13 KB | 14 KB | 83 KB | 24 KB |
| TLS/Crypto (mbedTLS) | ~30 KB | ~30 KB | ~30 KB | ~30 KB |

All four protocols produce binaries within 10% of each other. The ESP-IDF base (WiFi, TCP/IP, FreeRTOS, mbedTLS) dominates at ~830+ KB; protocol libraries add 13-83 KB on top. IOTMP requires 2.3-2.7x fewer lines of application code while providing more protocol-level features (resource discovery, schema introspection, server-controlled streaming, compact mode).

## Shared code

All programs use the same WiFi initialization (`common_wifi.h`) to ensure an identical baseline. All programs include TLS support (mbedTLS) as part of the ESP-IDF component dependency chain.

## How to reproduce

### Automated (recommended)

Run the build script to build all four protocols and extract sizes:

```bash
cd protocol/
./build.sh
```

This uses Docker (`espressif/idf:v5.4`) and produces `results.txt` with a formatted comparison table.

To build only specific protocols:

```bash
./build.sh iotmp mqtt    # Build only IOTMP and MQTT
```

### IOTMP library paths

The IOTMP build requires the iotmp-espidf and iotmp-embedded libraries on the host. The build script mounts them into the Docker container at `/components/`. Configure the paths via environment variables or edit the defaults at the top of `build.sh`:

```bash
# Default paths (edit build.sh or set env vars):
export IOTMP_ESPIDF_LIB="$HOME/Desarrollos/iotmp-espidf"
export IOTMP_EMBEDDED_LIB="$HOME/Desarrollos/iotmp-embedded"
./build.sh
```

The IOTMP project's `CMakeLists.txt` references these as:
```cmake
set(EXTRA_COMPONENT_DIRS "/components/iotmp-espidf" "/components/iotmp-embedded")
```

### Manual (single protocol)

Each subdirectory is a standalone ESP-IDF project. Build individually with Docker:

**MQTT / HTTP / CoAP** (no external dependencies):

```bash
docker run --rm -v $(pwd)/mqtt:/project -w /project espressif/idf:v5.4 \
    bash -c "rm -rf build sdkconfig && idf.py set-target esp32 && idf.py build && idf.py size"
```

Replace `mqtt` with `coap` or `http` for other protocols.

**IOTMP** (requires library mounts):

```bash
docker run --rm \
    -v $(pwd)/iotmp:/project \
    -v $HOME/Desarrollos/iotmp-espidf:/components/iotmp-espidf:ro \
    -v $HOME/Desarrollos/iotmp-embedded:/components/iotmp-embedded:ro \
    -w /project espressif/idf:v5.4 \
    bash -c "rm -rf build sdkconfig && idf.py set-target esp32 && idf.py build && idf.py size"
```

### Extracting component sizes

After building, use `idf.py size-components` to get per-library breakdowns:

```bash
docker run --rm -v $(pwd)/iotmp:/project \
    -v $HOME/Desarrollos/iotmp-espidf:/components/iotmp-espidf:ro \
    -v $HOME/Desarrollos/iotmp-embedded:/components/iotmp-embedded:ro \
    -w /project espressif/idf:v5.4 \
    bash -c "idf.py size-components"
```

This shows the contribution of each ESP-IDF component (e.g., `libiotmp-espidf.a`, `libmbedtls.a`, `liblwip.a`) to the total image size.
