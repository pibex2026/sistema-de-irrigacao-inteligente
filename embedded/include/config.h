#pragma once

#include <cstdint>

namespace Config {

    namespace Hardware {
        
        constexpr uint8_t SENSOR_TYPE_DH22 = 22;
        constexpr uint8_t SENSOR_TYPE_DH11 = 11; 
    }

    namespace Pins {
        constexpr uint8_t DHT_SENSOR_PIN = 5; 

        constexpr uint8_t SOIL_MOISTURE_ADC = 32; 
        constexpr uint8_t RELAY_VALVE = 25; 

        //LED onboard
        constexpr uint8_t STATUS_LED  = 2;  
    }


    namespace Network {
        constexpr const char* DEFAULT_AP_NAME = "ESP Irrigacao";
        constexpr const char* DEFAULT_AP_PASSWORD = "";
        constexpr uint8_t DEFAULT_GATEWAY[4]= {192, 168, 1, 1};
        constexpr uint8_t DEFAULT_SUBNET[4]= {255, 255, 255, 0};
        constexpr uint8_t DEFAULT_STATIC_IP[4] = {192, 168, 0, 100};
    }

    namespace DataLog {
        constexpr const char* LittleFS_MOUNT_POINT = "/littlefs";
        constexpr const char* SENSORS_READINGS_LOG = "/readings.csv";
        constexpr const char* INFERENCE_LOG = "/inference.csv";
    }

}