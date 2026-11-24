#pragma once

#include "ConfigManager.hpp"
#include "NetworkManager.hpp"
#include "SystemStateStore.hpp"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

class TelemetryClient {
public:
  TelemetryClient(ConfigManager &cfgMgr, NetworkManager &netMgr, SystemStateStore &stateStore);
  ~TelemetryClient();

  esp_err_t start();
  void stop();
  void notifyValveStateChanged();

private:
  static void telemetryTask(void *arg);
  bool publishSnapshot(const SystemConfig &cfg, const SystemSnapshot &snap,
                       bool includeValveEvent, bool valveEventOpen,
                       uint32_t durationSec, bool fallbackTriggered);

  ConfigManager &m_cfgMgr;
  NetworkManager &m_netMgr;
  SystemStateStore &m_stateStore;

  TaskHandle_t m_taskHandle = nullptr;
  volatile bool m_running = false;
};
