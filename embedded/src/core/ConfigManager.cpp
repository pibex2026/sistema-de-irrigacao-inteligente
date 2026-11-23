#include "ConfigManager.hpp"
#include "esp_log.h"
#include "nvs.h"
#include "nvs_flash.h"
#include <algorithm>

static const char *TAG = "ConfigManager";

ConfigManager::ConfigManager() {
  m_mutex = xSemaphoreCreateMutex();
}

ConfigManager::~ConfigManager() {
  if (m_mutex != nullptr) {
    vSemaphoreDelete(m_mutex);
    m_mutex = nullptr;
  }
}

void ConfigManager::sanitize(SystemConfig &cfg) {
  cfg.schemaVersion = SCHEMA_VERSION;
  if (cfg.soilType > 7) cfg.soilType = 0;
  if (cfg.cropId > 5) cfg.cropId = 0;
  if (cfg.seedlingStage > 8) cfg.seedlingStage = 0;

  cfg.confidenceThreshold = std::clamp(cfg.confidenceThreshold, 0.30f, 0.95f);
  cfg.thermalStressThreshold = std::clamp(cfg.thermalStressThreshold, 0.25f, 0.85f);

  if (cfg.soilAdcDry > 4095) cfg.soilAdcDry = Config::Defaults::SOIL_ADC_DRY;
  if (cfg.soilAdcWet > 4095) cfg.soilAdcWet = Config::Defaults::SOIL_ADC_WET;
  if (cfg.soilAdcDry == cfg.soilAdcWet) {
    cfg.soilAdcDry = Config::Defaults::SOIL_ADC_DRY;
    cfg.soilAdcWet = Config::Defaults::SOIL_ADC_WET;
  }

  cfg.maxIrrigationMs = std::clamp<uint32_t>(cfg.maxIrrigationMs, 5000, 600000);
  cfg.cooldownMs = std::clamp<uint32_t>(cfg.cooldownMs, 10000, 3600000);
  cfg.sensorReadIntervalMs = std::clamp<uint32_t>(cfg.sensorReadIntervalMs, 2500, 300000);
  cfg.logIntervalMs = std::clamp<uint32_t>(cfg.logIntervalMs, 10000, 3600000);
  cfg.telemetryIntervalMs = std::clamp<uint32_t>(cfg.telemetryIntervalMs, 5000, 3600000);
  cfg.backendUrl[sizeof(cfg.backendUrl) - 1] = '\0';
  cfg.deviceApiKey[sizeof(cfg.deviceApiKey) - 1] = '\0';
}

esp_err_t ConfigManager::init() {
  esp_err_t err = nvs_flash_init();
  if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_LOGW(TAG, "Particao NVS invalida/cheia; apagando e reinicializando...");
    ESP_ERROR_CHECK(nvs_flash_erase());
    err = nvs_flash_init();
  }
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Falha ao inicializar NVS: %s", esp_err_to_name(err));
    return err;
  }

  nvs_handle_t handle;
  err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Falha ao abrir namespace NVS: %s", esp_err_to_name(err));
    return err;
  }

  SystemConfig loaded{};
  size_t requiredSize = sizeof(SystemConfig);
  err = nvs_get_blob(handle, NVS_KEY_BLOB, &loaded, &requiredSize);

  if (err == ESP_OK && requiredSize == sizeof(SystemConfig) &&
      loaded.schemaVersion == SCHEMA_VERSION) {
    sanitize(loaded);
    xSemaphoreTake(m_mutex, portMAX_DELAY);
    m_config = loaded;
    xSemaphoreGive(m_mutex);
    ESP_LOGI(TAG, "Configuracao carregada da NVS (Soil=%u, Crop=%u, Stage=%u).",
             loaded.soilType, loaded.cropId, loaded.seedlingStage);
  } else {
    ESP_LOGW(TAG, "Usando configuracao padrao (%s); gravando na NVS.",
             esp_err_to_name(err));
    SystemConfig defaults{};
    sanitize(defaults);
    xSemaphoreTake(m_mutex, portMAX_DELAY);
    m_config = defaults;
    xSemaphoreGive(m_mutex);
    nvs_set_blob(handle, NVS_KEY_BLOB, &defaults, sizeof(SystemConfig));
    nvs_commit(handle);
  }

  nvs_close(handle);
  return ESP_OK;
}

SystemConfig ConfigManager::get() const {
  SystemConfig copy{};
  xSemaphoreTake(m_mutex, portMAX_DELAY);
  copy = m_config;
  xSemaphoreGive(m_mutex);
  return copy;
}

esp_err_t ConfigManager::update(const SystemConfig &newConfig) {
  SystemConfig sanitized = newConfig;
  sanitize(sanitized);

  nvs_handle_t handle;
  esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
  if (err != ESP_OK) return err;

  err = nvs_set_blob(handle, NVS_KEY_BLOB, &sanitized, sizeof(SystemConfig));
  if (err == ESP_OK) {
    err = nvs_commit(handle);
  }
  nvs_close(handle);

  if (err == ESP_OK) {
    xSemaphoreTake(m_mutex, portMAX_DELAY);
    m_config = sanitized;
    xSemaphoreGive(m_mutex);
    ESP_LOGI(TAG, "Configuracao salva na NVS.");
  }
  return err;
}

esp_err_t ConfigManager::resetDefaults() {
  SystemConfig defaults{};
  return update(defaults);
}
