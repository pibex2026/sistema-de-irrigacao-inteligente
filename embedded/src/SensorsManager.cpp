/*
The purpose os this class is to manage the sensors of the system. It will read the sensors and update the system state accordingly. 
*/


#include <DHT.h>
#include "config.h"
#include "SensorsManager.h"
#include "Utils.h"


SensorsManager::SensorsManager() : m_dht(Config::Pins::DHT_SENSOR_PIN, DHT_SENSOR_TYPE) {}

void SensorsManager::begin() {
    m_dht.begin();
    pinMode(Config::Pins::SOIL_MOISTURE_ADC, INPUT);
}

SensorsManager::SensorData SensorsManager::readSensors() {
    SensorData data;
    data.temperature = getTemperature();
    data.humidity = getHumidity();
    data.soilMoisture = getSoilMoisture();
    data.timestamp = getFormattedTimestamp();
    return data;
}

int8_t SensorsManager::getTemperature() {
    float t = m_dht.readTemperature();
    if (isnan(t)) return -128;
    float rounded = round(t);
    if (rounded > 127.0f) rounded = 127.0f;
    if (rounded < -127.0f) rounded = -127.0f;
    
    return static_cast<int8_t>(rounded);
}

uint8_t SensorsManager::getHumidity() {
    float h = m_dht.readHumidity();
    float rounded = round(h);
    if (rounded > 100.0f) rounded = 100.0f;
    if (rounded < 0.0f) rounded = 0.0f;
    return static_cast<uint8_t>(rounded);
}

int SensorsManager::getSoilMoisture() {
    int rawValue = analogRead(Config::Pins::SOIL_MOISTURE_ADC);
    // Mapeia o valor analógico do ESP32 (0-4095) para porcentagem (0-100)
    return map(rawValue, 4095, 0, 0, 100); 
}