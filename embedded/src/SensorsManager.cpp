/*
The purpose of this class is to manage the sensors of the system. It will read
the sensors and update the system state accordingly.
*/

#include "SensorsManager.h"
#include "Utils.h"
#include "config.h"
#include <DHT.h>
#include <cstring>

SensorsManager::SensorsManager()
    : m_dht(
    Config::Pins::DHT_SENSOR_PIN,
    Config::Hardware::SENSOR_TYPE_DH22
    ) {}

void SensorsManager::begin() {
  m_dht.begin();
  pinMode(Config::Pins::SOIL_MOISTURE_ADC, INPUT);
}

SensorsManager::SensorData SensorsManager::readSensors() {
  SensorData data;
  data.temperature = getTemperature();
  data.humidity = getHumidity();
  data.soilMoisture = getSoilMoisture();
  const char *ts = getFormattedTimestamp();
  if (ts != nullptr) {
    strncpy(data.timestamp, ts, sizeof(data.timestamp) - 1);
    data.timestamp[sizeof(data.timestamp) - 1] = '\0';
  } else {
    strncpy(data.timestamp, "0000-00-00 00:00:00", sizeof(data.timestamp) - 1);
    data.timestamp[sizeof(data.timestamp) - 1] = '\0';
  }
  return data;
}

SensorsManager::SensorData SensorsManager::getMockedValues() {
  SensorData data;
  data.temperature = 20;
  data.humidity = 60;
  data.soilMoisture = 40;
  const char *ts = getFormattedTimestamp();
  if (ts != nullptr) {
    strncpy(data.timestamp, ts, sizeof(data.timestamp) - 1);
    data.timestamp[sizeof(data.timestamp) - 1] = '\0';
  } else {
    strncpy(data.timestamp, "0000-00-00 00:00:00", sizeof(data.timestamp) - 1);
    data.timestamp[sizeof(data.timestamp) - 1] = '\0';
  }
  return data;
}

int8_t SensorsManager::getTemperature() {
  float t = m_dht.readTemperature();
  if (isnan(t))
    return -128;
  float rounded = round(t);
  if (rounded > 127.0f)
    rounded = 127.0f;
  if (rounded < -127.0f)
    rounded = -127.0f;

  return static_cast<int8_t>(rounded);
}

uint8_t SensorsManager::getHumidity() {
  float h = m_dht.readHumidity();
  if (isnan(h))
    return 255;
  float rounded = round(h);
  if (rounded > 100.0f)
    rounded = 100.0f;
  if (rounded < 0.0f)
    rounded = 0.0f;
  return static_cast<uint8_t>(rounded);
}

uint8_t SensorsManager::getSoilMoisture() {
  int rawValue = analogRead(Config::Pins::SOIL_MOISTURE_ADC);
  if (rawValue < 0) rawValue = 0;
  if (rawValue > 4095) rawValue = 4095;
  // Mapeia o valor analógico do ESP32 (4095=seco, 0=úmido) para porcentagem (0-100)
  long mapped = map(rawValue, 4095, 0, 0, 100);
  if (mapped < 0) mapped = 0;
  if (mapped > 100) mapped = 100;
  return static_cast<uint8_t>(mapped);
}