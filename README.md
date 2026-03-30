# IOTMP Protocol Benchmarks

Reproducible benchmarks comparing [IOTMP](https://iotmp.io) against MQTT, CoAP, HTTP, and LwM2M for IoT communication. These benchmarks accompany the IETF Internet-Drafts:

- [draft-bustamante-iotmp](https://datatracker.ietf.org/doc/draft-bustamante-iotmp/) -- IoT Messaging Protocol (IOTMP)
- [draft-bustamante-pson](https://datatracker.ietf.org/doc/draft-bustamante-pson/) -- Packed Simple Object Notation (PSON)

## Repository structure

```
wire-comparison/    Wire-level frame size comparison (C++, runs on host)
encoding/           Binary encoding benchmarks: PSON vs NanoCBOR vs TinyCBOR (ESP32)
protocol/           Full protocol stack benchmarks: IOTMP vs MQTT vs CoAP vs HTTP (ESP32)
RESULTS.md          Complete results and analysis
```

## Summary results

### Wire-level frame comparison

All values are application-layer bytes only (no TCP/IP/UDP/TLS headers).

| Scenario | IOTMP | MQTT 3.1.1 | MQTT v5 | CoAP | LwM2M | HTTP/2 |
|---|---:|---:|---:|---:|---:|---:|
| Session establishment | 34 B | 43 B | 45 B | 0 B* | 80 B | 42 B |
| Single sample (normal) | 35 B | 64 B | 43 B | 63 B | 22 B | 101 B |
| Single sample (compact) | 14 B | 64 B | 43 B | 63 B | 22 B | 101 B |
| 100 samples (normal) | 3,816 B | 6,370 B | 4,295 B | 3,955 B | 2,233 B | 10,070 B |
| 100 samples (compact) | 1,737 B | 6,370 B | 4,295 B | 3,955 B | 2,233 B | 10,070 B |
| RPC: read device value | 40 B | 140 B | 147 B | 55 B | 34 B | 76 B |
| API discovery | 61 B | 0 B** | 0 B** | 117 B | 65 B | 0 B** |
| Stream control (start+stop) | 36 B | 161 B | 163 B | 44 B | 86 B | 0 B** |
| Keepalive (1 hour, 60s) | 120 B | 240 B | 240 B | 0 B* | 204 B | 2,040 B |

\* CoAP = 0: connectionless protocol over UDP, no session or keepalive at protocol level.
\** 0 = functionality not available at protocol level.

### Encoding comparison (ESP32, ESP-IDF v5.4)

Payload: `{"temperature": 23.5, "humidity": 60, "pressure": 1013, "label": "outdoor"}`

| Encoding | Image Size | Encoding Code | Wire Size | Encode | Decode | Implementation |
|---|---:|---:|---:|---:|---:|---:|
| PSON | 195,044 B | 2,589 B | 55 B | 10.00 us | 5.93 us | 344 lines |
| NanoCBOR | 196,076 B | 3,816 B | 53 B | 19.65 us | 16.86 us | 2,223 lines |
| TinyCBOR | 200,888 B | 6,604 B | 55 B | 20.04 us | 52.78 us | 5,619 lines |

PSON encodes 2x faster than CBOR and decodes 3-9x faster, with a standalone implementation that is 6-16x smaller.

### Protocol stack comparison (ESP32, ESP-IDF v5.4)

| Protocol | Image Size | Lines of Code | Library |
|---|---:|---:|---|
| MQTT | 907,776 B | 122 | esp_mqtt (built-in) + cJSON |
| HTTP | 909,348 B | 109 | esp_http_client (built-in) + cJSON |
| **IOTMP** | **938,844 B** | **48** | iotmp-espidf + iotmp-embedded |
| CoAP | 952,896 B | 128 | libcoap (espressif/coap component) |

All four protocols produce binaries within 5% of each other. IOTMP requires 2.3-2.9x fewer lines of application code while providing more protocol-level features (resource discovery, schema introspection, server-controlled streaming, compact mode).

### Qualitative capabilities

| Capability | IOTMP | MQTT 3.1.1 | MQTT v5 | CoAP | LwM2M | HTTP/2 |
|---|:---:|:---:|:---:|:---:|:---:|:---:|
| Resource discovery | Yes | -- | -- | Partial | Yes | -- |
| Schema introspection | Yes | -- | -- | -- | -- | -- |
| Bidirectional RPC | Yes | -- | Partial | Partial | Yes | -- |
| Server-controlled streaming | Yes | -- | -- | Partial | Partial | -- |
| Bidirectional data streams | Yes | -- | -- | -- | -- | -- |
| Built-in binary encoding | Yes | -- | -- | Partial | Yes | -- |

## How to reproduce

### Requirements

- Docker (for ESP-IDF builds)
- C++17 compiler (for wire comparison)
- [iotmp-embedded](https://github.com/thinger-io/iotmp-embedded) headers (for wire comparison)

### Wire comparison

```bash
cd wire-comparison/
c++ -std=c++17 -I<path-to-iotmp-embedded>/include -o wire_comparison wire_comparison.cpp
./wire_comparison
```

### Encoding benchmarks

```bash
cd encoding/
./build.sh
```

Uses Docker image `espressif/idf:v5.4`. The script automatically clones NanoCBOR and TinyCBOR dependencies, then builds all three projects for ESP32.

### Protocol benchmarks

Each subdirectory under `protocol/` is a standalone ESP-IDF project:

```bash
cd protocol/
docker run --rm -v $(pwd)/iotmp:/project -w /project espressif/idf:v5.4 \
    bash -c "idf.py set-target esp32 && idf.py build && idf.py size"
```

Replace `iotmp` with `mqtt`, `coap`, or `http` for other protocols.

## License

MIT -- see [LICENSE](LICENSE).
