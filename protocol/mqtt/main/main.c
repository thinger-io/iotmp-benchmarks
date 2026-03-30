// MQTT Benchmark — ESP-IDF
// Publishes telemetry as JSON. Subscribes to command topic. Manual routing.

#include "esp_log.h"
#include "esp_random.h"
#include "mqtt_client.h"
#include "cJSON.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include "common_wifi.h"

static const char* TAG = "bench_mqtt";

#define MQTT_BROKER  "mqtt://broker.example.com"
#define MQTT_TOPIC   "devices/sensor1/telemetry"
#define MQTT_CMD     "devices/sensor1/cmd"
#define MQTT_RSP     "devices/sensor1/rsp"

static esp_mqtt_client_handle_t mqtt_client;
static bool mqtt_connected = false;

static float readTemperature(void) { return 22.0f + (float)(esp_random() % 100) / 10.0f; }
static float readHumidity(void)    { return 50.0f + (float)(esp_random() % 200) / 10.0f; }

static void publish_telemetry(void) {
    cJSON *root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "temperature", readTemperature());
    cJSON_AddNumberToObject(root, "humidity", readHumidity());
    char *json = cJSON_PrintUnformatted(root);
    esp_mqtt_client_publish(mqtt_client, MQTT_TOPIC, json, 0, 0, 0);
    free(json);
    cJSON_Delete(root);
}

static void handle_command(const char *data, int len) {
    cJSON *root = cJSON_ParseWithLength(data, len);
    if(!root) return;

    cJSON *action = cJSON_GetObjectItem(root, "action");
    if(!action || !cJSON_IsString(action)) { cJSON_Delete(root); return; }

    if(strcmp(action->valuestring, "led") == 0) {
        cJSON *val = cJSON_GetObjectItem(root, "value");
        if(val) gpio_set_level(GPIO_NUM_2, cJSON_IsTrue(val) ? 1 : 0);
        esp_mqtt_client_publish(mqtt_client, MQTT_RSP,
            "{\"resource\":\"led\",\"status\":\"ok\"}", 0, 0, 0);
    }
    else if(strcmp(action->valuestring, "read") == 0) {
        cJSON *res = cJSON_GetObjectItem(root, "resource");
        if(res && strcmp(res->valuestring, "environment") == 0) {
            cJSON *rsp = cJSON_CreateObject();
            cJSON_AddNumberToObject(rsp, "temperature", readTemperature());
            cJSON_AddNumberToObject(rsp, "humidity", readHumidity());
            char *json = cJSON_PrintUnformatted(rsp);
            esp_mqtt_client_publish(mqtt_client, MQTT_RSP, json, 0, 0, 0);
            free(json);
            cJSON_Delete(rsp);
        }
        else if(res && strcmp(res->valuestring, "uptime") == 0) {
            cJSON *rsp = cJSON_CreateObject();
            cJSON_AddNumberToObject(rsp, "ms", (double)(esp_timer_get_time() / 1000));
            char *json = cJSON_PrintUnformatted(rsp);
            esp_mqtt_client_publish(mqtt_client, MQTT_RSP, json, 0, 0, 0);
            free(json);
            cJSON_Delete(rsp);
        }
    }
    else if(strcmp(action->valuestring, "reboot") == 0) {
        esp_mqtt_client_publish(mqtt_client, MQTT_RSP,
            "{\"action\":\"reboot\",\"status\":\"ok\"}", 0, 0, 0);
        vTaskDelay(pdMS_TO_TICKS(100));
        esp_restart();
    }

    cJSON_Delete(root);
}

static void mqtt_event_handler(void *args, esp_event_base_t base,
                                int32_t event_id, void *data) {
    esp_mqtt_event_handle_t event = data;
    switch(event_id) {
        case MQTT_EVENT_CONNECTED:
            mqtt_connected = true;
            esp_mqtt_client_subscribe(mqtt_client, MQTT_CMD, 0);
            ESP_LOGI(TAG, "MQTT connected");
            break;
        case MQTT_EVENT_DISCONNECTED:
            mqtt_connected = false;
            break;
        case MQTT_EVENT_DATA:
            handle_command(event->data, event->data_len);
            break;
        default:
            break;
    }
}

void app_main(void) {
    bench_wifi_init();
    ESP_LOGI(TAG, "WiFi connected");

    gpio_reset_pin(GPIO_NUM_2);
    gpio_set_direction(GPIO_NUM_2, GPIO_MODE_OUTPUT);

    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = MQTT_BROKER,
        .credentials.username = "user1",
        .credentials.authentication.password = "secret123",
        .credentials.client_id = "sensor1",
    };
    mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(mqtt_client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(mqtt_client);

    // Periodic telemetry
    while(true) {
        vTaskDelay(pdMS_TO_TICKS(5000));
        if(mqtt_connected) {
            publish_telemetry();
        }
    }
}
