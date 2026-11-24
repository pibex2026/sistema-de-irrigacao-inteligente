#include "SystemStateStore.hpp"
#include "TimeUtils.hpp"
#include "esp_system.h"

SystemStateStore::SystemStateStore() {
  m_mutex = xSemaphoreCreateMutex();
}

SystemStateStore::~SystemStateStore() {
  if (m_mutex != nullptr) {
    vSemaphoreDelete(m_mutex);
    m_mutex = nullptr;
  }
}

SystemSnapshot SystemStateStore::getSnapshot() const {
  SystemSnapshot copy{};
  if (m_mutex == nullptr) return copy;
  xSemaphoreTake(m_mutex, portMAX_DELAY);
  copy = m_snapshot;
  xSemaphoreGive(m_mutex);
  return copy;
}

void SystemStateStore::updateFromSensorAi(SystemState state, const SensorData &sensors,
                                            const InferenceResult &inference,
                                            const PerformanceStats &sensorStats) {
  if (m_mutex == nullptr) return;
  xSemaphoreTake(m_mutex, portMAX_DELAY);
  m_snapshot.state = state;
  m_snapshot.lastSensors = sensors;
  m_snapshot.lastInference = inference;

  m_snapshot.performance.totalSensorReads = sensorStats.totalSensorReads;
  m_snapshot.performance.sensorErrorCount = sensorStats.sensorErrorCount;
  m_snapshot.performance.totalInferences = sensorStats.totalInferences;
  m_snapshot.performance.countOff = sensorStats.countOff;
  m_snapshot.performance.countIrrigate = sensorStats.countIrrigate;
  m_snapshot.performance.countThermalStress = sensorStats.countThermalStress;
  m_snapshot.performance.lastLatencyUs = sensorStats.lastLatencyUs;
  m_snapshot.performance.minLatencyUs = sensorStats.minLatencyUs;
  m_snapshot.performance.maxLatencyUs = sensorStats.maxLatencyUs;
  m_snapshot.performance.avgLatencyUs = sensorStats.avgLatencyUs;
  m_snapshot.performance.tfliteArenaUsedBytes = sensorStats.tfliteArenaUsedBytes;
  m_snapshot.performance.sensorAiTaskStackFreeBytes = sensorStats.sensorAiTaskStackFreeBytes;
  m_snapshot.performance.controlTaskStackFreeBytes = sensorStats.sensorAiTaskStackFreeBytes;

  m_snapshot.freeHeapBytes = esp_get_free_heap_size();
  m_snapshot.uptimeSec = TimeUtils::getUptimeSec();
  xSemaphoreGive(m_mutex);
}

void SystemStateStore::updateFromActuator(const ActuatorStatus &actuator,
                                            uint32_t valveActivations,
                                            uint32_t actuatorStackFreeBytes) {
  if (m_mutex == nullptr) return;
  xSemaphoreTake(m_mutex, portMAX_DELAY);
  m_snapshot.actuator = actuator;
  m_snapshot.performance.valveActivations = valveActivations;
  m_snapshot.performance.actuatorTaskStackFreeBytes = actuatorStackFreeBytes;
  m_snapshot.freeHeapBytes = esp_get_free_heap_size();
  m_snapshot.uptimeSec = TimeUtils::getUptimeSec();
  xSemaphoreGive(m_mutex);
}

void SystemStateStore::updateLoggerMetrics(uint32_t queuePending, uint32_t droppedCount,
                                             uint32_t loggerStackFreeBytes) {
  if (m_mutex == nullptr) return;
  xSemaphoreTake(m_mutex, portMAX_DELAY);
  m_snapshot.performance.logQueuePending = queuePending;
  m_snapshot.performance.logDroppedCount = droppedCount;
  m_snapshot.performance.loggerTaskStackFreeBytes = loggerStackFreeBytes;
  xSemaphoreGive(m_mutex);
}

void SystemStateStore::updateTelemetryMetrics(uint32_t telemetryStackFreeBytes) {
  if (m_mutex == nullptr) return;
  xSemaphoreTake(m_mutex, portMAX_DELAY);
  m_snapshot.performance.telemetryTaskStackFreeBytes = telemetryStackFreeBytes;
  xSemaphoreGive(m_mutex);
}
