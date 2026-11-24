#pragma once

#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "types.hpp"
#include <atomic>
#include <vector>

class FileLogger {
public:
  FileLogger();
  ~FileLogger();

  esp_err_t init();
  bool isMounted() const { return m_mounted; }

  bool enqueueSensorData(const SensorData &data);
  bool enqueueInferenceData(const InferenceLogEntry &entry);
  bool logSensorData(const SensorData &data);
  bool logInferenceData(const InferenceLogEntry &entry);

  std::vector<SensorData> readRecentReadings(size_t maxCount) const;
  std::vector<InferenceLogEntry> readRecentInferences(size_t maxCount) const;
  StorageStats getStorageStats() const;

  uint32_t getQueuePending() const;
  uint32_t getDroppedCount() const { return m_droppedCount.load(); }
  uint32_t getTaskStackFreeBytes() const { return m_stackFreeBytes.load(); }

  SemaphoreHandle_t getMutex() const { return m_mutex; }

private:
  struct LogQueueItem {
    enum class Kind : uint8_t { SENSOR = 0, INFERENCE = 1, STOP = 2 };
    Kind kind = Kind::SENSOR;
    SensorData sensor{};
    InferenceLogEntry inference{};
  };

  static void loggerTask(void *arg);
  bool enqueueItem(const LogQueueItem &item);
  bool writeSensorDataSync(const SensorData &data);
  bool writeInferenceDataSync(const InferenceLogEntry &entry);
  bool ensureCsvHeader(const char *path, const char *header) const;
  void rotateIfNeeded(const char *path, const char *prevPath, size_t maxBytes, const char *header) const;

  bool m_mounted = false;
  mutable SemaphoreHandle_t m_mutex = nullptr;
  QueueHandle_t m_queue = nullptr;
  TaskHandle_t m_taskHandle = nullptr;
  std::atomic<bool> m_running{false};
  std::atomic<uint32_t> m_droppedCount{0};
  std::atomic<uint32_t> m_stackFreeBytes{0};
};
