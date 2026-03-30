/*
 * pson_encode.h — Standalone minimal PSON encoder in pure C
 *
 * PSON tag format: (wire_type << 5) | inline_value
 *   - If inline_value < 31: single byte tag
 *   - If inline_value >= 31: tag byte with 0x1F, followed by varint
 *
 * Wire types:
 *   0 = unsigned integer
 *   1 = signed integer (magnitude)
 *   2 = floating point (inline 0=float32, 1=float64)
 *   3 = discrete (0=false, 1=true, 2=null)
 *   4 = string (inline/varint = length, followed by UTF-8 bytes)
 *   5 = bytes  (inline/varint = length, followed by raw bytes)
 *   6 = map    (inline/varint = number of key-value pairs)
 *   7 = array  (inline/varint = number of elements)
 *
 * Varint: protobuf-style, 7 bits per byte, MSB = continuation bit
 * Floats: little-endian IEEE 754
 * Float-to-integer promotion: whole-number floats encode as unsigned/signed
 */

#ifndef PSON_ENCODE_H
#define PSON_ENCODE_H

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <math.h>

/* Wire type constants */
#define PSON_WIRE_UNSIGNED  0
#define PSON_WIRE_SIGNED    1
#define PSON_WIRE_FLOAT     2
#define PSON_WIRE_DISCRETE  3
#define PSON_WIRE_STRING    4
#define PSON_WIRE_BYTES     5
#define PSON_WIRE_MAP       6
#define PSON_WIRE_ARRAY     7

/* Encoder state */
typedef struct {
    uint8_t *buf;
    size_t   cap;
    size_t   pos;
} pson_encoder_t;

static inline void pson_encoder_init(pson_encoder_t *enc, uint8_t *buf, size_t cap) {
    enc->buf = buf;
    enc->cap = cap;
    enc->pos = 0;
}

static inline size_t pson_encoded_size(const pson_encoder_t *enc) {
    return enc->pos;
}

/* Write raw byte */
static inline int pson_write_byte(pson_encoder_t *enc, uint8_t b) {
    if (enc->pos >= enc->cap) return -1;
    enc->buf[enc->pos++] = b;
    return 0;
}

/* Write raw bytes */
static inline int pson_write_raw(pson_encoder_t *enc, const void *data, size_t len) {
    if (enc->pos + len > enc->cap) return -1;
    memcpy(enc->buf + enc->pos, data, len);
    enc->pos += len;
    return 0;
}

/* Write varint (protobuf-style) */
static inline int pson_write_varint(pson_encoder_t *enc, uint64_t value) {
    do {
        uint8_t b = (uint8_t)(value & 0x7F);
        value >>= 7;
        if (value > 0) b |= 0x80;
        if (pson_write_byte(enc, b) != 0) return -1;
    } while (value > 0);
    return 0;
}

/* Write tag byte: (wire_type << 5) | inline_value */
static inline int pson_write_tag(pson_encoder_t *enc, uint8_t wire_type, uint64_t value) {
    if (value < 0x1F) {
        return pson_write_byte(enc, (uint8_t)((wire_type << 5) | value));
    }
    if (pson_write_byte(enc, (uint8_t)((wire_type << 5) | 0x1F)) != 0) return -1;
    return pson_write_varint(enc, value);
}

/* Encode unsigned integer */
static inline int pson_encode_uint(pson_encoder_t *enc, uint64_t value) {
    return pson_write_tag(enc, PSON_WIRE_UNSIGNED, value);
}

/* Encode signed integer (negative values only; magnitude encoded) */
static inline int pson_encode_int(pson_encoder_t *enc, int64_t value) {
    if (value >= 0) {
        return pson_write_tag(enc, PSON_WIRE_UNSIGNED, (uint64_t)value);
    }
    return pson_write_tag(enc, PSON_WIRE_SIGNED, (uint64_t)(-value));
}

/* Encode float32 */
static inline int pson_encode_float(pson_encoder_t *enc, float value) {
    /* Float-to-integer promotion for whole numbers */
    int64_t iv = (int64_t)value;
    if ((float)iv == value) {
        return pson_encode_int(enc, iv);
    }
    if (pson_write_byte(enc, (uint8_t)((PSON_WIRE_FLOAT << 5) | 0)) != 0) return -1;
    return pson_write_raw(enc, &value, sizeof(float));
}

/* Encode float64 */
static inline int pson_encode_double(pson_encoder_t *enc, double value) {
    /* Float-to-integer promotion for whole numbers */
    int64_t iv = (int64_t)value;
    if ((double)iv == value) {
        return pson_encode_int(enc, iv);
    }
    /* Check if float32 precision is sufficient */
    float fv = (float)value;
    if ((double)fv == value) {
        if (pson_write_byte(enc, (uint8_t)((PSON_WIRE_FLOAT << 5) | 0)) != 0) return -1;
        return pson_write_raw(enc, &fv, sizeof(float));
    }
    if (pson_write_byte(enc, (uint8_t)((PSON_WIRE_FLOAT << 5) | 1)) != 0) return -1;
    return pson_write_raw(enc, &value, sizeof(double));
}

/* Encode string (length-prefixed) */
static inline int pson_encode_string(pson_encoder_t *enc, const char *str, size_t len) {
    if (pson_write_tag(enc, PSON_WIRE_STRING, len) != 0) return -1;
    return pson_write_raw(enc, str, len);
}

/* Encode string (null-terminated convenience) */
static inline int pson_encode_str(pson_encoder_t *enc, const char *str) {
    return pson_encode_string(enc, str, strlen(str));
}

/* Begin a map with a known number of key-value pairs */
static inline int pson_encode_map(pson_encoder_t *enc, uint64_t count) {
    return pson_write_tag(enc, PSON_WIRE_MAP, count);
}

/* Begin an array with a known number of elements */
static inline int pson_encode_array(pson_encoder_t *enc, uint64_t count) {
    return pson_write_tag(enc, PSON_WIRE_ARRAY, count);
}

/* Encode boolean */
static inline int pson_encode_bool(pson_encoder_t *enc, int value) {
    return pson_write_byte(enc, (uint8_t)((PSON_WIRE_DISCRETE << 5) | (value ? 1 : 0)));
}

/* Encode null */
static inline int pson_encode_null(pson_encoder_t *enc) {
    return pson_write_byte(enc, (uint8_t)((PSON_WIRE_DISCRETE << 5) | 2));
}

#endif /* PSON_ENCODE_H */
