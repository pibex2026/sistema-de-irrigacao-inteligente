#pragma once
#include "esp_err.h"
#include <cstdint>

typedef int gpio_num_t;
typedef int adc_unit_t;
typedef int adc_channel_t;

#define GPIO_NUM_2 2
#define GPIO_NUM_5 5
#define GPIO_NUM_25 25
#define GPIO_NUM_32 32
#define ADC_UNIT_1 0
#define ADC_CHANNEL_4 4

#define GPIO_MODE_OUTPUT 1
#define GPIO_PULLUP_DISABLE 0
#define GPIO_PULLDOWN_DISABLE 0
#define GPIO_INTR_DISABLE 0

struct gpio_config_t {
  uint64_t pin_bit_mask;
  int mode;
  int pull_up_en;
  int pull_down_en;
  int intr_type;
};

inline int g_mockGpioLevel[40] = {};

inline esp_err_t gpio_reset_pin(gpio_num_t) { return ESP_OK; }
inline esp_err_t gpio_config(const gpio_config_t *) { return ESP_OK; }
inline esp_err_t gpio_set_level(gpio_num_t pin, uint32_t level) {
  if (pin >= 0 && pin < 40) g_mockGpioLevel[pin] = static_cast<int>(level);
  return ESP_OK;
}
