/*
 * CBOR (NanoCBOR) Encoding Benchmark — ESP-IDF
 *
 * Encodes and decodes: {"temperature": 23.5, "humidity": 60, "pressure": 1013, "label": "outdoor"}
 * Measures encode/decode time averaged over 10000 iterations.
 * Pure C, no networking, no dependencies beyond ESP-IDF base + NanoCBOR.
 */

#include <stdio.h>
#include <string.h>
#include <math.h>
#include "esp_log.h"
#include "esp_timer.h"
#include "nanocbor/nanocbor.h"

static const char *TAG = "bench_cbor";

/* ── Encode the test structure ─────────────────────────────────────── */

static size_t encode_test_data(uint8_t *buf, size_t cap) {
    nanocbor_encoder_t enc;
    nanocbor_encoder_init(&enc, buf, cap);

    /* Map with 4 key-value pairs */
    nanocbor_fmt_map(&enc, 4);

    /* "temperature": 23.5 */
    nanocbor_put_tstr(&enc, "temperature");
    nanocbor_fmt_float(&enc, 23.5f);

    /* "humidity": 60 */
    nanocbor_put_tstr(&enc, "humidity");
    nanocbor_fmt_uint(&enc, 60);

    /* "pressure": 1013 */
    nanocbor_put_tstr(&enc, "pressure");
    nanocbor_fmt_uint(&enc, 1013);

    /* "label": "outdoor" */
    nanocbor_put_tstr(&enc, "label");
    nanocbor_put_tstr(&enc, "outdoor");

    return nanocbor_encoded_len(&enc);
}

/* ── Decode and print the structure ────────────────────────────────── */

static int decode_test_data(const uint8_t *buf, size_t len) {
    nanocbor_value_t dec;
    nanocbor_value_t map;

    nanocbor_decoder_init(&dec, buf, len);

    /* Enter the map */
    if (nanocbor_enter_map(&dec, &map) < 0) {
        ESP_LOGE(TAG, "Failed to enter map");
        return -1;
    }

    ESP_LOGI(TAG, "Decoded CBOR map:");

    while (!nanocbor_at_end(&map)) {
        /* Decode key (text string) */
        const uint8_t *key;
        size_t key_len;
        if (nanocbor_get_tstr(&map, &key, &key_len) < 0) {
            ESP_LOGE(TAG, "Failed to decode key");
            return -1;
        }

        /* Peek at the next value type */
        uint8_t type = nanocbor_get_type(&map);

        if (type == NANOCBOR_TYPE_FLOAT) {
            /* Could be float or double */
            float fval;
            if (nanocbor_get_float(&map, &fval) >= 0) {
                ESP_LOGI(TAG, "  %.*s = %.2f (float)", (int)key_len, (const char *)key, fval);
            } else {
                double dval;
                if (nanocbor_get_double(&map, &dval) >= 0) {
                    ESP_LOGI(TAG, "  %.*s = %.2f (double)", (int)key_len, (const char *)key, dval);
                } else {
                    nanocbor_skip(&map);
                }
            }
        } else if (type == NANOCBOR_TYPE_UINT) {
            uint64_t uval;
            if (nanocbor_get_uint64(&map, &uval) >= 0) {
                ESP_LOGI(TAG, "  %.*s = %llu (uint)", (int)key_len, (const char *)key,
                         (unsigned long long)uval);
            }
        } else if (type == NANOCBOR_TYPE_NINT) {
            int64_t ival;
            if (nanocbor_get_int64(&map, &ival) >= 0) {
                ESP_LOGI(TAG, "  %.*s = %lld (int)", (int)key_len, (const char *)key,
                         (long long)ival);
            }
        } else if (type == NANOCBOR_TYPE_TSTR) {
            const uint8_t *sval;
            size_t slen;
            if (nanocbor_get_tstr(&map, &sval, &slen) >= 0) {
                ESP_LOGI(TAG, "  %.*s = \"%.*s\" (string)", (int)key_len, (const char *)key,
                         (int)slen, (const char *)sval);
            }
        } else {
            ESP_LOGI(TAG, "  %.*s = <type %d>", (int)key_len, (const char *)key, type);
            nanocbor_skip(&map);
        }
    }

    nanocbor_leave_container(&dec, &map);
    return 0;
}

/* ── Decode silently (for benchmarking) ────────────────────────────── */

static int decode_silent(const uint8_t *buf, size_t len) {
    nanocbor_value_t dec;
    nanocbor_value_t map;

    nanocbor_decoder_init(&dec, buf, len);

    if (nanocbor_enter_map(&dec, &map) < 0) return -1;

    while (!nanocbor_at_end(&map)) {
        const uint8_t *key;
        size_t key_len;
        if (nanocbor_get_tstr(&map, &key, &key_len) < 0) return -1;

        uint8_t type = nanocbor_get_type(&map);
        if (type == NANOCBOR_TYPE_FLOAT) {
            float fval;
            if (nanocbor_get_float(&map, &fval) < 0) {
                double dval;
                if (nanocbor_get_double(&map, &dval) < 0) nanocbor_skip(&map);
            }
        } else if (type == NANOCBOR_TYPE_UINT) {
            uint64_t uval;
            nanocbor_get_uint64(&map, &uval);
        } else if (type == NANOCBOR_TYPE_NINT) {
            int64_t ival;
            nanocbor_get_int64(&map, &ival);
        } else if (type == NANOCBOR_TYPE_TSTR) {
            const uint8_t *sval;
            size_t slen;
            nanocbor_get_tstr(&map, &sval, &slen);
        } else {
            nanocbor_skip(&map);
        }
    }

    nanocbor_leave_container(&dec, &map);
    return 0;
}

/* ── Main ──────────────────────────────────────────────────────────── */

void app_main(void) {
    uint8_t buf[256];
    const int ITERATIONS = 1000000;

    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "  CBOR (NanoCBOR) Encoding Benchmark (ESP32)");
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
    ESP_LOGI(TAG, "  Results (NanoCBOR)");
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "  Encoded size:     %u bytes", (unsigned)encoded_size);
    ESP_LOGI(TAG, "  Encode total:     %lld us (%d iterations)", (long long)encode_us, ITERATIONS);
    ESP_LOGI(TAG, "  Encode avg:       %.2f us/iter", (double)encode_us / ITERATIONS);
    ESP_LOGI(TAG, "  Decode total:     %lld us (%d iterations)", (long long)decode_us, ITERATIONS);
    ESP_LOGI(TAG, "  Decode avg:       %.2f us/iter", (double)decode_us / ITERATIONS);
    ESP_LOGI(TAG, "========================================");
}
