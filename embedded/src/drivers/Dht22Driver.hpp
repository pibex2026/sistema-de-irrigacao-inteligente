#pragma once

#include "driver/gpio.h"
#include "esp_err.h"

class Dht22Driver {
public:
  explicit Dht22Driver(gpio_num_t pin);

  esp_err_t init();
  esp_err_t read(float &outTempC, float &outHumidityPct);

private:
  gpio_num_t m_pin;
  int64_t m_lastReadMs = -10000;
  float m_cachedTemp = 0.0f;
  float m_cachedHum = 0.0f;
  bool m_hasCached = false;

  esp_err_t readSingleAttempt(float &outTempC, float &outHumidityPct);
  int waitLevel(int expectedLevel, int timeoutUs) const;
};
