# ESP-IDF component wrapper for NanoCBOR
# Sources are expected in nanocbor/ subdirectory (cloned from GitHub)
idf_component_register(
    SRCS "nanocbor/src/encoder.c"
         "nanocbor/src/decoder.c"
    INCLUDE_DIRS "nanocbor/include"
)
