#pragma once

#include "config.h"
#include <DHT.h>
#include <cstdint>

class SensorsManager {

public:
  struct SensorData {
    int8_t temperature;
    uint8_t humidity;
    uint8_t soilMoisture;
    char timestamp[20];
  };
  SensorsManager();
  void begin();
  SensorData readSensors();
  SensorData getMockedValues();

private:
  DHT m_dht;
  int8_t getTemperature();
  uint8_t getHumidity();
  uint8_t getSoilMoisture();
};