#include "ActuatorManager.hpp"
#include "ConfigManager.hpp"
#include "FileLogger.hpp"
#include "LocalWebServer.hpp"
#include "ModelManager.hpp"
#include "NetworkManager.hpp"
#include "SensorsManager.hpp"
#include "TelemetryClient.hpp"
#include "TimeUtils.hpp"
#include "config.hpp"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_task_wdt.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "types.hpp"

static const char *TAG = "MAIN";

static ConfigManager g_configMgr;
static FileLogger g_fileLogger;
static ActuatorManager g_actuatorMgr;
static SensorsManager g_sensorsMgr;
static ModelManager g_modelMgr;
static NetworkManager g_networkMgr;
static LocalWebServer g_webServer(g_configMgr, g_fileLogger, g_networkMgr, g_sensorsMgr, g_actuatorMgr);
static TelemetryClient g_telemetryClient(g_configMgr, g_networkMgr, g_webServer);

static void controlTask(void *) {
  esp_task_wdt_add(nullptr);

  SystemState currentState = SystemState::INIT;
  SensorData lastSensorData{};
  InferenceResult lastInference{};

  int64_t lastReadMs = -1000000;
  int64_t lastReadingsLogMs = -1000000;
  int64_t lastInferenceLogMs = -1000000;
  uint8_t consecutiveSensorErrors = 0;
  bool modelReady = false;
  bool prevValveOpen = false;
  uint64_t latencyAccumUs = 0;
  PerformanceStats perf{};

  while (true) {
    esp_task_wdt_reset();

    const SystemConfig cfg = g_configMgr.get();
    const int64_t nowMs = TimeUtils::getMillis();
    g_actuatorMgr.update(cfg);

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
          g_fileLogger.logSensorData(lastSensorData);
        }
        currentState = SystemState::RUN_INFERENCE;
      }
      break;
    }

    case SystemState::RUN_INFERENCE: {
      AgronomicContext ctx{};
      ctx.soilType = cfg.soilType;
      ctx.cropId = cfg.cropId;
      ctx.seedlingStage = cfg.seedlingStage;
      ctx.confidenceThreshold = cfg.confidenceThreshold;
      ctx.thermalStressThreshold = cfg.thermalStressThreshold;

      lastInference = g_modelMgr.predict(lastSensorData, ctx);
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
      g_actuatorMgr.applyDecision(lastInference, cfg);
      const ActuatorStatus actSt = g_actuatorMgr.getStatus();

      if ((nowMs - lastInferenceLogMs) >= static_cast<int64_t>(cfg.logIntervalMs) ||
          lastInference.thermalStressAlert) {
        lastInferenceLogMs = nowMs;
        InferenceLogEntry entry{};
        entry.sensors = lastSensorData;
        entry.context.soilType = cfg.soilType;
        entry.context.cropId = cfg.cropId;
        entry.context.seedlingStage = cfg.seedlingStage;
        entry.inference = lastInference;
        entry.valveOpen = actSt.valveOpen;
        g_fileLogger.logInferenceData(entry);
      }

      currentState = SystemState::READ_SENSORS;
      break;
    }

    case SystemState::ERROR: {
      g_actuatorMgr.emergencyStop();
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

    const ActuatorStatus curAct = g_actuatorMgr.getStatus();
    if (curAct.valveOpen && !prevValveOpen) {
      perf.valveActivations++;
    }
    prevValveOpen = curAct.valveOpen;
    perf.controlTaskStackFreeBytes = static_cast<uint32_t>(uxTaskGetStackHighWaterMark(nullptr) * sizeof(StackType_t));

    SystemSnapshot snap{};
    snap.state = currentState;
    snap.lastSensors = lastSensorData;
    snap.lastInference = lastInference;
    snap.actuator = curAct;
    snap.performance = perf;
    snap.freeHeapBytes = esp_get_free_heap_size();
    snap.uptimeSec = TimeUtils::getUptimeSec();
    g_webServer.updateSnapshot(snap);

    vTaskDelay(pdMS_TO_TICKS(250));
  }
}

extern "C" void app_main(void) {
  ESP_LOGI(TAG, "============================================================");
  ESP_LOGI(TAG, " Sistema de Irrigacao Inteligente - PIBEX 2026 (IFPE)");
  ESP_LOGI(TAG, " Firmware ESP-IDF v5.5 | PhysMLP-27 Full INT8");
  ESP_LOGI(TAG, "============================================================");

  ESP_ERROR_CHECK(g_configMgr.init());
  ESP_ERROR_CHECK(g_actuatorMgr.init()); // Garante valvula fechada o quanto antes

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

  // Task de controle, IA e atuacao fixada no Core 1 (APP_CPU)
  xTaskCreatePinnedToCore(
      controlTask,
      "control_task",
      8192,
      nullptr,
      5,
      nullptr,
      1);
}
