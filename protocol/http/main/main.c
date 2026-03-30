// HTTP Benchmark — ESP-IDF
// Periodically POSTs telemetry as JSON. Polls for commands via GET.

#include "esp_log.h"
#include "esp_random.h"
#include "esp_timer.h"
#include "esp_http_client.h"
#include "driver/gpio.h"
#include "cJSON.h"
#include "common_wifi.h"

static const char* TAG = "bench_http";

#define API_BASE "http://api.example.com/api/v1/devices/sensor1"

static float readTemperature(void) { return 22.0f + (float)(esp_random() % 100) / 10.0f; }
static float readHumidity(void)    { return 50.0f + (float)(esp_random() % 200) / 10.0f; }

static void publish_telemetry(void) {
    cJSON *root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "temperature", readTemperature());
    cJSON_AddNumberToObject(root, "humidity", readHumidity());
    char *json = cJSON_PrintUnformatted(root);

    esp_http_client_config_t config = {
        .url = API_BASE "/data",
        .method = HTTP_METHOD_POST,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_header(client, "Authorization", "Bearer secret123");
    esp_http_client_set_post_field(client, json, strlen(json));
    esp_http_client_perform(client);
    esp_http_client_cleanup(client);

    free(json);
    cJSON_Delete(root);
}

static void poll_commands(void) {
    esp_http_client_config_t config = {
        .url = API_BASE "/commands",
        .method = HTTP_METHOD_GET,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    esp_http_client_set_header(client, "Authorization", "Bearer secret123");

    esp_err_t err = esp_http_client_perform(client);
    if(err == ESP_OK && esp_http_client_get_status_code(client) == 200) {
        int len = esp_http_client_get_content_length(client);
        if(len > 0 && len < 512) {
            char buf[512];
            esp_http_client_read(client, buf, len);
            buf[len] = 0;

            cJSON *root = cJSON_Parse(buf);
            if(root) {
                cJSON *action = cJSON_GetObjectItem(root, "action");
                if(action && cJSON_IsString(action)) {
                    if(strcmp(action->valuestring, "led") == 0) {
                        cJSON *val = cJSON_GetObjectItem(root, "value");
                        if(val) gpio_set_level(GPIO_NUM_2, cJSON_IsTrue(val) ? 1 : 0);
                    }
                    else if(strcmp(action->valuestring, "read") == 0) {
                        cJSON *res = cJSON_GetObjectItem(root, "resource");
                        if(res && strcmp(res->valuestring, "environment") == 0) {
                            cJSON *rsp = cJSON_CreateObject();
                            cJSON_AddNumberToObject(rsp, "temperature", readTemperature());
                            cJSON_AddNumberToObject(rsp, "humidity", readHumidity());
                            char *rsp_json = cJSON_PrintUnformatted(rsp);

                            esp_http_client_config_t rsp_cfg = {
                                .url = API_BASE "/commands/response",
                                .method = HTTP_METHOD_POST,
                            };
                            esp_http_client_handle_t rsp_client = esp_http_client_init(&rsp_cfg);
                            esp_http_client_set_header(rsp_client, "Content-Type", "application/json");
                            esp_http_client_set_post_field(rsp_client, rsp_json, strlen(rsp_json));
                            esp_http_client_perform(rsp_client);
                            esp_http_client_cleanup(rsp_client);

                            free(rsp_json);
                            cJSON_Delete(rsp);
                        }
                    }
                    else if(strcmp(action->valuestring, "reboot") == 0) {
                        esp_restart();
                    }
                }
                cJSON_Delete(root);
            }
        }
    }
    esp_http_client_cleanup(client);
}

void app_main(void) {
    bench_wifi_init();
    ESP_LOGI(TAG, "WiFi connected");

    gpio_reset_pin(GPIO_NUM_2);
    gpio_set_direction(GPIO_NUM_2, GPIO_MODE_OUTPUT);

    while(true) {
        publish_telemetry();
        poll_commands();
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}
