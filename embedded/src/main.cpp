#include <Arduino.h>
#include <LittleFS.h>
#include "config.h"
#include "constants.h"
#include "FileLogger.h"
#include "SensorsManager.h"
#include "NetworkManager.h"
#include "Utils.h"

enum class SystemState {
  INIT,
  READ_SENSORS,
  RUN_INFERENCE,
  EXECUTE_ACTION,
  ERROR
};

// Gerenciadores principais
Storage::FileLogger fileLogger;
SensorsManager sensorsManager;
NetworkManager networkManager;

SystemState currentState = SystemState::INIT;
unsigned long lastSensorReadTime = 0;
constexpr unsigned long SENSOR_READ_INTERVAL = 10000; // 10 segundos

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n[Sistema de Irrigacao Inteligente] Inicializando...");

  // 1. Inicializa Sistema de Arquivos (LittleFS)
  if (!LittleFS.begin(true, Config::DataLog::LittleFS_MOUNT_POINT)) {
    Serial.println("[LittleFS] Falha ao montar o sistema de arquivos.");
  } else {
    Serial.println("[LittleFS] Montado com sucesso.");
  }

  // 2. Inicializa Gerenciador de Armazenamento / Logs
  fileLogger.begin();

  // 3. Inicializa Módulo de Sensores
  sensorsManager.begin();
  Serial.println("[Sensores] DHT22 e sensor de solo configurados.");

  // 4. Inicializa Conectividade de Rede
  networkManager.begin();
  Serial.println("[Rede] NetworkManager iniciado.");

  // 5. Configura pinos de atuadores
  pinMode(Config::Pins::RELAY_VALVE, OUTPUT);
  digitalWrite(Config::Pins::RELAY_VALVE, LOW);

  pinMode(Config::Pins::STATUS_LED, OUTPUT);
  digitalWrite(Config::Pins::STATUS_LED, HIGH);

  currentState = SystemState::READ_SENSORS;
  Serial.println("[Sistema] Setup concluido com sucesso.\n");
}

void loop() {
  // Processa tarefas de rede em segundo plano (portal cativo e reconexão)
  networkManager.loop();

  unsigned long currentMillis = millis();

  // Loop temporizado para leitura dos sensores
  if (currentMillis - lastSensorReadTime >= SENSOR_READ_INTERVAL) {
    lastSensorReadTime = currentMillis;

    // Leitura dos dados
    SensorsManager::SensorData data = sensorsManager.readSensors();

    Serial.printf("[Leitura] Temp: %d C | Umid: %u%% | Solo: %u%% | Data: %s\n",
                  data.temperature, data.humidity, data.soilMoisture, data.timestamp);

    // Registro no arquivo CSV via LittleFS
    bool logged = fileLogger.logData(data);
    if (!logged) {
      Serial.println("[Storage] Aviso: Nao foi possivel gravar os dados no log.");
    }

    // Regra simples de acionamento inicial (enquanto modelo de IA não for integrado)
    // Se a umidade do solo estiver abaixo de 30%, aciona irrigação
    if (data.soilMoisture < 30 && data.soilMoisture > 0) {
      digitalWrite(Config::Pins::RELAY_VALVE, HIGH);
      Serial.println("[Atuacao] Solo seco detectado. Valvula de irrigacao ABERTA.");
    } else {
      digitalWrite(Config::Pins::RELAY_VALVE, LOW);
      Serial.println("[Atuacao] Nivel de umidade adequado. Valvula FECHADA.");
    }
  }

  delay(20);
}
