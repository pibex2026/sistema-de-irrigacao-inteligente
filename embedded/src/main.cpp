#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "model_data.h"

static const char *TAG = "MAIN";

extern "C" void app_main(void) {
  ESP_LOGI(TAG, "Sistema de Irrigacao Inteligente (ESP-IDF) iniciando...");
  ESP_LOGI(TAG, "Modelo TinyML INT8: %u bytes, %u features",
           g_model_len, TinyMLConfig::INPUT_FEATURE_COUNT);
  vTaskDelay(pdMS_TO_TICKS(1000));
}
