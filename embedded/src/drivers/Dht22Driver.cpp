#include "Dht22Driver.hpp"
#include "TimeUtils.hpp"
#include "config.hpp"
#include "esp_log.h"
#include "esp_rom_sys.h"
#include "freertos/FreeRTOS.h"
#include "freertos/portmacro.h"
#include "freertos/task.h"

static const char *TAG = "Dht22Driver";
static portMUX_TYPE s_dhtMux = portMUX_INITIALIZER_UNLOCKED;

Dht22Driver::Dht22Driver(gpio_num_t pin) : m_pin(pin) {}

esp_err_t Dht22Driver::init() {
  gpio_reset_pin(m_pin);
  gpio_config_t cfg{};
  cfg.pin_bit_mask = (1ULL << m_pin);
  cfg.mode = GPIO_MODE_INPUT_OUTPUT_OD;
  cfg.pull_up_en = GPIO_PULLUP_ENABLE;
  cfg.pull_down_en = GPIO_PULLDOWN_DISABLE;
  cfg.intr_type = GPIO_INTR_DISABLE;
  esp_err_t err = gpio_config(&cfg);
  if (err == ESP_OK) {
    gpio_set_level(m_pin, 1);
  }
  return err;
}

int Dht22Driver::waitLevel(int expectedLevel, int timeoutUs) const {
  int elapsed = 0;
  while (gpio_get_level(m_pin) != expectedLevel) {
    if (elapsed >= timeoutUs) {
      return -1;
    }
    esp_rom_delay_us(1);
    elapsed++;
  }
  return elapsed;
}

esp_err_t Dht22Driver::readSingleAttempt(float &outTempC, float &outHumidityPct) {
  uint8_t data[5] = {0, 0, 0, 0, 0};

  // Sinal de start do host: nivel baixo >= 1.1 ms, depois solta para pull-up
  gpio_set_direction(m_pin, GPIO_MODE_OUTPUT);
  gpio_set_level(m_pin, 0);
  esp_rom_delay_us(1200);

  portENTER_CRITICAL(&s_dhtMux);
  gpio_set_level(m_pin, 1);
  esp_rom_delay_us(30);
  gpio_set_direction(m_pin, GPIO_MODE_INPUT);

  // Resposta do DHT22: ~80 us LOW + ~80 us HIGH
  if (waitLevel(0, 100) < 0 || waitLevel(1, 100) < 0 || waitLevel(0, 100) < 0) {
    portEXIT_CRITICAL(&s_dhtMux);
    return ESP_ERR_TIMEOUT;
  }

  for (int i = 0; i < 40; ++i) {
    if (waitLevel(1, 80) < 0) {
      portEXIT_CRITICAL(&s_dhtMux);
      return ESP_ERR_TIMEOUT;
    }
    const int highUs = waitLevel(0, 100);
    if (highUs < 0) {
      portEXIT_CRITICAL(&s_dhtMux);
      return ESP_ERR_TIMEOUT;
    }
    data[i / 8] <<= 1;
    if (highUs > 40) {
      data[i / 8] |= 1;
    }
  }
  portEXIT_CRITICAL(&s_dhtMux);

  const uint8_t checksum = static_cast<uint8_t>((data[0] + data[1] + data[2] + data[3]) & 0xFF);
  if (checksum != data[4]) {
    return ESP_ERR_INVALID_CRC;
  }

  const uint16_t rawHum = (static_cast<uint16_t>(data[0]) << 8) | data[1];
  const uint16_t rawTemp = (static_cast<uint16_t>(data[2] & 0x7F) << 8) | data[3];

  outHumidityPct = static_cast<float>(rawHum) * 0.1f;
  outTempC = static_cast<float>(rawTemp) * 0.1f;
  if (data[2] & 0x80) {
    outTempC = -outTempC;
  }

  if (outHumidityPct < 0.0f || outHumidityPct > 100.0f || outTempC < -40.0f || outTempC > 80.0f) {
    return ESP_ERR_INVALID_RESPONSE;
  }
  return ESP_OK;
}

esp_err_t Dht22Driver::read(float &outTempC, float &outHumidityPct) {
  const int64_t nowMs = TimeUtils::getMillis();
  if (m_hasCached && (nowMs - m_lastReadMs) < static_cast<int64_t>(Config::Hardware::DHT22_MIN_INTERVAL_MS)) {
    outTempC = m_cachedTemp;
    outHumidityPct = m_cachedHum;
    return ESP_OK;
  }

  esp_err_t err = ESP_FAIL;
  for (int attempt = 0; attempt < 2; ++attempt) {
    err = readSingleAttempt(outTempC, outHumidityPct);
    if (err == ESP_OK) {
      m_lastReadMs = TimeUtils::getMillis();
      m_cachedTemp = outTempC;
      m_cachedHum = outHumidityPct;
      m_hasCached = true;
      return ESP_OK;
    }
    vTaskDelay(pdMS_TO_TICKS(50));
  }

  ESP_LOGW(TAG, "Falha leitura DHT22: %s", esp_err_to_name(err));
  return err;
}
