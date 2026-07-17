#include "config.h"
#include <DHT.h>

#pragma once

class SensorsManager {

public:
  struct SensorData {
    int8_t temperature;
    uint8_t humidity;
    uint8_t soilMoisture;
    const char *timestamp;
  };
  SensorsManager();
  void begin();
  SensorData readSensors();

private:
  DHT m_dht;
  int8_t getTemperature();
  uint8_t getHumidity();
  uint8_t getSoilMoisture();
};