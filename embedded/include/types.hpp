#pragma once

#include "config.hpp"
#include "model_data.h"
#include <cstdint>

enum class SystemState : uint8_t {
  INIT = 0,
  READ_SENSORS,
  RUN_INFERENCE,
  EXECUTE_ACTION,
  ERROR
};

enum class IrrigationDecision : uint8_t {
  OFF = 0,
  IRRIGATE = 1,
  THERMAL_STRESS_WET_SOIL = 2
};

struct SystemConfig {
  uint32_t schemaVersion = 2;
  uint8_t soilType = Config::Defaults::SOIL_TYPE;
  uint8_t cropId = Config::Defaults::CROP_ID;
  uint8_t seedlingStage = Config::Defaults::SEEDLING_STAGE;
  float confidenceThreshold = Config::Defaults::CONFIDENCE_THRESHOLD;
  float thermalStressThreshold = Config::Defaults::THERMAL_STRESS_THRESHOLD;
  uint16_t soilAdcDry = Config::Defaults::SOIL_ADC_DRY;
  uint16_t soilAdcWet = Config::Defaults::SOIL_ADC_WET;
  uint32_t maxIrrigationMs = Config::Defaults::MAX_IRRIGATION_MS;
  uint32_t cooldownMs = Config::Defaults::COOLDOWN_MS;
  uint32_t sensorReadIntervalMs = Config::Defaults::SENSOR_READ_INTERVAL_MS;
  uint32_t logIntervalMs = Config::Defaults::LOG_INTERVAL_MS;
  uint32_t telemetryIntervalMs = Config::Defaults::TELEMETRY_INTERVAL_MS;
  char backendUrl[128] = "";
  char deviceApiKey[64] = "";
};

struct SensorData {
  int16_t temperatureX10 = TinyMLConfig::SENTINEL_TEMP_ERROR_I16; // ex: 254 = 25.4 C
  uint8_t humidity = TinyMLConfig::SENTINEL_HUM_ERROR_U8;        // 0..100 (255 = erro)
  uint8_t soilMoisture = TinyMLConfig::SENTINEL_HUM_ERROR_U8;    // 0..100 (255 = erro)
  uint16_t rawSoilAdc = 0;
  bool valid = false;
  char timestamp[24] = "0000-00-00 00:00:00";

  float getTemperatureC() const {
    return static_cast<float>(temperatureX10) / 10.0f;
  }
};

struct AgronomicContext {
  uint8_t soilType = 0;
  uint8_t cropId = 0;
  uint8_t seedlingStage = 0;
  float confidenceThreshold = TinyMLConfig::DEFAULT_CONFIDENCE_THRESHOLD;
  float thermalStressThreshold = Config::Defaults::THERMAL_STRESS_THRESHOLD;
};

struct InferenceResult {
  IrrigationDecision decision = IrrigationDecision::OFF;
  float probabilities[3] = {0.0f, 0.0f, 0.0f};
  float confidence = 0.0f;
  bool shouldIrrigate = false;
  bool thermalStressAlert = false;
  bool fallbackTriggered = false;
  bool valid = false;
  uint32_t latencyUs = 0;
  char timestamp[24] = "0000-00-00 00:00:00";
};

struct InferenceLogEntry {
  SensorData sensors;
  AgronomicContext context;
  InferenceResult inference;
  bool valveOpen = false;
};

struct ActuatorStatus {
  bool valveOpen = false;
  bool inCooldown = false;
  bool emergencyStopped = false;
  bool thermalStressAlert = false;
  uint32_t openElapsedMs = 0;
  uint32_t cooldownRemainingMs = 0;
  uint8_t consecutiveIrrigateCount = 0;
};

struct WifiScanEntry {
  char ssid[33] = "";
  int8_t rssi = -100;
  uint8_t authmode = 0;
  uint8_t channel = 0;
};

struct NetworkStatus {
  bool staConnected = false;
  bool staConnecting = false;
  bool apActive = false;
  bool ntpSynced = false;
  int8_t rssi = 0;
  uint8_t lastDisconnectReason = 0;
  char ssid[33] = "";
  char ip[16] = "";
  char lastError[64] = "";
};

struct PerformanceStats {
  uint32_t totalSensorReads = 0;
  uint32_t sensorErrorCount = 0;
  uint32_t totalInferences = 0;
  uint32_t countOff = 0;
  uint32_t countIrrigate = 0;
  uint32_t countThermalStress = 0;
  uint32_t valveActivations = 0;
  uint32_t lastLatencyUs = 0;
  uint32_t minLatencyUs = 0;
  uint32_t maxLatencyUs = 0;
  uint32_t avgLatencyUs = 0;
  uint32_t tfliteArenaUsedBytes = 0;
  uint32_t controlTaskStackFreeBytes = 0;
};

struct StorageStats {
  bool mounted = false;
  uint32_t totalBytes = 0;
  uint32_t usedBytes = 0;
  uint32_t readingsBytes = 0;
  uint32_t readingsPrevBytes = 0;
  uint32_t inferenceBytes = 0;
  uint32_t inferencePrevBytes = 0;
};

struct SystemSnapshot {
  SystemState state = SystemState::INIT;
  SensorData lastSensors;
  InferenceResult lastInference;
  ActuatorStatus actuator;
  NetworkStatus network;
  PerformanceStats performance;
  uint32_t freeHeapBytes = 0;
  uint32_t uptimeSec = 0;
};
