#pragma once

#include "ConfigManager.hpp"
#include "FileLogger.hpp"
#include "NetworkManager.hpp"
#include "SensorsManager.hpp"
#include "esp_err.h"
#include "esp_http_server.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "types.hpp"

class LocalWebServer {
public:
  LocalWebServer(ConfigManager &cfgMgr, FileLogger &logger, NetworkManager &netMgr,
                 SensorsManager &sensorsMgr);
  ~LocalWebServer();

  esp_err_t start();
  void stop();
  void updateSnapshot(const SystemSnapshot &snap);
  SystemSnapshot getSnapshot() const;

private:
  static esp_err_t handleRoot(httpd_req_t *req);
  static esp_err_t handleCaptivePortalApi(httpd_req_t *req);
  static esp_err_t handleFavicon(httpd_req_t *req);
  static esp_err_t handleCaptiveRedirect(httpd_req_t *req);
  static esp_err_t handle404CaptiveRedirect(httpd_req_t *req, httpd_err_code_t err);
  static esp_err_t handleGetStatus(httpd_req_t *req);
  static esp_err_t handleGetPerformance(httpd_req_t *req);
  static esp_err_t handleGetSystemInfo(httpd_req_t *req);
  static esp_err_t handleGetConfig(httpd_req_t *req);
  static esp_err_t handlePostConfig(httpd_req_t *req);
  static esp_err_t handleGetWifiScan(httpd_req_t *req);
  static esp_err_t handlePostWifi(httpd_req_t *req);
  static esp_err_t handleCalibrateDry(httpd_req_t *req);
  static esp_err_t handleCalibrateWet(httpd_req_t *req);
  static esp_err_t handleDownloadReadings(httpd_req_t *req);
  static esp_err_t handleDownloadInference(httpd_req_t *req);

  esp_err_t streamCsvFile(httpd_req_t *req, const char *path, const char *downloadName) const;

  ConfigManager &m_cfgMgr;
  FileLogger &m_logger;
  NetworkManager &m_netMgr;
  SensorsManager &m_sensorsMgr;

  httpd_handle_t m_server = nullptr;
  SystemSnapshot m_snapshot{};
  mutable SemaphoreHandle_t m_snapMutex = nullptr;
};
