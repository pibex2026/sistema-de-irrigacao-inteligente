#pragma once

#include <cstdint>

namespace Config {

    namespace Pins {
        constexpr uint8_t DHT_SENSOR_PIN = 4; 

        constexpr uint8_t SOIL_MOISTURE_ADC = 32; 
        constexpr uint8_t RELAY_VALVE = 25; 

        //LED onboard
        constexpr uint8_t STATUS_LED  = 2;  
    }

    namespace Hardware {
        
        #define DHT_SENSOR_TYPE DHT22 
    }

    namespace Network {
        constexpr const char* DEFAULT_AP_NAME = "ESP Irrigacao";
        constexpr const char* DEFAULT_AP_PASSWORD = "";
    }

    namespace DataLog {
        constexpr const char* LittleFS_MOUNT_POINT = "/littlefs";
        constexpr const char* SENSORS_READINGS_LOG = "/readings.csv";
        constexpr const char* INFERENCE_LOG = "/inference.csv";
    }

}