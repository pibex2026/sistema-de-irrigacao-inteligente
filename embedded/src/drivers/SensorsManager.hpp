#pragma once

#include "Dht22Driver.hpp"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "types.hpp"

class SensorsManager {
public:
  SensorsManager();
  ~SensorsManager();

  esp_err_t init();
  SensorData readSensors(const SystemConfig &cfg);
  uint16_t readSoilAdcMedian();

private:
  Dht22Driver m_dht;
  adc_oneshot_unit_handle_t m_adcHandle = nullptr;
  adc_cali_handle_t m_adcCaliHandle = nullptr;
  SemaphoreHandle_t m_adcMutex = nullptr;
  bool m_hasCali = false;
  bool m_initialized = false;
};
