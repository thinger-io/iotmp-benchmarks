// CoAP Benchmark — ESP-IDF
// Acts as CoAP server for device resources + periodically POSTs telemetry.
// Uses ESP-IDF's libcoap component.

#include "esp_log.h"
#include "esp_random.h"
#include "esp_timer.h"
#include "driver/gpio.h"
#include "coap3/coap.h"
#include "cJSON.h"
#include "common_wifi.h"

static const char* TAG = "bench_coap";

static float readTemperature(void) { return 22.0f + (float)(esp_random() % 100) / 10.0f; }
static float readHumidity(void)    { return 50.0f + (float)(esp_random() % 200) / 10.0f; }

// CoAP server resource handler: /environment
static void hnd_environment_get(coap_resource_t *resource,
                                 coap_session_t *session,
                                 const coap_pdu_t *request,
                                 const coap_string_t *query,
                                 coap_pdu_t *response) {
    cJSON *root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "temperature", readTemperature());
    cJSON_AddNumberToObject(root, "humidity", readHumidity());
    char *json = cJSON_PrintUnformatted(root);

    coap_pdu_set_code(response, COAP_RESPONSE_CODE_CONTENT);
    coap_add_data(response, strlen(json), (const uint8_t *)json);

    free(json);
    cJSON_Delete(root);
}

// CoAP server resource handler: /led
static void hnd_led_put(coap_resource_t *resource,
                          coap_session_t *session,
                          const coap_pdu_t *request,
                          const coap_string_t *query,
                          coap_pdu_t *response) {
    size_t len;
    const uint8_t *data;
    if(coap_get_data(request, &len, &data)) {
        cJSON *root = cJSON_ParseWithLength((const char*)data, len);
        if(root) {
            cJSON *val = cJSON_GetObjectItem(root, "value");
            if(val) gpio_set_level(GPIO_NUM_2, cJSON_IsTrue(val) ? 1 : 0);
            cJSON_Delete(root);
        }
    }
    coap_pdu_set_code(response, COAP_RESPONSE_CODE_CHANGED);
}

// CoAP server resource handler: /uptime
static void hnd_uptime_get(coap_resource_t *resource,
                            coap_session_t *session,
                            const coap_pdu_t *request,
                            const coap_string_t *query,
                            coap_pdu_t *response) {
    char buf[32];
    snprintf(buf, sizeof(buf), "{\"ms\":%lld}", esp_timer_get_time() / 1000);
    coap_pdu_set_code(response, COAP_RESPONSE_CODE_CONTENT);
    coap_add_data(response, strlen(buf), (const uint8_t *)buf);
}

// CoAP server resource handler: /reboot
static void hnd_reboot_post(coap_resource_t *resource,
                             coap_session_t *session,
                             const coap_pdu_t *request,
                             const coap_string_t *query,
                             coap_pdu_t *response) {
    coap_pdu_set_code(response, COAP_RESPONSE_CODE_CHANGED);
    // In production: esp_restart();
}

static void coap_server_task(void *param) {
    coap_context_t *ctx = coap_new_context(NULL);
    if(!ctx) { ESP_LOGE(TAG, "coap_new_context failed"); vTaskDelete(NULL); return; }

    coap_address_t addr;
    coap_address_init(&addr);
    addr.addr.sin.sin_family = AF_INET;
    addr.addr.sin.sin_port = htons(5683);
    addr.addr.sin.sin_addr.s_addr = INADDR_ANY;

    coap_endpoint_t *ep = coap_new_endpoint(ctx, &addr, COAP_PROTO_UDP);
    if(!ep) { ESP_LOGE(TAG, "coap_new_endpoint failed"); coap_free_context(ctx); vTaskDelete(NULL); return; }

    // Register resources
    coap_resource_t *r_env = coap_resource_init(coap_make_str_const("environment"), 0);
    coap_register_handler(r_env, COAP_REQUEST_GET, hnd_environment_get);
    coap_add_resource(ctx, r_env);

    coap_resource_t *r_led = coap_resource_init(coap_make_str_const("led"), 0);
    coap_register_handler(r_led, COAP_REQUEST_PUT, hnd_led_put);
    coap_add_resource(ctx, r_led);

    coap_resource_t *r_up = coap_resource_init(coap_make_str_const("uptime"), 0);
    coap_register_handler(r_up, COAP_REQUEST_GET, hnd_uptime_get);
    coap_add_resource(ctx, r_up);

    coap_resource_t *r_reboot = coap_resource_init(coap_make_str_const("reboot"), 0);
    coap_register_handler(r_reboot, COAP_REQUEST_POST, hnd_reboot_post);
    coap_add_resource(ctx, r_reboot);

    while(true) {
        coap_io_process(ctx, 1000);
    }
}

void app_main(void) {
    bench_wifi_init();
    ESP_LOGI(TAG, "WiFi connected");

    gpio_reset_pin(GPIO_NUM_2);
    gpio_set_direction(GPIO_NUM_2, GPIO_MODE_OUTPUT);

    // Start CoAP server in a task
    xTaskCreate(coap_server_task, "coap_server", 8192, NULL, 5, NULL);

    // Periodic telemetry via CoAP POST (simplified — no actual client POST here
    // as libcoap server mode is the primary use case for CoAP devices)
    while(true) {
        vTaskDelay(pdMS_TO_TICKS(5000));
        ESP_LOGI(TAG, "Telemetry: temp=%.1f hum=%.1f", readTemperature(), readHumidity());
    }
}
