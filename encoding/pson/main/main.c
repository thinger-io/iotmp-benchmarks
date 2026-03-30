/*
 * PSON Encoding Benchmark — ESP-IDF
 *
 * Encodes and decodes: {"temperature": 23.5, "humidity": 60, "pressure": 1013, "label": "outdoor"}
 * Measures encode/decode time averaged over 10000 iterations.
 * Pure C, no networking, no dependencies beyond ESP-IDF base.
 */

#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "esp_timer.h"
#include "pson_encode.h"
#include "pson_decode.h"

static const char *TAG = "bench_pson";

/* ── Encode the test structure ─────────────────────────────────────── */

static size_t encode_test_data(uint8_t *buf, size_t cap) {
    pson_encoder_t enc;
    pson_encoder_init(&enc, buf, cap);

    /* Map with 4 key-value pairs */
    pson_encode_map(&enc, 4);

    /* "temperature": 23.5 */
    pson_encode_str(&enc, "temperature");
    pson_encode_float(&enc, 23.5f);

    /* "humidity": 60 */
    pson_encode_str(&enc, "humidity");
    pson_encode_uint(&enc, 60);

    /* "pressure": 1013 */
    pson_encode_str(&enc, "pressure");
    pson_encode_uint(&enc, 1013);

    /* "label": "outdoor" */
    pson_encode_str(&enc, "label");
    pson_encode_str(&enc, "outdoor");

    return pson_encoded_size(&enc);
}

/* ── Decode and print the structure ────────────────────────────────── */

static int decode_test_data(const uint8_t *buf, size_t len) {
    pson_decoder_t dec;
    pson_decoder_init(&dec, buf, len);

    pson_value_t val;

    /* Expect map */
    if (pson_decode_value(&dec, &val) != 0 || val.type != PSON_TYPE_MAP) return -1;
    uint64_t map_count = val.val.count;

    ESP_LOGI(TAG, "Decoded map with %llu entries:", (unsigned long long)map_count);

    for (uint64_t i = 0; i < map_count; i++) {
        /* Decode key (string) */
        pson_value_t key;
        if (pson_decode_value(&dec, &key) != 0 || key.type != PSON_TYPE_STRING) return -1;

        /* Decode value */
        pson_value_t v;
        if (pson_decode_value(&dec, &v) != 0) return -1;

        switch (v.type) {
            case PSON_TYPE_FLOAT:
                ESP_LOGI(TAG, "  %.*s = %.2f (float32)",
                         (int)key.val.str.len, key.val.str.ptr, v.val.f);
                break;
            case PSON_TYPE_DOUBLE:
                ESP_LOGI(TAG, "  %.*s = %.2f (float64)",
                         (int)key.val.str.len, key.val.str.ptr, v.val.d);
                break;
            case PSON_TYPE_UINT:
                ESP_LOGI(TAG, "  %.*s = %llu (uint)",
                         (int)key.val.str.len, key.val.str.ptr,
                         (unsigned long long)v.val.u);
                break;
            case PSON_TYPE_INT:
                ESP_LOGI(TAG, "  %.*s = %lld (int)",
                         (int)key.val.str.len, key.val.str.ptr,
                         (long long)v.val.i);
                break;
            case PSON_TYPE_STRING:
                ESP_LOGI(TAG, "  %.*s = \"%.*s\" (string)",
                         (int)key.val.str.len, key.val.str.ptr,
                         (int)v.val.str.len, v.val.str.ptr);
                break;
            case PSON_TYPE_BOOL:
                ESP_LOGI(TAG, "  %.*s = %s (bool)",
                         (int)key.val.str.len, key.val.str.ptr,
                         v.val.b ? "true" : "false");
                break;
            case PSON_TYPE_NULL:
                ESP_LOGI(TAG, "  %.*s = null",
                         (int)key.val.str.len, key.val.str.ptr);
                break;
            default:
                ESP_LOGI(TAG, "  %.*s = <unknown type %d>",
                         (int)key.val.str.len, key.val.str.ptr, v.type);
                break;
        }
    }

    return 0;
}

/* ── Decode silently (for benchmarking) ────────────────────────────── */

static int decode_silent(const uint8_t *buf, size_t len) {
    pson_decoder_t dec;
    pson_decoder_init(&dec, buf, len);

    pson_value_t val;
    if (pson_decode_value(&dec, &val) != 0 || val.type != PSON_TYPE_MAP) return -1;
    uint64_t count = val.val.count;

    for (uint64_t i = 0; i < count; i++) {
        pson_value_t key, v;
        if (pson_decode_value(&dec, &key) != 0) return -1;
        if (pson_decode_value(&dec, &v) != 0) return -1;
    }
    return 0;
}

/* ── Main ──────────────────────────────────────────────────────────── */

void app_main(void) {
    uint8_t buf[256];
    const int ITERATIONS = 1000000;

    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "  PSON Encoding Benchmark (ESP32)");
    ESP_LOGI(TAG, "========================================");

    /* ── Step 1: Encode ─────────────────────────────────────────── */

    size_t encoded_size = encode_test_data(buf, sizeof(buf));

    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "Encoded size: %u bytes", (unsigned)encoded_size);

    /* Print hex dump */
    char hex[512];
    size_t hex_pos = 0;
    for (size_t i = 0; i < encoded_size && hex_pos < sizeof(hex) - 4; i++) {
        hex_pos += snprintf(hex + hex_pos, sizeof(hex) - hex_pos, "%02X ", buf[i]);
    }
    ESP_LOGI(TAG, "Encoded hex: %s", hex);

    /* ── Step 2: Decode and print ───────────────────────────────── */

    ESP_LOGI(TAG, "");
    decode_test_data(buf, encoded_size);

    /* ── Step 3: Benchmark encode ───────────────────────────────── */

    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "Benchmarking %d iterations...", ITERATIONS);

    int64_t start_us = esp_timer_get_time();
    for (int i = 0; i < ITERATIONS; i++) {
        encode_test_data(buf, sizeof(buf));
    }
    int64_t encode_us = esp_timer_get_time() - start_us;

    /* ── Step 4: Benchmark decode ───────────────────────────────── */

    /* Re-encode once for clean buffer */
    encoded_size = encode_test_data(buf, sizeof(buf));

    start_us = esp_timer_get_time();
    for (int i = 0; i < ITERATIONS; i++) {
        decode_silent(buf, encoded_size);
    }
    int64_t decode_us = esp_timer_get_time() - start_us;

    /* ── Step 5: Print results ──────────────────────────────────── */

    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "  Results (PSON)");
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "  Encoded size:     %u bytes", (unsigned)encoded_size);
    ESP_LOGI(TAG, "  Encode total:     %lld us (%d iterations)", (long long)encode_us, ITERATIONS);
    ESP_LOGI(TAG, "  Encode avg:       %.2f us/iter", (double)encode_us / ITERATIONS);
    ESP_LOGI(TAG, "  Decode total:     %lld us (%d iterations)", (long long)decode_us, ITERATIONS);
    ESP_LOGI(TAG, "  Decode avg:       %.2f us/iter", (double)decode_us / ITERATIONS);
    ESP_LOGI(TAG, "========================================");

    /* ── Step 6: Implementation size ────────────────────────────── */

    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "Standalone PSON implementation:");
    ESP_LOGI(TAG, "  pson_encode.h: ~165 lines (encoder)");
    ESP_LOGI(TAG, "  pson_decode.h: ~179 lines (decoder)");
    ESP_LOGI(TAG, "  Total: ~344 lines for full encode+decode");
}
