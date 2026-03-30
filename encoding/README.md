# Encoding Benchmarks (ESP32)

Compares binary encoding formats on real embedded hardware (ESP32) using ESP-IDF v5.4.

All three projects encode and decode the same data structure:

```json
{"temperature": 23.5, "humidity": 60, "pressure": 1013, "label": "outdoor"}
```

## Encoders tested

| Directory | Encoding | Library |
|---|---|---|
| `pson/` | PSON | Standalone encoder/decoder (344 lines total) |
| `cbor/` | CBOR | [NanoCBOR](https://github.com/bergzand/NanoCBOR) |
| `tinycbor/` | CBOR | [TinyCBOR](https://github.com/intel/tinycbor) (Intel) |

## Results

| Encoding | Image Size | Encoding Code | Wire Size | Encode | Decode |
|---|---:|---:|---:|---:|---:|
| PSON | 195,044 B | 2,589 B | 55 B | 10.00 us | 5.93 us |
| NanoCBOR | 196,076 B | 3,816 B | 53 B | 19.65 us | 16.86 us |
| TinyCBOR | 200,888 B | 6,604 B | 55 B | 20.04 us | 52.78 us |

## How to build

### Prerequisites

- Docker

### Build all encoders

```bash
cd encoding/
./build.sh
```

The script will automatically clone NanoCBOR and TinyCBOR if not present, then build each project inside Docker using `espressif/idf:v5.4`.

### Build a single encoder

```bash
./build.sh pson
./build.sh cbor
./build.sh tinycbor
```

Build logs are written to `*_encoding_build.log` and a summary to `encoding_results.txt`.
