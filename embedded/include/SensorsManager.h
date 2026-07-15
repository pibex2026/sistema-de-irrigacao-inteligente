#include "config.h"
#include <DHT.h>

#pragma once

class SensorsManager { 
 
    public:
       struct SensorData {
        int8_t temperature;
        uint8_t humidity;
        int soilMoisture;
        const char* timestamp;
    };
        SensorsManager();
        void begin();
        SensorData readSensors();
 
    private:
        
        DHT m_dht;
        int8_t getTemperature();
        uint8_t getHumidity();
        int getSoilMoisture();
};