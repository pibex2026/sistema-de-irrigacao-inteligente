#pragma once

#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "types.hpp"

class ConfigManager {
public:
  ConfigManager();
  ~ConfigManager();

  esp_err_t init();
  SystemConfig get() const;
  esp_err_t update(const SystemConfig &newConfig);
  esp_err_t resetDefaults();

  static void sanitize(SystemConfig &cfg);

private:
  static constexpr const char *NVS_NAMESPACE = "irrig_cfg";
  static constexpr const char *NVS_KEY_BLOB = "sys_cfg";
  static constexpr uint32_t SCHEMA_VERSION = 1;

  SystemConfig m_config;
  mutable SemaphoreHandle_t m_mutex = nullptr;
};
