/*
 * CBOR (TinyCBOR) Encoding Benchmark -- ESP-IDF
 *
 * Encodes and decodes: {"temperature": 23.5, "humidity": 60, "pressure": 1013, "label": "outdoor"}
 * Measures encode/decode time averaged over 10000 iterations.
 * Pure C, no networking, no dependencies beyond ESP-IDF base + TinyCBOR.
 */

#include <stdio.h>
#include <string.h>
#include <math.h>
#include "esp_log.h"
#include "esp_timer.h"
#include "cbor.h"

static const char *TAG = "bench_tinycbor";

/* -- Encode the test structure ------------------------------------------- */

static size_t encode_test_data(uint8_t *buf, size_t cap) {
    CborEncoder encoder, mapEncoder;

    cbor_encoder_init(&encoder, buf, cap, 0);
    cbor_encoder_create_map(&encoder, &mapEncoder, 4);

    /* "temperature": 23.5 */
    cbor_encode_text_stringz(&mapEncoder, "temperature");
    cbor_encode_float(&mapEncoder, 23.5f);

    /* "humidity": 60 */
    cbor_encode_text_stringz(&mapEncoder, "humidity");
    cbor_encode_uint(&mapEncoder, 60);

    /* "pressure": 1013 */
    cbor_encode_text_stringz(&mapEncoder, "pressure");
    cbor_encode_uint(&mapEncoder, 1013);

    /* "label": "outdoor" */
    cbor_encode_text_stringz(&mapEncoder, "label");
    cbor_encode_text_stringz(&mapEncoder, "outdoor");

    cbor_encoder_close_container(&encoder, &mapEncoder);

    return cbor_encoder_get_buffer_size(&encoder, buf);
}

/* -- Decode and print the structure -------------------------------------- */

static int decode_test_data(const uint8_t *buf, size_t len) {
    CborParser parser;
    CborValue it, map;

    CborError err = cbor_parser_init(buf, len, 0, &parser, &it);
    if (err != CborNoError) {
        ESP_LOGE(TAG, "Failed to init parser: %s", cbor_error_string(err));
        return -1;
    }

    if (!cbor_value_is_map(&it)) {
        ESP_LOGE(TAG, "Expected a map at top level");
        return -1;
    }

    err = cbor_value_enter_container(&it, &map);
    if (err != CborNoError) {
        ESP_LOGE(TAG, "Failed to enter map: %s", cbor_error_string(err));
        return -1;
    }

    ESP_LOGI(TAG, "Decoded CBOR map:");

    while (!cbor_value_at_end(&map)) {
        /* Decode key (text string) */
        if (!cbor_value_is_text_string(&map)) {
            ESP_LOGE(TAG, "Expected text string key");
            return -1;
        }

        /* Get key - TinyCBOR needs us to compare in place or copy */
        size_t key_len = 0;
        cbor_value_get_string_length(&map, &key_len);
        char key[64];
        size_t key_buf_len = sizeof(key);
        cbor_value_copy_text_string(&map, key, &key_buf_len, &map);

        /* Decode value based on type */
        CborType type = cbor_value_get_type(&map);

        if (type == CborFloatType) {
            float fval;
            cbor_value_get_float(&map, &fval);
            ESP_LOGI(TAG, "  %s = %.2f (float)", key, fval);
            cbor_value_advance_fixed(&map);
        } else if (type == CborDoubleType) {
            double dval;
            cbor_value_get_double(&map, &dval);
            ESP_LOGI(TAG, "  %s = %.2f (double)", key, dval);
            cbor_value_advance_fixed(&map);
        } else if (type == CborIntegerType) {
            uint64_t uval;
            cbor_value_get_uint64(&map, &uval);
            ESP_LOGI(TAG, "  %s = %llu (uint)", key, (unsigned long long)uval);
            cbor_value_advance_fixed(&map);
        } else if (type == CborTextStringType) {
            char sval[128];
            size_t slen = sizeof(sval);
            cbor_value_copy_text_string(&map, sval, &slen, &map);
            ESP_LOGI(TAG, "  %s = \"%s\" (string)", key, sval);
        } else {
            ESP_LOGI(TAG, "  %s = <type %d>", key, type);
            cbor_value_advance(&map);
        }
    }

    cbor_value_leave_container(&it, &map);
    return 0;
}

/* -- Decode silently (for benchmarking) ---------------------------------- */

static int decode_silent(const uint8_t *buf, size_t len) {
    CborParser parser;
    CborValue it, map;

    if (cbor_parser_init(buf, len, 0, &parser, &it) != CborNoError) return -1;
    if (cbor_value_enter_container(&it, &map) != CborNoError) return -1;

    while (!cbor_value_at_end(&map)) {
        /* Skip key */
        char key[64];
        size_t key_len = sizeof(key);
        cbor_value_copy_text_string(&map, key, &key_len, &map);

        /* Decode value based on type */
        CborType type = cbor_value_get_type(&map);

        if (type == CborFloatType) {
            float fval;
            cbor_value_get_float(&map, &fval);
            cbor_value_advance_fixed(&map);
        } else if (type == CborDoubleType) {
            double dval;
            cbor_value_get_double(&map, &dval);
            cbor_value_advance_fixed(&map);
        } else if (type == CborIntegerType) {
            uint64_t uval;
            cbor_value_get_uint64(&map, &uval);
            cbor_value_advance_fixed(&map);
        } else if (type == CborTextStringType) {
            char sval[128];
            size_t slen = sizeof(sval);
            cbor_value_copy_text_string(&map, sval, &slen, &map);
        } else {
            cbor_value_advance(&map);
        }
    }

    cbor_value_leave_container(&it, &map);
    return 0;
}

/* -- Main ---------------------------------------------------------------- */

void app_main(void) {
    uint8_t buf[256];
    const int ITERATIONS = 1000000;

    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "  CBOR (TinyCBOR) Encoding Benchmark (ESP32)");
    ESP_LOGI(TAG, "========================================");

    /* -- Step 1: Encode -------------------------------------------------- */

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

    /* -- Step 2: Decode and print ---------------------------------------- */

    ESP_LOGI(TAG, "");
    decode_test_data(buf, encoded_size);

    /* -- Step 3: Benchmark encode ---------------------------------------- */

    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "Benchmarking %d iterations...", ITERATIONS);

    int64_t start_us = esp_timer_get_time();
    for (int i = 0; i < ITERATIONS; i++) {
        encode_test_data(buf, sizeof(buf));
    }
    int64_t encode_us = esp_timer_get_time() - start_us;

    /* -- Step 4: Benchmark decode ---------------------------------------- */

    /* Re-encode once for clean buffer */
    encoded_size = encode_test_data(buf, sizeof(buf));

    start_us = esp_timer_get_time();
    for (int i = 0; i < ITERATIONS; i++) {
        decode_silent(buf, encoded_size);
    }
    int64_t decode_us = esp_timer_get_time() - start_us;

    /* -- Step 5: Print results ------------------------------------------- */

    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "  Results (TinyCBOR)");
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "  Encoded size:     %u bytes", (unsigned)encoded_size);
    ESP_LOGI(TAG, "  Encode total:     %lld us (%d iterations)", (long long)encode_us, ITERATIONS);
    ESP_LOGI(TAG, "  Encode avg:       %.2f us/iter", (double)encode_us / ITERATIONS);
    ESP_LOGI(TAG, "  Decode total:     %lld us (%d iterations)", (long long)decode_us, ITERATIONS);
    ESP_LOGI(TAG, "  Decode avg:       %.2f us/iter", (double)decode_us / ITERATIONS);
    ESP_LOGI(TAG, "========================================");
}
