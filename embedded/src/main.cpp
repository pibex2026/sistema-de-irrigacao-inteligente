#include "ActuatorManager.hpp"
#include "ConfigManager.hpp"
#include "FileLogger.hpp"
#include "LocalWebServer.hpp"
#include "ModelManager.hpp"
#include "NetworkManager.hpp"
#include "SensorsManager.hpp"
#include "SystemStateStore.hpp"
#include "TelemetryClient.hpp"
#include "TimeUtils.hpp"
#include "config.hpp"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_task_wdt.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "types.hpp"

static const char *TAG = "MAIN";

static ConfigManager g_configMgr;
static FileLogger g_fileLogger;
static ActuatorManager g_actuatorMgr;
static SensorsManager g_sensorsMgr;
static ModelManager g_modelMgr;
static NetworkManager g_networkMgr;
static SystemStateStore g_stateStore;
static LocalWebServer g_webServer(g_configMgr, g_fileLogger, g_networkMgr, g_sensorsMgr,
                                  g_actuatorMgr, g_stateStore);
static TelemetryClient g_telemetryClient(g_configMgr, g_networkMgr, g_stateStore);

static QueueHandle_t g_actuatorCmdQueue = nullptr;

static void actuatorSafetyTask(void *) {
  esp_task_wdt_add(nullptr);

  bool prevValveOpen = false;
  uint32_t valveActivations = 0;

  while (true) {
    esp_task_wdt_reset();

    const SystemConfig cfg = g_configMgr.get();
    ActuatorCommand cmd{};
    const bool hasCmd = (g_actuatorCmdQueue != nullptr) &&
                        (xQueueReceive(g_actuatorCmdQueue, &cmd,
                                       pdMS_TO_TICKS(Config::Tasks::ACTUATOR_TICK_MS)) == pdTRUE);

    if (hasCmd) {
      if (cmd.type == ActuatorCommandType::EMERGENCY_STOP) {
        g_actuatorMgr.emergencyStop();
      } else {
        g_actuatorMgr.applyDecision(cmd.inference, cfg);
        if (cmd.shouldLogInference) {
          const ActuatorStatus actSt = g_actuatorMgr.getStatus();
          InferenceLogEntry entry{};
          entry.sensors = cmd.sensors;
          entry.context = cmd.context;
          entry.inference = cmd.inference;
          entry.valveOpen = actSt.valveOpen;
          g_fileLogger.enqueueInferenceData(entry);
        }
      }
    }

    g_actuatorMgr.update(cfg);

    const ActuatorStatus curAct = g_actuatorMgr.getStatus();
    if (curAct.valveOpen != prevValveOpen) {
      if (curAct.valveOpen) {
        valveActivations++;
      }
      prevValveOpen = curAct.valveOpen;
      g_telemetryClient.notifyValveStateChanged();
    }

    const uint32_t stackFree =
        static_cast<uint32_t>(uxTaskGetStackHighWaterMark(nullptr) * sizeof(StackType_t));
    g_stateStore.updateFromActuator(curAct, valveActivations, stackFree);
  }
}

static void sensorAiTask(void *) {
  esp_task_wdt_add(nullptr);

  SystemState currentState = SystemState::INIT;
  SensorData lastSensorData{};
  InferenceResult lastInference{};
  AgronomicContext lastContext{};

  int64_t lastReadMs = -1000000;
  int64_t lastReadingsLogMs = -1000000;
  int64_t lastInferenceLogMs = -1000000;
  uint8_t consecutiveSensorErrors = 0;
  bool modelReady = false;
  uint64_t latencyAccumUs = 0;
  PerformanceStats perf{};

  while (true) {
    esp_task_wdt_reset();

    const SystemConfig cfg = g_configMgr.get();
    const int64_t nowMs = TimeUtils::getMillis();

    switch (currentState) {
    case SystemState::INIT: {
      modelReady = (g_modelMgr.init() == ESP_OK);
      if (!modelReady) {
        ESP_LOGE(TAG, "Modelo TinyML indisponivel. Entrando em modo ERROR.");
        currentState = SystemState::ERROR;
      } else {
        perf.tfliteArenaUsedBytes = static_cast<uint32_t>(g_modelMgr.getArenaUsedBytes());
        if (!g_modelMgr.runGoldenSelfTest()) {
          ESP_LOGW(TAG, "Aviso: um ou mais Golden Test Vectors divergiram.");
        }
        currentState = SystemState::READ_SENSORS;
      }
      break;
    }

    case SystemState::READ_SENSORS: {
      if ((nowMs - lastReadMs) >= static_cast<int64_t>(cfg.sensorReadIntervalMs)) {
        lastReadMs = nowMs;
        lastSensorData = g_sensorsMgr.readSensors(cfg);
        perf.totalSensorReads++;

        ESP_LOGI(TAG, "[Sensores] T=%.1f C | RH=%u%% | Solo=%u%% (ADC=%u) | Valido=%s | %s",
                 lastSensorData.getTemperatureC(),
                 static_cast<unsigned>(lastSensorData.humidity),
                 static_cast<unsigned>(lastSensorData.soilMoisture),
                 static_cast<unsigned>(lastSensorData.rawSoilAdc),
                 lastSensorData.valid ? "SIM" : "NAO",
                 lastSensorData.timestamp);

        if (!lastSensorData.valid) {
          perf.sensorErrorCount++;
          consecutiveSensorErrors++;
          if (consecutiveSensorErrors >= Config::Hardware::MAX_CONSECUTIVE_SENSOR_FAILURES) {
            ESP_LOGE(TAG, "%u falhas seguidas de leitura de sensores -> ERROR!",
                     consecutiveSensorErrors);
            currentState = SystemState::ERROR;
          }
          break;
        }

        consecutiveSensorErrors = 0;
        if ((nowMs - lastReadingsLogMs) >= static_cast<int64_t>(cfg.logIntervalMs)) {
          lastReadingsLogMs = nowMs;
          g_fileLogger.enqueueSensorData(lastSensorData);
        }
        currentState = SystemState::RUN_INFERENCE;
      }
      break;
    }

    case SystemState::RUN_INFERENCE: {
      lastContext = AgronomicContext{};
      lastContext.soilType = cfg.soilType;
      lastContext.cropId = cfg.cropId;
      lastContext.seedlingStage = cfg.seedlingStage;
      lastContext.confidenceThreshold = cfg.confidenceThreshold;
      lastContext.thermalStressThreshold = cfg.thermalStressThreshold;

      lastInference = g_modelMgr.predict(lastSensorData, lastContext);
      if (!lastInference.valid) {
        ESP_LOGE(TAG, "Inferencia invalida -> ERROR.");
        currentState = SystemState::ERROR;
        break;
      }

      perf.totalInferences++;
      if (lastInference.decision == IrrigationDecision::OFF) perf.countOff++;
      else if (lastInference.decision == IrrigationDecision::IRRIGATE) perf.countIrrigate++;
      else if (lastInference.decision == IrrigationDecision::THERMAL_STRESS_WET_SOIL) perf.countThermalStress++;

      perf.lastLatencyUs = lastInference.latencyUs;
      if (perf.minLatencyUs == 0 || lastInference.latencyUs < perf.minLatencyUs) perf.minLatencyUs = lastInference.latencyUs;
      if (lastInference.latencyUs > perf.maxLatencyUs) perf.maxLatencyUs = lastInference.latencyUs;
      latencyAccumUs += lastInference.latencyUs;
      perf.avgLatencyUs = static_cast<uint32_t>(latencyAccumUs / perf.totalInferences);

      ESP_LOGI(TAG, "[IA INT8] Decisao=%u | P=[%.3f, %.3f, %.3f] | Irrigar=%s | Stress=%s | Fallback=%s | %lu us",
               static_cast<unsigned>(lastInference.decision),
               lastInference.probabilities[0],
               lastInference.probabilities[1],
               lastInference.probabilities[2],
               lastInference.shouldIrrigate ? "SIM" : "NAO",
               lastInference.thermalStressAlert ? "SIM" : "NAO",
               lastInference.fallbackTriggered ? "SIM" : "NAO",
               static_cast<unsigned long>(lastInference.latencyUs));

      currentState = SystemState::EXECUTE_ACTION;
      break;
    }

    case SystemState::EXECUTE_ACTION: {
      ActuatorCommand cmd{};
      cmd.type = ActuatorCommandType::APPLY_DECISION;
      cmd.inference = lastInference;
      cmd.sensors = lastSensorData;
      cmd.context = lastContext;
      cmd.shouldLogInference =
          ((nowMs - lastInferenceLogMs) >= static_cast<int64_t>(cfg.logIntervalMs)) ||
          lastInference.thermalStressAlert;
      if (cmd.shouldLogInference) {
        lastInferenceLogMs = nowMs;
      }

      if (g_actuatorCmdQueue != nullptr) {
        if (xQueueSend(g_actuatorCmdQueue, &cmd, pdMS_TO_TICKS(50)) != pdTRUE) {
          ESP_LOGW(TAG, "Fila de comandos do atuador cheia; comando descartado.");
        }
      }

      currentState = SystemState::READ_SENSORS;
      break;
    }

    case SystemState::ERROR: {
      if (g_actuatorCmdQueue != nullptr) {
        ActuatorCommand stopCmd{};
        stopCmd.type = ActuatorCommandType::EMERGENCY_STOP;
        xQueueSend(g_actuatorCmdQueue, &stopCmd, 0);
      } else {
        g_actuatorMgr.emergencyStop();
      }

      if (!modelReady) {
        vTaskDelay(pdMS_TO_TICKS(1000));
        break;
      }
      if ((nowMs - lastReadMs) >= static_cast<int64_t>(cfg.sensorReadIntervalMs)) {
        ESP_LOGW(TAG, "Tentando recuperacao de sensores apos estado ERROR...");
        currentState = SystemState::READ_SENSORS;
      }
      break;
    }
    }

    perf.sensorAiTaskStackFreeBytes =
        static_cast<uint32_t>(uxTaskGetStackHighWaterMark(nullptr) * sizeof(StackType_t));
    perf.controlTaskStackFreeBytes = perf.sensorAiTaskStackFreeBytes;

    g_stateStore.updateFromSensorAi(currentState, lastSensorData, lastInference, perf);
    g_stateStore.updateLoggerMetrics(g_fileLogger.getQueuePending(),
                                     g_fileLogger.getDroppedCount(),
                                     g_fileLogger.getTaskStackFreeBytes());

    vTaskDelay(pdMS_TO_TICKS(Config::Tasks::SENSOR_WDT_SLICE_MS));
  }
}

extern "C" void app_main(void) {
  ESP_LOGI(TAG, "============================================================");
  ESP_LOGI(TAG, " Sistema de Irrigacao Inteligente - PIBEX 2026 (IFPE)");
  ESP_LOGI(TAG, " Firmware ESP-IDF v5.5 | PhysMLP-27 Full INT8 (SMP)");
  ESP_LOGI(TAG, "============================================================");

  ESP_ERROR_CHECK(g_configMgr.init());
  ESP_ERROR_CHECK(g_actuatorMgr.init()); // Garante valvula fechada o quanto antes

  g_actuatorCmdQueue = xQueueCreate(Config::Tasks::ACTUATOR_CMD_QUEUE_LEN, sizeof(ActuatorCommand));
  if (g_actuatorCmdQueue == nullptr) {
    ESP_LOGE(TAG, "Falha ao criar fila de comandos do atuador.");
  }

  if (g_fileLogger.init() != ESP_OK) {
    ESP_LOGW(TAG, "LittleFS indisponivel; seguindo sem persistencia CSV.");
  }

  if (g_sensorsMgr.init() != ESP_OK) {
    ESP_LOGE(TAG, "Falha ao inicializar sensores.");
  }

  if (g_networkMgr.init() != ESP_OK) {
    ESP_LOGW(TAG, "Falha na pilha Wi-Fi; operando em modo 100%% offline.");
  }

  if (g_webServer.start() != ESP_OK) {
    ESP_LOGW(TAG, "Falha ao iniciar servidor HTTP local.");
  }

  if (g_telemetryClient.start() != ESP_OK) {
    ESP_LOGW(TAG, "Falha ao iniciar cliente de telemetria.");
  }

  xTaskCreatePinnedToCore(
      actuatorSafetyTask,
      "actuator_safety",
      Config::Tasks::ACTUATOR_SAFETY_STACK,
      nullptr,
      Config::Tasks::ACTUATOR_SAFETY_PRIO,
      nullptr,
      tskNO_AFFINITY);

  xTaskCreatePinnedToCore(
      sensorAiTask,
      "sensor_ai_task",
      Config::Tasks::SENSOR_AI_STACK,
      nullptr,
      Config::Tasks::SENSOR_AI_PRIO,
      nullptr,
      tskNO_AFFINITY);
}
