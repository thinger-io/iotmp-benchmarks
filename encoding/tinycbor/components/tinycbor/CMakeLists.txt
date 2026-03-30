# ESP-IDF component wrapper for TinyCBOR
# Sources are expected in tinycbor/ subdirectory (cloned from GitHub)
idf_component_register(
    SRCS "tinycbor/src/cborencoder.c"
         "tinycbor/src/cborencoder_close_container_checked.c"
         "tinycbor/src/cborencoder_float.c"
         "tinycbor/src/cborerrorstrings.c"
         "tinycbor/src/cborparser.c"
         "tinycbor/src/cborparser_float.c"
    INCLUDE_DIRS "tinycbor/src"
)

# TinyCBOR expects generated headers; provide minimal stubs
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/tinycbor-version.h"
    "#define TINYCBOR_VERSION_MAJOR 0\n"
    "#define TINYCBOR_VERSION_MINOR 6\n"
    "#define TINYCBOR_VERSION_PATCH 0\n"
)
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/tinycbor-export.h"
    "#ifndef TINYCBOR_EXPORT_H\n"
    "#define TINYCBOR_EXPORT_H\n"
    "#define CBOR_API\n"
    "#define CBOR_PRIVATE_API\n"
    "#define CBOR_INLINE_API static inline\n"
    "#endif\n"
)
target_include_directories(${COMPONENT_LIB} PUBLIC "${CMAKE_CURRENT_BINARY_DIR}")
