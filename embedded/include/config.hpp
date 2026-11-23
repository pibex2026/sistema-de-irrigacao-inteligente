#pragma once

#include "driver/gpio.h"
#include "esp_adc/adc_oneshot.h"
#include <cstddef>
#include <cstdint>

namespace Config {

namespace Pins {
constexpr gpio_num_t DHT_SENSOR_PIN = GPIO_NUM_5;
constexpr gpio_num_t SOIL_MOISTURE_ADC_PIN = GPIO_NUM_32;
constexpr adc_unit_t SOIL_MOISTURE_ADC_UNIT = ADC_UNIT_1;
constexpr adc_channel_t SOIL_MOISTURE_ADC_CHANNEL = ADC_CHANNEL_4; // GPIO 32
constexpr gpio_num_t RELAY_VALVE_PIN = GPIO_NUM_25;
constexpr gpio_num_t STATUS_LED_PIN = GPIO_NUM_2;
} // namespace Pins

namespace Hardware {
constexpr bool RELAY_ACTIVE_LOW = true;
constexpr uint8_t CONSECUTIVE_IRRIGATE_CONFIRMATIONS = 2;
constexpr uint8_t MAX_CONSECUTIVE_SENSOR_FAILURES = 3;
constexpr uint32_t DHT22_MIN_INTERVAL_MS = 2000;
constexpr uint8_t SOIL_ADC_SAMPLES = 10;
constexpr uint16_t SOIL_ADC_FAULT_LOW = 10;
constexpr uint16_t SOIL_ADC_FAULT_HIGH = 4090;
} // namespace Hardware

namespace Defaults {
constexpr uint8_t SOIL_TYPE = 0;      // 0 = UNKNOWN
constexpr uint8_t CROP_ID = 0;        // 0 = UNKNOWN (Fallback Hortalicas)
constexpr uint8_t SEEDLING_STAGE = 0; // 0 = UNKNOWN
constexpr float CONFIDENCE_THRESHOLD = 0.55f;
constexpr float THERMAL_STRESS_THRESHOLD = 0.40f;
constexpr uint16_t SOIL_ADC_DRY = 3200;
constexpr uint16_t SOIL_ADC_WET = 1200;
constexpr uint32_t MAX_IRRIGATION_MS = 120000;    // 2 minutos
constexpr uint32_t COOLDOWN_MS = 300000;          // 5 minutos
constexpr uint32_t SENSOR_READ_INTERVAL_MS = 10000; // 10 segundos
constexpr uint32_t LOG_INTERVAL_MS = 60000;         // 60 segundos
constexpr uint32_t TELEMETRY_INTERVAL_MS = 15000;   // 15 segundos
} // namespace Defaults

namespace Network {
constexpr const char *DEFAULT_AP_SSID = "ESP-Irrigacao";
constexpr const char *DEFAULT_AP_PASS = "irrigacao2026"; // WPA2 (>= 8 chars)
constexpr const char *DEFAULT_LOCAL_API_PIN = "pibex2026";
constexpr const char *DEFAULT_MDNS_HOST = "irrigacao";
constexpr uint32_t PORTAL_TIMEOUT_S = 240;
constexpr const char *NTP_SERVER_PRIMARY = "a.st1.ntp.br";
constexpr const char *NTP_SERVER_SECONDARY = "pool.ntp.org";
constexpr const char *TIMEZONE_POSIX = "BRT3"; // UTC-3 (Paulista-PE)
} // namespace Network

namespace Storage {
constexpr const char *LITTLEFS_MOUNT_POINT = "/littlefs";
constexpr const char *LITTLEFS_PARTITION_LABEL = "spiffs";
constexpr const char *READINGS_CSV_PATH = "/littlefs/readings.csv";
constexpr const char *READINGS_PREV_PATH = "/littlefs/readings.prev.csv";
constexpr const char *INFERENCE_CSV_PATH = "/littlefs/inference.csv";
constexpr const char *INFERENCE_PREV_PATH = "/littlefs/inference.prev.csv";
constexpr const char *WEB_INDEX_GZ_PATH = "/littlefs/index.html.gz";
constexpr size_t MAX_READINGS_LOG_BYTES = 180 * 1024;
constexpr size_t MAX_INFERENCE_LOG_BYTES = 180 * 1024;
} // namespace Storage

} // namespace Config
