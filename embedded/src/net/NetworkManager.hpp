#pragma once

#include "esp_err.h"
#include "esp_event.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "types.hpp"

class NetworkManager {
public:
  NetworkManager();
  ~NetworkManager();

  esp_err_t init();
  esp_err_t setStaCredentials(const char *ssid, const char *password);
  void triggerReconnect();
  NetworkStatus getStatus() const;

private:
  static void wifiEventHandler(void *arg, esp_event_base_t eventBase, int32_t eventId, void *eventData);
  static void ipEventHandler(void *arg, esp_event_base_t eventBase, int32_t eventId, void *eventData);
  static void reconnectTimerCallback(void *arg);
  static void apTimeoutCallback(void *arg);

  void onWifiEvent(int32_t eventId, void *eventData);
  void onIpEvent(int32_t eventId, void *eventData);
  void startSoftApFallback();
  void stopSoftAp();
  void initSntpOnce();

  mutable SemaphoreHandle_t m_mutex = nullptr;
  esp_timer_handle_t m_reconnectTimer = nullptr;
  esp_timer_handle_t m_apTimeoutTimer = nullptr;

  NetworkStatus m_status{};
  bool m_sntpStarted = false;
  bool m_mdnsStarted = false;
  uint8_t m_retryCount = 0;
  uint32_t m_backoffMs = 10000;
};
