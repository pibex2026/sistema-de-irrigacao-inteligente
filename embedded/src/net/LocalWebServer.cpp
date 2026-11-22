#include "LocalWebServer.hpp"
#include "cJSON.h"
#include "config.hpp"
#include "esp_chip_info.h"
#include "esp_clk_tree.h"
#include "esp_flash.h"
#include "esp_heap_caps.h"
#include "esp_idf_version.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_ota_ops.h"
#include "esp_system.h"
#include "esp_wifi_types.h"
#include <cstdio>
#include <cstring>

static const char *TAG = "LocalWebServer";

LocalWebServer::LocalWebServer(ConfigManager &cfgMgr, FileLogger &logger,
                               NetworkManager &netMgr, SensorsManager &sensorsMgr)
    : m_cfgMgr(cfgMgr), m_logger(logger), m_netMgr(netMgr), m_sensorsMgr(sensorsMgr) {
  m_snapMutex = xSemaphoreCreateMutex();
}

LocalWebServer::~LocalWebServer() {
  stop();
  if (m_snapMutex != nullptr) {
    vSemaphoreDelete(m_snapMutex);
    m_snapMutex = nullptr;
  }
}

void LocalWebServer::updateSnapshot(const SystemSnapshot &snap) {
  if (m_snapMutex == nullptr) return;
  xSemaphoreTake(m_snapMutex, portMAX_DELAY);
  m_snapshot = snap;
  xSemaphoreGive(m_snapMutex);
}

SystemSnapshot LocalWebServer::getSnapshot() const {
  SystemSnapshot copy{};
  if (m_snapMutex == nullptr) return copy;
  xSemaphoreTake(m_snapMutex, portMAX_DELAY);
  copy = m_snapshot;
  xSemaphoreGive(m_snapMutex);
  return copy;
}

esp_err_t LocalWebServer::start() {
  if (m_server != nullptr) return ESP_OK;

  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.max_uri_handlers = 22;
  config.max_open_sockets = 11;
  config.stack_size = 6144;
  config.lru_purge_enable = true;

  esp_err_t err = httpd_start(&m_server, &config);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Falha ao iniciar httpd: %s", esp_err_to_name(err));
    return err;
  }

  const httpd_uri_t routes[] = {
      {"/", HTTP_GET, handleRoot, this},
      {"/captive-portal-api", HTTP_GET, handleCaptivePortalApi, this},
      {"/favicon.ico", HTTP_GET, handleFavicon, this},
      {"/generate_204", HTTP_GET, handleCaptiveRedirect, this},
      {"/generate_204", HTTP_HEAD, handleCaptiveRedirect, this},
      {"/gen_204", HTTP_GET, handleCaptiveRedirect, this},
      {"/hotspot-detect.html", HTTP_GET, handleCaptiveRedirect, this},
      {"/connecttest.txt", HTTP_GET, handleCaptiveRedirect, this},
      {"/ncsi.txt", HTTP_GET, handleCaptiveRedirect, this},
      {"/redirect", HTTP_GET, handleCaptiveRedirect, this},
      {"/api/status", HTTP_GET, handleGetStatus, this},
      {"/api/performance", HTTP_GET, handleGetPerformance, this},
      {"/api/system", HTTP_GET, handleGetSystemInfo, this},
      {"/api/config", HTTP_GET, handleGetConfig, this},
      {"/api/config", HTTP_POST, handlePostConfig, this},
      {"/api/wifi/scan", HTTP_GET, handleGetWifiScan, this},
      {"/api/wifi", HTTP_POST, handlePostWifi, this},
      {"/api/calibrate/dry", HTTP_POST, handleCalibrateDry, this},
      {"/api/calibrate/wet", HTTP_POST, handleCalibrateWet, this},
      {"/logs/readings.csv", HTTP_GET, handleDownloadReadings, this},
      {"/logs/inference.csv", HTTP_GET, handleDownloadInference, this},
  };

  for (const auto &r : routes) {
    const esp_err_t regErr = httpd_register_uri_handler(m_server, &r);
    if (regErr != ESP_OK) {
      ESP_LOGE(TAG, "Falha ao registrar rota %s: %s", r.uri, esp_err_to_name(regErr));
    }
  }
  httpd_register_err_handler(m_server, HTTPD_404_NOT_FOUND, handle404CaptiveRedirect);

  ESP_LOGI(TAG, "Servidor HTTP local iniciado na porta %d.", config.server_port);
  return ESP_OK;
}

void LocalWebServer::stop() {
  if (m_server != nullptr) {
    httpd_stop(m_server);
    m_server = nullptr;
  }
}

esp_err_t LocalWebServer::handleRoot(httpd_req_t *req) {
  ESP_LOGI(TAG, "HTTP GET %s (handleRoot)", req->uri);
  auto *self = static_cast<LocalWebServer *>(req->user_ctx);
  FILE *f = nullptr;
  if (self->m_logger.isMounted()) {
    xSemaphoreTake(self->m_logger.getMutex(), portMAX_DELAY);
    f = std::fopen(Config::Storage::WEB_INDEX_GZ_PATH, "rb");
    if (f != nullptr) {
      httpd_resp_set_type(req, "text/html");
      httpd_resp_set_hdr(req, "Content-Encoding", "gzip");
      char buf[1024];
      size_t n = 0;
      size_t totalSent = 0;
      while ((n = std::fread(buf, 1, sizeof(buf), f)) > 0) {
        if (httpd_resp_send_chunk(req, buf, static_cast<ssize_t>(n)) != ESP_OK) {
          ESP_LOGW(TAG, "Erro ao enviar chunk de index.html.gz (enviados %zu B)", totalSent);
          std::fclose(f);
          xSemaphoreGive(self->m_logger.getMutex());
          httpd_resp_send_chunk(req, nullptr, 0);
          return ESP_FAIL;
        }
        totalSent += n;
      }
      std::fclose(f);
      xSemaphoreGive(self->m_logger.getMutex());
      httpd_resp_send_chunk(req, nullptr, 0);
      ESP_LOGI(TAG, "index.html.gz enviado (%zu bytes).", totalSent);
      return ESP_OK;
    }
    xSemaphoreGive(self->m_logger.getMutex());
    ESP_LOGW(TAG, "index.html.gz nao encontrado em %s; usando fallback.", Config::Storage::WEB_INDEX_GZ_PATH);
  }

  httpd_resp_set_type(req, "text/html");
  const char *fallback =
      "<html><body><h2>Irrigacao Inteligente ESP32 (ESP-IDF)</h2>"
      "<p>Consulte <a href='/api/status'>/api/status</a>, "
      "<a href='/api/config'>/api/config</a>, "
      "<a href='/logs/readings.csv'>readings.csv</a> ou "
      "<a href='/logs/inference.csv'>inference.csv</a>.</p></body></html>";
  return httpd_resp_sendstr(req, fallback);
}

esp_err_t LocalWebServer::handleCaptivePortalApi(httpd_req_t *req) {
  ESP_LOGI(TAG, "Captive Portal API (RFC 8910) consultada.");
  httpd_resp_set_type(req, "application/captive+json");
  httpd_resp_set_hdr(req, "Cache-Control", "no-store, no-cache, must-revalidate");
  httpd_resp_set_hdr(req, "Connection", "close");
  return httpd_resp_sendstr(
      req, "{\"captive\":true,\"user-portal-url\":\"http://192.168.4.1/\"}");
}

esp_err_t LocalWebServer::handleFavicon(httpd_req_t *req) {
  httpd_resp_set_status(req, "204 No Content");
  return httpd_resp_send(req, nullptr, 0);
}

esp_err_t LocalWebServer::handleCaptiveRedirect(httpd_req_t *req) {
  ESP_LOGI(TAG, "Captive redirect: %s -> http://192.168.4.1/", req->uri);
  httpd_resp_set_status(req, "302 Found");
  httpd_resp_set_type(req, "text/html");
  httpd_resp_set_hdr(req, "Location", "http://192.168.4.1/");
  httpd_resp_set_hdr(req, "Cache-Control", "no-store, no-cache, must-revalidate");
  httpd_resp_set_hdr(req, "Connection", "close");
  if (req->method == HTTP_HEAD) {
    return httpd_resp_send(req, nullptr, 0);
  }
  const char *body =
      "<!DOCTYPE html><html><head><meta http-equiv=\"refresh\" content=\"0;url=http://192.168.4.1/\">"
      "</head><body><a href=\"http://192.168.4.1/\">Abrir Portal ESP-Irrigacao</a></body></html>";
  return httpd_resp_sendstr(req, body);
}

esp_err_t LocalWebServer::handle404CaptiveRedirect(httpd_req_t *req, httpd_err_code_t) {
  ESP_LOGI(TAG, "HTTP 404 URI: %s", req->uri);
  if (std::strncmp(req->uri, "/api/", 5) == 0 || std::strncmp(req->uri, "/logs/", 6) == 0) {
    httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Not Found");
    return ESP_FAIL;
  }
  return handleCaptiveRedirect(req);
}

esp_err_t LocalWebServer::handleGetStatus(httpd_req_t *req) {
  auto *self = static_cast<LocalWebServer *>(req->user_ctx);
  const SystemSnapshot snap = self->getSnapshot();
  const NetworkStatus net = self->m_netMgr.getStatus();

  cJSON *root = cJSON_CreateObject();
  cJSON_AddNumberToObject(root, "state", static_cast<int>(snap.state));
  cJSON_AddNumberToObject(root, "uptimeSec", snap.uptimeSec);
  cJSON_AddNumberToObject(root, "freeHeapBytes", snap.freeHeapBytes);

  cJSON *sens = cJSON_AddObjectToObject(root, "sensors");
  cJSON_AddNumberToObject(sens, "temperatureC", snap.lastSensors.getTemperatureC());
  cJSON_AddNumberToObject(sens, "humidityPct", snap.lastSensors.humidity);
  cJSON_AddNumberToObject(sens, "soilMoisturePct", snap.lastSensors.soilMoisture);
  cJSON_AddNumberToObject(sens, "rawSoilAdc", snap.lastSensors.rawSoilAdc);
  cJSON_AddBoolToObject(sens, "valid", snap.lastSensors.valid);
  cJSON_AddStringToObject(sens, "timestamp", snap.lastSensors.timestamp);

  cJSON *inf = cJSON_AddObjectToObject(root, "inference");
  cJSON_AddNumberToObject(inf, "decision", static_cast<int>(snap.lastInference.decision));
  cJSON_AddBoolToObject(inf, "shouldIrrigate", snap.lastInference.shouldIrrigate);
  cJSON_AddBoolToObject(inf, "thermalStressAlert", snap.lastInference.thermalStressAlert);
  cJSON_AddBoolToObject(inf, "fallbackTriggered", snap.lastInference.fallbackTriggered);
  cJSON_AddBoolToObject(inf, "valid", snap.lastInference.valid);
  cJSON_AddNumberToObject(inf, "latencyUs", snap.lastInference.latencyUs);
  cJSON *probs = cJSON_AddArrayToObject(inf, "probabilities");
  for (int i = 0; i < 3; ++i) {
    cJSON_AddItemToArray(probs, cJSON_CreateNumber(snap.lastInference.probabilities[i]));
  }

  cJSON *act = cJSON_AddObjectToObject(root, "actuator");
  cJSON_AddBoolToObject(act, "valveOpen", snap.actuator.valveOpen);
  cJSON_AddBoolToObject(act, "inCooldown", snap.actuator.inCooldown);
  cJSON_AddBoolToObject(act, "emergencyStopped", snap.actuator.emergencyStopped);
  cJSON_AddNumberToObject(act, "openElapsedMs", snap.actuator.openElapsedMs);

  cJSON *netObj = cJSON_AddObjectToObject(root, "network");
  cJSON_AddBoolToObject(netObj, "staConnected", net.staConnected);
  cJSON_AddBoolToObject(netObj, "staConnecting", net.staConnecting);
  cJSON_AddBoolToObject(netObj, "apActive", net.apActive);
  cJSON_AddBoolToObject(netObj, "ntpSynced", net.ntpSynced);
  cJSON_AddNumberToObject(netObj, "rssi", net.rssi);
  cJSON_AddStringToObject(netObj, "ssid", net.ssid);
  cJSON_AddStringToObject(netObj, "ip", net.ip);
  cJSON_AddStringToObject(netObj, "lastError", net.lastError);

  char *jsonStr = cJSON_PrintUnformatted(root);
  cJSON_Delete(root);
  httpd_resp_set_type(req, "application/json");
  const esp_err_t res = httpd_resp_sendstr(req, jsonStr ? jsonStr : "{}");
  std::free(jsonStr);
  return res;
}

esp_err_t LocalWebServer::handleGetPerformance(httpd_req_t *req) {
  auto *self = static_cast<LocalWebServer *>(req->user_ctx);
  const SystemSnapshot snap = self->getSnapshot();
  const auto recentInf = self->m_logger.readRecentInferences(15);

  cJSON *root = cJSON_CreateObject();
  cJSON *p = cJSON_AddObjectToObject(root, "stats");
  cJSON_AddNumberToObject(p, "uptimeSec", snap.uptimeSec);
  cJSON_AddNumberToObject(p, "totalSensorReads", snap.performance.totalSensorReads);
  cJSON_AddNumberToObject(p, "sensorErrorCount", snap.performance.sensorErrorCount);
  cJSON_AddNumberToObject(p, "totalInferences", snap.performance.totalInferences);
  cJSON_AddNumberToObject(p, "countOff", snap.performance.countOff);
  cJSON_AddNumberToObject(p, "countIrrigate", snap.performance.countIrrigate);
  cJSON_AddNumberToObject(p, "countThermalStress", snap.performance.countThermalStress);
  cJSON_AddNumberToObject(p, "valveActivations", snap.performance.valveActivations);
  cJSON_AddNumberToObject(p, "lastLatencyUs", snap.performance.lastLatencyUs);
  cJSON_AddNumberToObject(p, "minLatencyUs", snap.performance.minLatencyUs);
  cJSON_AddNumberToObject(p, "maxLatencyUs", snap.performance.maxLatencyUs);
  cJSON_AddNumberToObject(p, "avgLatencyUs", snap.performance.avgLatencyUs);
  cJSON_AddNumberToObject(p, "tfliteArenaUsedBytes", snap.performance.tfliteArenaUsedBytes);
  cJSON_AddNumberToObject(p, "controlTaskStackFreeBytes", snap.performance.controlTaskStackFreeBytes);

  cJSON *history = cJSON_AddArrayToObject(root, "recentInferences");
  for (const auto &e : recentInf) {
    cJSON *item = cJSON_CreateObject();
    cJSON_AddNumberToObject(item, "tempC", e.sensors.getTemperatureC());
    cJSON_AddNumberToObject(item, "humPct", e.sensors.humidity);
    cJSON_AddNumberToObject(item, "soilPct", e.sensors.soilMoisture);
    cJSON_AddNumberToObject(item, "decision", static_cast<int>(e.inference.decision));
    cJSON_AddBoolToObject(item, "valveOpen", e.valveOpen);
    cJSON_AddBoolToObject(item, "fallback", e.inference.fallbackTriggered);
    cJSON_AddNumberToObject(item, "p0", e.inference.probabilities[0]);
    cJSON_AddNumberToObject(item, "p1", e.inference.probabilities[1]);
    cJSON_AddNumberToObject(item, "p2", e.inference.probabilities[2]);
    cJSON_AddStringToObject(item, "timestamp", e.inference.timestamp);
    cJSON_AddItemToArray(history, item);
  }

  char *jsonStr = cJSON_PrintUnformatted(root);
  cJSON_Delete(root);
  httpd_resp_set_type(req, "application/json");
  const esp_err_t res = httpd_resp_sendstr(req, jsonStr ? jsonStr : "{}");
  std::free(jsonStr);
  return res;
}

static const char *resetReasonToStr(esp_reset_reason_t r) {
  switch (r) {
  case ESP_RST_POWERON: return "POWERON";
  case ESP_RST_SW: return "SW_RESET";
  case ESP_RST_PANIC: return "PANIC";
  case ESP_RST_INT_WDT: return "INT_WDT";
  case ESP_RST_TASK_WDT: return "TASK_WDT";
  case ESP_RST_WDT: return "WDT";
  case ESP_RST_DEEPSLEEP: return "DEEPSLEEP";
  case ESP_RST_BROWNOUT: return "BROWNOUT";
  default: return "OTHER";
  }
}

esp_err_t LocalWebServer::handleGetSystemInfo(httpd_req_t *req) {
  auto *self = static_cast<LocalWebServer *>(req->user_ctx);
  const SystemSnapshot snap = self->getSnapshot();
  const StorageStats st = self->m_logger.getStorageStats();

  esp_chip_info_t chipInfo{};
  esp_chip_info(&chipInfo);

  uint8_t mac[6] = {};
  esp_read_mac(mac, ESP_MAC_WIFI_STA);
  char macStr[18];
  std::snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X:%02X:%02X:%02X",
                mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

  uint32_t cpuHz = 0, xtalHz = 0;
  esp_clk_tree_src_get_freq_hz(SOC_MOD_CLK_CPU, ESP_CLK_TREE_SRC_FREQ_PRECISION_CACHED, &cpuHz);
  esp_clk_tree_src_get_freq_hz(SOC_MOD_CLK_XTAL, ESP_CLK_TREE_SRC_FREQ_PRECISION_CACHED, &xtalHz);

  uint32_t flashSize = 0;
  esp_flash_get_size(nullptr, &flashSize);
  const esp_partition_t *runningPart = esp_ota_get_running_partition();

  const uint32_t totalHeap = static_cast<uint32_t>(heap_caps_get_total_size(MALLOC_CAP_8BIT));
  const uint32_t freeHeap = esp_get_free_heap_size();
  const uint32_t minFreeHeap = esp_get_minimum_free_heap_size();
  const uint32_t largestBlock = static_cast<uint32_t>(heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));

  cJSON *root = cJSON_CreateObject();

  cJSON *board = cJSON_AddObjectToObject(root, "board");
  cJSON_AddStringToObject(board, "model", "ESP32 (Xtensa Dual-Core LX6)");
  cJSON_AddNumberToObject(board, "cores", chipInfo.cores);
  cJSON_AddNumberToObject(board, "revision", chipInfo.revision);
  cJSON_AddStringToObject(board, "mac", macStr);
  cJSON_AddStringToObject(board, "idfVersion", esp_get_idf_version());
  cJSON_AddStringToObject(board, "resetReason", resetReasonToStr(esp_reset_reason()));
  cJSON_AddNumberToObject(board, "uptimeSec", snap.uptimeSec);

  cJSON *clk = cJSON_AddObjectToObject(root, "clock");
  cJSON_AddNumberToObject(clk, "cpuFreqMhz", cpuHz / 1000000.0);
  cJSON_AddNumberToObject(clk, "xtalFreqMhz", xtalHz / 1000000.0);

  cJSON *temp = cJSON_AddObjectToObject(root, "temperature");
  cJSON_AddBoolToObject(temp, "internalSupported", false);
  cJSON_AddStringToObject(temp, "note", "Sensor interno nao suportado no ESP32 classico (IDF v5.5)");
  cJSON_AddNumberToObject(temp, "ambientDht22C", snap.lastSensors.valid ? snap.lastSensors.getTemperatureC() : 0.0);
  cJSON_AddBoolToObject(temp, "ambientValid", snap.lastSensors.valid);

  cJSON *mem = cJSON_AddObjectToObject(root, "memory");
  cJSON_AddNumberToObject(mem, "totalHeapBytes", totalHeap);
  cJSON_AddNumberToObject(mem, "freeHeapBytes", freeHeap);
  cJSON_AddNumberToObject(mem, "usedHeapBytes", totalHeap > freeHeap ? (totalHeap - freeHeap) : 0);
  cJSON_AddNumberToObject(mem, "minFreeHeapBytes", minFreeHeap);
  cJSON_AddNumberToObject(mem, "largestFreeBlockBytes", largestBlock);
  cJSON_AddNumberToObject(mem, "tfliteArenaUsedBytes", snap.performance.tfliteArenaUsedBytes);
  cJSON_AddNumberToObject(mem, "controlTaskStackFreeBytes", snap.performance.controlTaskStackFreeBytes);

  cJSON *stor = cJSON_AddObjectToObject(root, "storage");
  cJSON_AddNumberToObject(stor, "flashSizeBytes", flashSize);
  cJSON_AddStringToObject(stor, "runningPartition", runningPart ? runningPart->label : "unknown");
  cJSON_AddBoolToObject(stor, "littlefsMounted", st.mounted);
  cJSON_AddNumberToObject(stor, "littlefsTotalBytes", st.totalBytes);
  cJSON_AddNumberToObject(stor, "littlefsUsedBytes", st.usedBytes);
  cJSON_AddNumberToObject(stor, "readingsCsvBytes", st.readingsBytes);
  cJSON_AddNumberToObject(stor, "readingsPrevBytes", st.readingsPrevBytes);
  cJSON_AddNumberToObject(stor, "inferenceCsvBytes", st.inferenceBytes);
  cJSON_AddNumberToObject(stor, "inferencePrevBytes", st.inferencePrevBytes);

  char *jsonStr = cJSON_PrintUnformatted(root);
  cJSON_Delete(root);
  httpd_resp_set_type(req, "application/json");
  const esp_err_t res = httpd_resp_sendstr(req, jsonStr ? jsonStr : "{}");
  std::free(jsonStr);
  return res;
}

esp_err_t LocalWebServer::handleGetConfig(httpd_req_t *req) {
  auto *self = static_cast<LocalWebServer *>(req->user_ctx);
  const SystemConfig cfg = self->m_cfgMgr.get();

  cJSON *root = cJSON_CreateObject();
  cJSON_AddNumberToObject(root, "soilType", cfg.soilType);
  cJSON_AddNumberToObject(root, "cropId", cfg.cropId);
  cJSON_AddNumberToObject(root, "seedlingStage", cfg.seedlingStage);
  cJSON_AddNumberToObject(root, "confidenceThreshold", cfg.confidenceThreshold);
  cJSON_AddNumberToObject(root, "thermalStressThreshold", cfg.thermalStressThreshold);
  cJSON_AddNumberToObject(root, "soilAdcDry", cfg.soilAdcDry);
  cJSON_AddNumberToObject(root, "soilAdcWet", cfg.soilAdcWet);
  cJSON_AddNumberToObject(root, "maxIrrigationMs", cfg.maxIrrigationMs);
  cJSON_AddNumberToObject(root, "cooldownMs", cfg.cooldownMs);
  cJSON_AddNumberToObject(root, "sensorReadIntervalMs", cfg.sensorReadIntervalMs);
  cJSON_AddNumberToObject(root, "logIntervalMs", cfg.logIntervalMs);

  char *jsonStr = cJSON_PrintUnformatted(root);
  cJSON_Delete(root);
  httpd_resp_set_type(req, "application/json");
  const esp_err_t res = httpd_resp_sendstr(req, jsonStr ? jsonStr : "{}");
  std::free(jsonStr);
  return res;
}

esp_err_t LocalWebServer::handlePostConfig(httpd_req_t *req) {
  auto *self = static_cast<LocalWebServer *>(req->user_ctx);
  if (req->content_len <= 0 || req->content_len > 1024) {
    httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Payload invalido");
    return ESP_FAIL;
  }

  char buf[1025] = {};
  const int received = httpd_req_recv(req, buf, req->content_len);
  if (received <= 0) {
    httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Erro ao ler body");
    return ESP_FAIL;
  }

  cJSON *root = cJSON_Parse(buf);
  if (root == nullptr) {
    httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "JSON invalido");
    return ESP_FAIL;
  }

  SystemConfig cfg = self->m_cfgMgr.get();
  if (cJSON *v = cJSON_GetObjectItem(root, "soilType"); cJSON_IsNumber(v)) cfg.soilType = static_cast<uint8_t>(v->valueint);
  if (cJSON *v = cJSON_GetObjectItem(root, "cropId"); cJSON_IsNumber(v)) cfg.cropId = static_cast<uint8_t>(v->valueint);
  if (cJSON *v = cJSON_GetObjectItem(root, "seedlingStage"); cJSON_IsNumber(v)) cfg.seedlingStage = static_cast<uint8_t>(v->valueint);
  if (cJSON *v = cJSON_GetObjectItem(root, "confidenceThreshold"); cJSON_IsNumber(v)) cfg.confidenceThreshold = static_cast<float>(v->valuedouble);
  if (cJSON *v = cJSON_GetObjectItem(root, "thermalStressThreshold"); cJSON_IsNumber(v)) cfg.thermalStressThreshold = static_cast<float>(v->valuedouble);
  if (cJSON *v = cJSON_GetObjectItem(root, "soilAdcDry"); cJSON_IsNumber(v)) cfg.soilAdcDry = static_cast<uint16_t>(v->valueint);
  if (cJSON *v = cJSON_GetObjectItem(root, "soilAdcWet"); cJSON_IsNumber(v)) cfg.soilAdcWet = static_cast<uint16_t>(v->valueint);
  if (cJSON *v = cJSON_GetObjectItem(root, "maxIrrigationMs"); cJSON_IsNumber(v)) cfg.maxIrrigationMs = static_cast<uint32_t>(v->valuedouble);
  if (cJSON *v = cJSON_GetObjectItem(root, "cooldownMs"); cJSON_IsNumber(v)) cfg.cooldownMs = static_cast<uint32_t>(v->valuedouble);
  if (cJSON *v = cJSON_GetObjectItem(root, "sensorReadIntervalMs"); cJSON_IsNumber(v)) cfg.sensorReadIntervalMs = static_cast<uint32_t>(v->valuedouble);
  if (cJSON *v = cJSON_GetObjectItem(root, "logIntervalMs"); cJSON_IsNumber(v)) cfg.logIntervalMs = static_cast<uint32_t>(v->valuedouble);

  cJSON_Delete(root);
  self->m_cfgMgr.update(cfg);

  httpd_resp_set_type(req, "application/json");
  return httpd_resp_sendstr(req, "{\"ok\":true}");
}

static const char *authModeToStr(uint8_t authmode) {
  switch (static_cast<wifi_auth_mode_t>(authmode)) {
  case WIFI_AUTH_OPEN: return "Aberta";
  case WIFI_AUTH_WEP: return "WEP";
  case WIFI_AUTH_WPA_PSK: return "WPA";
  case WIFI_AUTH_WPA2_PSK: return "WPA2";
  case WIFI_AUTH_WPA_WPA2_PSK: return "WPA/WPA2";
  case WIFI_AUTH_WPA3_PSK: return "WPA3";
  case WIFI_AUTH_WPA2_WPA3_PSK: return "WPA2/WPA3";
  case WIFI_AUTH_WPA2_ENTERPRISE: return "WPA2-Ent";
  default: return "Protegida";
  }
}

esp_err_t LocalWebServer::handleGetWifiScan(httpd_req_t *req) {
  auto *self = static_cast<LocalWebServer *>(req->user_ctx);
  const auto nets = self->m_netMgr.scanNetworks(15);

  cJSON *root = cJSON_CreateObject();
  cJSON_AddBoolToObject(root, "ok", true);
  cJSON_AddNumberToObject(root, "count", static_cast<double>(nets.size()));
  cJSON *arr = cJSON_AddArrayToObject(root, "networks");
  for (const auto &n : nets) {
    cJSON *item = cJSON_CreateObject();
    cJSON_AddStringToObject(item, "ssid", n.ssid);
    cJSON_AddNumberToObject(item, "rssi", n.rssi);
    cJSON_AddNumberToObject(item, "channel", n.channel);
    cJSON_AddStringToObject(item, "auth", authModeToStr(n.authmode));
    cJSON_AddBoolToObject(item, "open", n.authmode == WIFI_AUTH_OPEN);
    cJSON_AddItemToArray(arr, item);
  }

  char *jsonStr = cJSON_PrintUnformatted(root);
  cJSON_Delete(root);
  httpd_resp_set_type(req, "application/json");
  httpd_resp_set_hdr(req, "Cache-Control", "no-store");
  const esp_err_t res = httpd_resp_sendstr(req, jsonStr ? jsonStr : "{\"ok\":false,\"networks\":[]}");
  std::free(jsonStr);
  return res;
}

esp_err_t LocalWebServer::handlePostWifi(httpd_req_t *req) {
  auto *self = static_cast<LocalWebServer *>(req->user_ctx);
  if (req->content_len <= 0 || req->content_len > 256) {
    httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Payload invalido");
    return ESP_FAIL;
  }

  char buf[257] = {};
  if (httpd_req_recv(req, buf, req->content_len) <= 0) {
    httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Erro ao ler body");
    return ESP_FAIL;
  }

  cJSON *root = cJSON_Parse(buf);
  if (root == nullptr) {
    httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "JSON invalido");
    return ESP_FAIL;
  }

  cJSON *ssidItem = cJSON_GetObjectItem(root, "ssid");
  cJSON *passItem = cJSON_GetObjectItem(root, "password");
  if (!cJSON_IsString(ssidItem) || std::strlen(ssidItem->valuestring) == 0) {
    cJSON_Delete(root);
    httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "SSID obrigatorio");
    return ESP_FAIL;
  }

  const char *pass = cJSON_IsString(passItem) ? passItem->valuestring : "";
  const esp_err_t err = self->m_netMgr.setStaCredentials(ssidItem->valuestring, pass);
  cJSON_Delete(root);

  httpd_resp_set_type(req, "application/json");
  if (err != ESP_OK) {
    return httpd_resp_sendstr(req, "{\"ok\":false}");
  }
  return httpd_resp_sendstr(req, "{\"ok\":true}");
}

esp_err_t LocalWebServer::handleCalibrateDry(httpd_req_t *req) {
  auto *self = static_cast<LocalWebServer *>(req->user_ctx);
  const uint16_t raw = self->m_sensorsMgr.readSoilAdcMedian();
  SystemConfig cfg = self->m_cfgMgr.get();
  cfg.soilAdcDry = raw;
  self->m_cfgMgr.update(cfg);

  char resp[64];
  std::snprintf(resp, sizeof(resp), "{\"ok\":true,\"soilAdcDry\":%u}", raw);
  httpd_resp_set_type(req, "application/json");
  return httpd_resp_sendstr(req, resp);
}

esp_err_t LocalWebServer::handleCalibrateWet(httpd_req_t *req) {
  auto *self = static_cast<LocalWebServer *>(req->user_ctx);
  const uint16_t raw = self->m_sensorsMgr.readSoilAdcMedian();
  SystemConfig cfg = self->m_cfgMgr.get();
  cfg.soilAdcWet = raw;
  self->m_cfgMgr.update(cfg);

  char resp[64];
  std::snprintf(resp, sizeof(resp), "{\"ok\":true,\"soilAdcWet\":%u}", raw);
  httpd_resp_set_type(req, "application/json");
  return httpd_resp_sendstr(req, resp);
}

esp_err_t LocalWebServer::streamCsvFile(httpd_req_t *req, const char *path, const char *downloadName) const {
  if (!m_logger.isMounted()) {
    httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "LittleFS nao montado");
    return ESP_FAIL;
  }

  xSemaphoreTake(m_logger.getMutex(), portMAX_DELAY);
  FILE *f = std::fopen(path, "r");
  if (f == nullptr) {
    xSemaphoreGive(m_logger.getMutex());
    httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Arquivo nao encontrado");
    return ESP_FAIL;
  }

  char disp[80];
  std::snprintf(disp, sizeof(disp), "attachment; filename=\"%s\"", downloadName);
  httpd_resp_set_type(req, "text/csv");
  httpd_resp_set_hdr(req, "Content-Disposition", disp);

  char buf[1024];
  size_t n = 0;
  while ((n = std::fread(buf, 1, sizeof(buf), f)) > 0) {
    if (httpd_resp_send_chunk(req, buf, static_cast<ssize_t>(n)) != ESP_OK) {
      std::fclose(f);
      xSemaphoreGive(m_logger.getMutex());
      httpd_resp_send_chunk(req, nullptr, 0);
      return ESP_FAIL;
    }
  }
  std::fclose(f);
  xSemaphoreGive(m_logger.getMutex());
  httpd_resp_send_chunk(req, nullptr, 0);
  return ESP_OK;
}

esp_err_t LocalWebServer::handleDownloadReadings(httpd_req_t *req) {
  auto *self = static_cast<LocalWebServer *>(req->user_ctx);
  return self->streamCsvFile(req, Config::Storage::READINGS_CSV_PATH, "readings.csv");
}

esp_err_t LocalWebServer::handleDownloadInference(httpd_req_t *req) {
  auto *self = static_cast<LocalWebServer *>(req->user_ctx);
  return self->streamCsvFile(req, Config::Storage::INFERENCE_CSV_PATH, "inference.csv");
}
