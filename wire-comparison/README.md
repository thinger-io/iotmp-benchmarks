# Wire-Level Frame Comparison

This benchmark constructs real binary frames for IOTMP, MQTT (v3.1.1 and v5), CoAP, LwM2M, and HTTP/2, then compares their exact byte sizes for common IoT scenarios.

IOTMP frames use the actual PSON encoder from [iotmp-embedded](https://github.com/thinger-io/iotmp-embedded). MQTT frames are built manually according to the OASIS specification. CoAP, LwM2M, and HTTP/2 frames are constructed per their respective RFCs.

## How to build

```bash
c++ -std=c++17 -I<path-to-iotmp-embedded>/include -o wire_comparison wire_comparison.cpp
./wire_comparison
```

Requires the [iotmp-embedded](https://github.com/thinger-io/iotmp-embedded) headers.

## What it measures

- Session establishment overhead
- Single telemetry sample (normal and compact modes)
- 100-sample streaming lifecycle
- RPC round-trip (read device value)
- API discovery
- Stream control (start/stop)
- Keepalive cost over 1 hour

Results are in [RESULTS.md](../RESULTS.md).
