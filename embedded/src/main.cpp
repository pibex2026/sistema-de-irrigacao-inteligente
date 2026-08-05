#include <Arduino.h>
#include "orchestrator.h"
#include "SensorsManager.h"
#include "NetworkManager.h"
#include "config.h"
#include "secrets.h"

SensorsManager sensor;
NetworkManager net;

void setup() {
    Serial.begin(115200);
    net.begin(NetworkMode::MODE_WIFI_MANAGER, "ESP32_Lavoura", "brisa-pitoco", Secrets::WIFI_DEFAULT_PASSWORD);
    delay(1500);
    SensorsManager::SensorData sData = sensor.getMockedValues();
    Serial.printf("Temperatura: %d °C | Umidade: %u %%\n", sData.temperature, sData.humidity);
    Serial.printf("IP: %s\n", net.getIP());


    Config::Hardware::SENSOR_TYPE_DH22;
}

void loop() {}

 
