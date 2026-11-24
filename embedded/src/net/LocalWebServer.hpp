#pragma once

#include "ActuatorManager.hpp"
#include "ConfigManager.hpp"
#include "FileLogger.hpp"
#include "NetworkManager.hpp"
#include "SensorsManager.hpp"
#include "SystemStateStore.hpp"
#include "esp_err.h"
#include "esp_http_server.h"
#include "types.hpp"

class LocalWebServer {
public:
  LocalWebServer(ConfigManager &cfgMgr, FileLogger &logger, NetworkManager &netMgr,
                 SensorsManager &sensorsMgr, ActuatorManager &actuatorMgr,
                 SystemStateStore &stateStore);
  ~LocalWebServer();

  esp_err_t start();
  void stop();
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
  static esp_err_t handlePostActuatorManual(httpd_req_t *req);
  static esp_err_t handleDownloadReadings(httpd_req_t *req);
  static esp_err_t handleDownloadInference(httpd_req_t *req);

  bool requireLocalAuth(httpd_req_t *req) const;
  esp_err_t readRequestBody(httpd_req_t *req, char *buf, size_t bufSize, size_t maxLen) const;
  bool readFileChunk(const char *path, long offset, char *buf, size_t bufSize,
                     size_t &bytesRead, bool &exists) const;
  esp_err_t streamCsvFile(httpd_req_t *req, const char *path, const char *downloadName) const;

  ConfigManager &m_cfgMgr;
  FileLogger &m_logger;
  NetworkManager &m_netMgr;
  SensorsManager &m_sensorsMgr;
  ActuatorManager &m_actuatorMgr;
  SystemStateStore &m_stateStore;

  httpd_handle_t m_server = nullptr;
};
