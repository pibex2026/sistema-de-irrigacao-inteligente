#pragma once

#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "types.hpp"
#include <vector>

class FileLogger {
public:
  FileLogger();
  ~FileLogger();

  esp_err_t init();
  bool isMounted() const { return m_mounted; }

  bool logSensorData(const SensorData &data);
  bool logInferenceData(const InferenceLogEntry &entry);

  std::vector<SensorData> readRecentReadings(size_t maxCount) const;
  std::vector<InferenceLogEntry> readRecentInferences(size_t maxCount) const;
  StorageStats getStorageStats() const;

  SemaphoreHandle_t getMutex() const { return m_mutex; }

private:
  bool ensureCsvHeader(const char *path, const char *header) const;
  void rotateIfNeeded(const char *path, const char *prevPath, size_t maxBytes, const char *header) const;

  bool m_mounted = false;
  mutable SemaphoreHandle_t m_mutex = nullptr;
};
