#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "types.hpp"
#include <cstdint>

class SystemStateStore {
public:
  SystemStateStore();
  ~SystemStateStore();

  SystemSnapshot getSnapshot() const;

  void updateFromSensorAi(SystemState state, const SensorData &sensors,
                          const InferenceResult &inference,
                          const PerformanceStats &sensorStats);
  void updateFromActuator(const ActuatorStatus &actuator,
                          uint32_t valveActivations,
                          uint32_t actuatorStackFreeBytes);
  void updateLoggerMetrics(uint32_t queuePending, uint32_t droppedCount,
                           uint32_t loggerStackFreeBytes);
  void updateTelemetryMetrics(uint32_t telemetryStackFreeBytes);

private:
  SystemSnapshot m_snapshot{};
  mutable SemaphoreHandle_t m_mutex = nullptr;
};
