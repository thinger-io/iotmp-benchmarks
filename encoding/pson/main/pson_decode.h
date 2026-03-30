/*
 * pson_decode.h — Standalone minimal PSON decoder in pure C
 *
 * Reads PSON-encoded data from a buffer and provides a callback/iterator
 * style API plus direct value extraction helpers.
 */

#ifndef PSON_DECODE_H
#define PSON_DECODE_H

#include <stdint.h>
#include <stddef.h>
#include <string.h>

/* Wire type constants (same as encoder) */
#define PSON_WIRE_UNSIGNED  0
#define PSON_WIRE_SIGNED    1
#define PSON_WIRE_FLOAT     2
#define PSON_WIRE_DISCRETE  3
#define PSON_WIRE_STRING    4
#define PSON_WIRE_BYTES     5
#define PSON_WIRE_MAP       6
#define PSON_WIRE_ARRAY     7

/* Decoded value types */
#define PSON_TYPE_UINT      0
#define PSON_TYPE_INT       1
#define PSON_TYPE_FLOAT     2
#define PSON_TYPE_DOUBLE    3
#define PSON_TYPE_BOOL      4
#define PSON_TYPE_NULL      5
#define PSON_TYPE_STRING    6
#define PSON_TYPE_BYTES     7
#define PSON_TYPE_MAP       8
#define PSON_TYPE_ARRAY     9

/* Decoded value */
typedef struct {
    int type;
    union {
        uint64_t    u;
        int64_t     i;
        float       f;
        double      d;
        int         b;       /* boolean */
        struct {
            const char *ptr;
            size_t      len;
        } str;               /* string or bytes (points into input buffer) */
        uint64_t    count;   /* map or array element count */
    } val;
} pson_value_t;

/* Decoder state */
typedef struct {
    const uint8_t *buf;
    size_t         len;
    size_t         pos;
} pson_decoder_t;

static inline void pson_decoder_init(pson_decoder_t *dec, const uint8_t *buf, size_t len) {
    dec->buf = buf;
    dec->len = len;
    dec->pos = 0;
}

static inline size_t pson_decoded_bytes(const pson_decoder_t *dec) {
    return dec->pos;
}

/* Read a single byte */
static inline int pson_read_byte(pson_decoder_t *dec, uint8_t *out) {
    if (dec->pos >= dec->len) return -1;
    *out = dec->buf[dec->pos++];
    return 0;
}

/* Read raw bytes */
static inline int pson_read_raw(pson_decoder_t *dec, void *out, size_t n) {
    if (dec->pos + n > dec->len) return -1;
    memcpy(out, dec->buf + dec->pos, n);
    dec->pos += n;
    return 0;
}

/* Read varint */
static inline int pson_read_varint(pson_decoder_t *dec, uint64_t *out) {
    *out = 0;
    uint8_t byte;
    uint8_t shift = 0;
    do {
        if (pson_read_byte(dec, &byte) != 0) return -1;
        if (shift >= 64) return -1;
        *out |= (uint64_t)(byte & 0x7F) << shift;
        shift += 7;
    } while (byte & 0x80);
    return 0;
}

/* Decode one PSON value */
static inline int pson_decode_value(pson_decoder_t *dec, pson_value_t *val) {
    uint8_t tag;
    if (pson_read_byte(dec, &tag) != 0) return -1;

    uint8_t wire_type = tag >> 5;
    uint64_t inline_val = tag & 0x1F;

    /* If overflow marker, read varint */
    if (inline_val == 0x1F) {
        if (pson_read_varint(dec, &inline_val) != 0) return -1;
    }

    switch (wire_type) {
        case PSON_WIRE_UNSIGNED:
            val->type = PSON_TYPE_UINT;
            val->val.u = inline_val;
            return 0;

        case PSON_WIRE_SIGNED:
            val->type = PSON_TYPE_INT;
            val->val.i = -(int64_t)inline_val;
            return 0;

        case PSON_WIRE_FLOAT:
            if (inline_val == 0) {
                val->type = PSON_TYPE_FLOAT;
                return pson_read_raw(dec, &val->val.f, sizeof(float));
            } else if (inline_val == 1) {
                val->type = PSON_TYPE_DOUBLE;
                return pson_read_raw(dec, &val->val.d, sizeof(double));
            }
            return -1;

        case PSON_WIRE_DISCRETE:
            if (inline_val == 0) {
                val->type = PSON_TYPE_BOOL;
                val->val.b = 0;
            } else if (inline_val == 1) {
                val->type = PSON_TYPE_BOOL;
                val->val.b = 1;
            } else if (inline_val == 2) {
                val->type = PSON_TYPE_NULL;
            } else {
                return -1;
            }
            return 0;

        case PSON_WIRE_STRING:
            val->type = PSON_TYPE_STRING;
            val->val.str.len = (size_t)inline_val;
            if (dec->pos + val->val.str.len > dec->len) return -1;
            val->val.str.ptr = (const char *)(dec->buf + dec->pos);
            dec->pos += val->val.str.len;
            return 0;

        case PSON_WIRE_BYTES:
            val->type = PSON_TYPE_BYTES;
            val->val.str.len = (size_t)inline_val;
            if (dec->pos + val->val.str.len > dec->len) return -1;
            val->val.str.ptr = (const char *)(dec->buf + dec->pos);
            dec->pos += val->val.str.len;
            return 0;

        case PSON_WIRE_MAP:
            val->type = PSON_TYPE_MAP;
            val->val.count = inline_val;
            return 0;

        case PSON_WIRE_ARRAY:
            val->type = PSON_TYPE_ARRAY;
            val->val.count = inline_val;
            return 0;

        default:
            return -1;
    }
}

#endif /* PSON_DECODE_H */
