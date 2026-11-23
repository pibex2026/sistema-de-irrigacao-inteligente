#pragma once

#include "config.hpp"
#include "types.hpp"
#include <algorithm>

inline void sanitizeSystemConfig(SystemConfig &cfg, uint32_t schemaVersion = 2) {
  cfg.schemaVersion = schemaVersion;
  if (cfg.soilType > 7) cfg.soilType = 0;
  if (cfg.cropId > 5) cfg.cropId = 0;
  if (cfg.seedlingStage > 8) cfg.seedlingStage = 0;

  cfg.confidenceThreshold = std::clamp(cfg.confidenceThreshold, 0.30f, 0.95f);
  cfg.thermalStressThreshold = std::clamp(cfg.thermalStressThreshold, 0.25f, 0.85f);

  if (cfg.soilAdcDry > 4095) cfg.soilAdcDry = Config::Defaults::SOIL_ADC_DRY;
  if (cfg.soilAdcWet > 4095) cfg.soilAdcWet = Config::Defaults::SOIL_ADC_WET;
  if (cfg.soilAdcDry == cfg.soilAdcWet) {
    cfg.soilAdcDry = Config::Defaults::SOIL_ADC_DRY;
    cfg.soilAdcWet = Config::Defaults::SOIL_ADC_WET;
  }

  cfg.maxIrrigationMs = std::clamp<uint32_t>(cfg.maxIrrigationMs, 5000, 600000);
  cfg.cooldownMs = std::clamp<uint32_t>(cfg.cooldownMs, 10000, 3600000);
  cfg.sensorReadIntervalMs = std::clamp<uint32_t>(cfg.sensorReadIntervalMs, 2500, 300000);
  cfg.logIntervalMs = std::clamp<uint32_t>(cfg.logIntervalMs, 10000, 3600000);
  cfg.telemetryIntervalMs = std::clamp<uint32_t>(cfg.telemetryIntervalMs, 5000, 3600000);
  cfg.backendUrl[sizeof(cfg.backendUrl) - 1] = '\0';
  cfg.deviceApiKey[sizeof(cfg.deviceApiKey) - 1] = '\0';
}
