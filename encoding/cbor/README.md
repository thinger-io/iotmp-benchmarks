# CBOR (NanoCBOR) Encoding Benchmark

## Setup

Clone NanoCBOR into the component directory before building:

```bash
cd components/nanocbor
git clone https://github.com/bergzand/NanoCBOR.git nanocbor
```

This gives the layout:

```
components/nanocbor/
  CMakeLists.txt          <- ESP-IDF component wrapper (already present)
  nanocbor/               <- Cloned repository
    include/nanocbor/
      nanocbor.h
    src/
      encoder.c
      decoder.c
```

## Build

```bash
# Using Docker (same as other benchmarks)
docker run --rm -v $(pwd):/project -w /project espressif/idf:v5.4 \
    bash -c "idf.py set-target esp32 && idf.py build"
```

Or use the `build_encoding_benchmarks.sh` script in the parent directory.
