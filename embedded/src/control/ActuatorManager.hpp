#pragma once

#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "types.hpp"

class ActuatorManager {
public:
  ActuatorManager();
  ~ActuatorManager();

  esp_err_t init();
  void applyDecision(const InferenceResult &result, const SystemConfig &cfg);
  void update(const SystemConfig &cfg);
  bool manualOverride(bool open, const SystemConfig &cfg);
  void emergencyStop();
  ActuatorStatus getStatus() const;

private:
  void setValvePhysical(bool open);
  void setLedPhysical(bool on);
  void updateStatusLed(int64_t nowMs);

  mutable SemaphoreHandle_t m_mutex = nullptr;
  bool m_initialized = false;
  bool m_valveOpen = false;
  bool m_inCooldown = false;
  bool m_emergencyStopped = false;
  bool m_thermalStressAlert = false;
  bool m_ledState = false;

  int64_t m_valveOpenedAtMs = 0;
  int64_t m_cooldownStartedAtMs = 0;
  uint32_t m_activeCooldownMs = Config::Defaults::COOLDOWN_MS;
  int64_t m_lastLedToggleMs = 0;
  uint8_t m_ledBurstStep = 0;
  uint8_t m_consecutiveIrrigateCount = 0;
};
