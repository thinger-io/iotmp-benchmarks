// IOTMP Benchmark — ESP-IDF
// Defines resources. Server controls streaming.
// 46 lines of protocol-relevant code.

#include "esp_log.h"
#include "esp_random.h"
#include "esp_timer.h"
#include "common_wifi.h"
#include <thinger/iotmp/client.hpp>

static const char* TAG = "bench_iotmp";

static float readTemperature() { return 22.0f + (float)(esp_random() % 100) / 10.0f; }
static float readHumidity()    { return 50.0f + (float)(esp_random() % 200) / 10.0f; }

extern "C" void app_main(void) {
    bench_wifi_init();
    ESP_LOGI(TAG, "WiFi connected");

    static thinger::iotmp::client thing;
    thing.set_credentials("user1", "sensor1", "secret123");
    thing.set_host("iot.thinger.io");

    // LED actuator (input resource)
    gpio_reset_pin(GPIO_NUM_2);
    gpio_set_direction(GPIO_NUM_2, GPIO_MODE_OUTPUT);
    thing["led"] << digitalPin(GPIO_NUM_2);

    // Sensor telemetry (output resource)
    thing["environment"] = [](thinger::iotmp::output& out) {
        out["temperature"] = readTemperature();
        out["humidity"] = readHumidity();
    };

    // Uptime (output resource)
    thing["uptime"] = [](thinger::iotmp::output& out) {
        out["ms"] = (uint32_t)(esp_timer_get_time() / 1000);
    };

    // Reboot (run resource)
    thing["reboot"] = []() {
        esp_restart();
    };

    thing.start();

    while(true) { vTaskDelay(pdMS_TO_TICKS(1000)); }
}
