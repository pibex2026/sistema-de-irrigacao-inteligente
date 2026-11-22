#include "SensorsManager.hpp"
#include "TimeUtils.hpp"
#include "config.hpp"
#include "esp_adc/adc_cali_scheme.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <algorithm>
#include <array>
#include <cmath>

static const char *TAG = "SensorsManager";

SensorsManager::SensorsManager() : m_dht(Config::Pins::DHT_SENSOR_PIN) {}

SensorsManager::~SensorsManager() {
  if (m_hasCali && m_adcCaliHandle != nullptr) {
    adc_cali_delete_scheme_line_fitting(m_adcCaliHandle);
    m_adcCaliHandle = nullptr;
  }
  if (m_adcHandle != nullptr) {
    adc_oneshot_del_unit(m_adcHandle);
    m_adcHandle = nullptr;
  }
}

esp_err_t SensorsManager::init() {
  esp_err_t err = m_dht.init();
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Erro ao inicializar DHT22: %s", esp_err_to_name(err));
    return err;
  }

  adc_oneshot_unit_init_cfg_t unitCfg{};
  unitCfg.unit_id = Config::Pins::SOIL_MOISTURE_ADC_UNIT;
  unitCfg.ulp_mode = ADC_ULP_MODE_DISABLE;
  err = adc_oneshot_new_unit(&unitCfg, &m_adcHandle);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Erro ao criar ADC1 unit: %s", esp_err_to_name(err));
    return err;
  }

  adc_oneshot_chan_cfg_t chanCfg{};
  chanCfg.atten = ADC_ATTEN_DB_12;
  chanCfg.bitwidth = ADC_BITWIDTH_12;
  err = adc_oneshot_config_channel(m_adcHandle, Config::Pins::SOIL_MOISTURE_ADC_CHANNEL, &chanCfg);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Erro ao configurar canal ADC: %s", esp_err_to_name(err));
    return err;
  }

  adc_cali_line_fitting_config_t caliCfg{};
  caliCfg.unit_id = Config::Pins::SOIL_MOISTURE_ADC_UNIT;
  caliCfg.atten = ADC_ATTEN_DB_12;
  caliCfg.bitwidth = ADC_BITWIDTH_12;
  if (adc_cali_create_scheme_line_fitting(&caliCfg, &m_adcCaliHandle) == ESP_OK) {
    m_hasCali = true;
  }

  m_initialized = true;
  ESP_LOGI(TAG, "SensorsManager pronto (DHT22 GPIO %d, HW-080 ADC1_CH4 GPIO %d, Cali=%s).",
           Config::Pins::DHT_SENSOR_PIN, Config::Pins::SOIL_MOISTURE_ADC_PIN,
           m_hasCali ? "ON" : "OFF");
  return ESP_OK;
}

uint16_t SensorsManager::readSoilAdcMedian() {
  if (!m_initialized || m_adcHandle == nullptr) return 0;

  std::array<int, Config::Hardware::SOIL_ADC_SAMPLES> samples{};
  for (size_t i = 0; i < samples.size(); ++i) {
    int raw = 0;
    if (adc_oneshot_read(m_adcHandle, Config::Pins::SOIL_MOISTURE_ADC_CHANNEL, &raw) != ESP_OK) {
      raw = 0;
    }
    samples[i] = std::clamp(raw, 0, 4095);
    vTaskDelay(pdMS_TO_TICKS(2));
  }

  std::sort(samples.begin(), samples.end());
  return static_cast<uint16_t>(samples[samples.size() / 2]);
}

SensorData SensorsManager::readSensors(const SystemConfig &cfg) {
  SensorData data{};
  TimeUtils::getFormattedTimestamp(data.timestamp, sizeof(data.timestamp));

  float tempC = 0.0f;
  float humPct = 0.0f;
  const esp_err_t dhtErr = m_dht.read(tempC, humPct);
  if (dhtErr == ESP_OK) {
    data.temperatureX10 = static_cast<int16_t>(std::lround(tempC * 10.0f));
    data.humidity = static_cast<uint8_t>(std::clamp<int>(static_cast<int>(std::lround(humPct)), 0, 100));
  } else {
    data.temperatureX10 = TinyMLConfig::SENTINEL_TEMP_ERROR_I16;
    data.humidity = TinyMLConfig::SENTINEL_HUM_ERROR_U8;
  }

  data.rawSoilAdc = readSoilAdcMedian();
  const bool adcFault = (data.rawSoilAdc <= Config::Hardware::SOIL_ADC_FAULT_LOW ||
                         data.rawSoilAdc >= Config::Hardware::SOIL_ADC_FAULT_HIGH);

  if (adcFault) {
    data.soilMoisture = TinyMLConfig::SENTINEL_HUM_ERROR_U8;
    ESP_LOGW(TAG, "Falha eletrica detectada no sensor HW-080 (ADC=%u).", data.rawSoilAdc);
  } else {
    const float dry = static_cast<float>(cfg.soilAdcDry);
    const float wet = static_cast<float>(cfg.soilAdcWet);
    const float raw = static_cast<float>(data.rawSoilAdc);
    // Mapeamento linear calibrado: dry -> 0%, wet -> 100%
    const float pct = ((dry - raw) / (dry - wet)) * 100.0f;
    data.soilMoisture = static_cast<uint8_t>(std::clamp<int>(static_cast<int>(std::lround(pct)), 0, 100));
  }

  data.valid = (dhtErr == ESP_OK) && !adcFault &&
               TinyMLConfig::isValidSensorReading(
                   data.getTemperatureC(),
                   static_cast<float>(data.humidity),
                   static_cast<float>(data.soilMoisture));

  return data;
}
