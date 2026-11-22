#include "LocalWebServer.hpp"
#include "cJSON.h"
#include "config.hpp"
#include "esp_log.h"
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
  config.max_uri_handlers = 12;
  config.stack_size = 6144;
  config.lru_purge_enable = true;

  esp_err_t err = httpd_start(&m_server, &config);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Falha ao iniciar httpd: %s", esp_err_to_name(err));
    return err;
  }

  const httpd_uri_t routes[] = {
      {"/", HTTP_GET, handleRoot, this},
      {"/api/status", HTTP_GET, handleGetStatus, this},
      {"/api/config", HTTP_GET, handleGetConfig, this},
      {"/api/config", HTTP_POST, handlePostConfig, this},
      {"/api/wifi", HTTP_POST, handlePostWifi, this},
      {"/api/calibrate/dry", HTTP_POST, handleCalibrateDry, this},
      {"/api/calibrate/wet", HTTP_POST, handleCalibrateWet, this},
      {"/logs/readings.csv", HTTP_GET, handleDownloadReadings, this},
      {"/logs/inference.csv", HTTP_GET, handleDownloadInference, this},
  };

  for (const auto &r : routes) {
    httpd_register_uri_handler(m_server, &r);
  }

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
      while ((n = std::fread(buf, 1, sizeof(buf), f)) > 0) {
        if (httpd_resp_send_chunk(req, buf, static_cast<ssize_t>(n)) != ESP_OK) {
          std::fclose(f);
          xSemaphoreGive(self->m_logger.getMutex());
          httpd_resp_send_chunk(req, nullptr, 0);
          return ESP_FAIL;
        }
      }
      std::fclose(f);
      xSemaphoreGive(self->m_logger.getMutex());
      httpd_resp_send_chunk(req, nullptr, 0);
      return ESP_OK;
    }
    xSemaphoreGive(self->m_logger.getMutex());
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
  cJSON_AddBoolToObject(netObj, "apActive", net.apActive);
  cJSON_AddBoolToObject(netObj, "ntpSynced", net.ntpSynced);
  cJSON_AddNumberToObject(netObj, "rssi", net.rssi);
  cJSON_AddStringToObject(netObj, "ssid", net.ssid);
  cJSON_AddStringToObject(netObj, "ip", net.ip);

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
