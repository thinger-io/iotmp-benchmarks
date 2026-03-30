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
| MQTT | 907,776 B | 122 | esp_mqtt (built-in) + cJSON |
| HTTP | 909,348 B | 109 | esp_http_client (built-in) + cJSON |
| **IOTMP** | **938,844 B** | **48** | iotmp-espidf + iotmp-embedded |
| CoAP | 952,896 B | 128 | libcoap (espressif/coap component) |

All four protocols produce binaries within 5% of each other. The ESP-IDF base (WiFi, TCP/IP, FreeRTOS) dominates at ~880+ KB; protocol libraries add 25-70 KB on top. IOTMP requires 2.3-2.9x fewer lines of application code while providing more protocol-level features (resource discovery, schema introspection, server-controlled streaming, compact mode).

## Shared code

All programs use the same WiFi initialization (`common_wifi.h`) to ensure an identical baseline. No TLS is enabled in any program.

## How to build

Each subdirectory is a standalone ESP-IDF project. Build with Docker:

```bash
docker run --rm -v $(pwd)/iotmp:/project -w /project espressif/idf:v5.4 \
    bash -c "idf.py set-target esp32 && idf.py build && idf.py size"
```

Replace `iotmp` with `mqtt`, `coap`, or `http` for other protocols.
