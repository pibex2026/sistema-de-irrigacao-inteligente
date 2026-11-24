#include "TelemetryClient.hpp"
#include "TimeUtils.hpp"
#include "cJSON.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include <cstdlib>
#include <cstring>

static const char *TAG = "TelemetryClient";

TelemetryClient::TelemetryClient(ConfigManager &cfgMgr, NetworkManager &netMgr,
                                 SystemStateStore &stateStore)
    : m_cfgMgr(cfgMgr), m_netMgr(netMgr), m_stateStore(stateStore) {}

TelemetryClient::~TelemetryClient() {
  stop();
}

esp_err_t TelemetryClient::start() {
  if (m_taskHandle != nullptr) return ESP_OK;

  m_running = true;
  const BaseType_t rc = xTaskCreatePinnedToCore(
      telemetryTask,
      "telemetry_task",
      Config::Tasks::TELEMETRY_STACK,
      this,
      Config::Tasks::TELEMETRY_PRIO,
      &m_taskHandle,
      tskNO_AFFINITY
  );
  if (rc != pdPASS) {
    m_running = false;
    m_taskHandle = nullptr;
    ESP_LOGE(TAG, "Falha ao criar telemetry_task.");
    return ESP_FAIL;
  }

  ESP_LOGI(TAG, "TelemetryClient iniciado (SMP prio %u).", Config::Tasks::TELEMETRY_PRIO);
  return ESP_OK;
}

void TelemetryClient::notifyValveStateChanged() {
  if (m_taskHandle != nullptr) {
    xTaskNotifyGive(m_taskHandle);
  }
}

void TelemetryClient::stop() {
  m_running = false;
  if (m_taskHandle != nullptr) {
    vTaskDelay(pdMS_TO_TICKS(50));
    m_taskHandle = nullptr;
  }
}

bool TelemetryClient::publishSnapshot(const SystemConfig &cfg, const SystemSnapshot &snap,
                                      bool includeValveEvent, bool valveEventOpen,
                                      uint32_t durationSec, bool fallbackTriggered) {
  cJSON *root = cJSON_CreateObject();
  cJSON_AddNumberToObject(root, "temperatura", snap.lastSensors.getTemperatureC());
  cJSON_AddNumberToObject(root, "umidade", snap.lastSensors.humidity);
  cJSON_AddNumberToObject(root, "umidade_solo", snap.lastSensors.soilMoisture);
  cJSON_AddStringToObject(root, "timestamp_embarcado", snap.lastSensors.timestamp);
  cJSON_AddBoolToObject(root, "valvula_aberta", snap.actuator.valveOpen);

  if (includeValveEvent) {
    cJSON *ev = cJSON_AddObjectToObject(root, "evento_irrigacao");
    cJSON_AddStringToObject(ev, "status", valveEventOpen ? "INICIADA" : "FINALIZADA");
    cJSON_AddStringToObject(ev, "origem",
                            fallbackTriggered ? "FALLBACK_THRESHOLD" : "AUTOMATICO_REDE_NEURAL");
    cJSON_AddNumberToObject(ev, "duracao_segundos", durationSec);
  }

  char *payload = cJSON_PrintUnformatted(root);
  cJSON_Delete(root);
  if (payload == nullptr) return false;

  esp_http_client_config_t httpCfg{};
  httpCfg.url = cfg.backendUrl;
  httpCfg.method = HTTP_METHOD_POST;
  httpCfg.timeout_ms = 5000;

  esp_http_client_handle_t client = esp_http_client_init(&httpCfg);
  if (client == nullptr) {
    std::free(payload);
    return false;
  }

  esp_http_client_set_header(client, "Content-Type", "application/json");
  if (cfg.deviceApiKey[0] != '\0') {
    esp_http_client_set_header(client, "X-Device-Key", cfg.deviceApiKey);
  }
  esp_http_client_set_post_field(client, payload, static_cast<int>(std::strlen(payload)));

  const esp_err_t err = esp_http_client_perform(client);
  const int statusCode = esp_http_client_get_status_code(client);
  esp_http_client_cleanup(client);
  std::free(payload);

  if (err != ESP_OK) {
    ESP_LOGW(TAG, "Erro HTTP ao enviar telemetria: %s", esp_err_to_name(err));
    return false;
  }

  if (statusCode < 200 || statusCode >= 300) {
    ESP_LOGW(TAG, "Backend respondeu HTTP %d ao enviar telemetria.", statusCode);
    return false;
  }

  ESP_LOGD(TAG, "Telemetria publicada com sucesso (HTTP %d).", statusCode);
  return true;
}

void TelemetryClient::telemetryTask(void *arg) {
  auto *self = static_cast<TelemetryClient *>(arg);
  int64_t lastPublishMs = 0;
  bool prevValveOpen = false;
  int64_t valveOpenedAtMs = 0;

  while (self->m_running) {
    const SystemConfig cfg = self->m_cfgMgr.get();
    const NetworkStatus net = self->m_netMgr.getStatus();
    const SystemSnapshot snap = self->m_stateStore.getSnapshot();
    const int64_t nowMs = TimeUtils::getMillis();

    const bool valveChanged = (snap.actuator.valveOpen != prevValveOpen);
    if (valveChanged) {
      if (snap.actuator.valveOpen) {
        valveOpenedAtMs = nowMs;
      }
    }

    const bool intervalElapsed = (nowMs - lastPublishMs) >= static_cast<int64_t>(cfg.telemetryIntervalMs);
    if (net.staConnected && cfg.backendUrl[0] != '\0' && snap.lastSensors.valid &&
        (intervalElapsed || valveChanged)) {
      uint32_t durationSec = 0;
      if (valveChanged && !snap.actuator.valveOpen && valveOpenedAtMs > 0) {
        durationSec = static_cast<uint32_t>((nowMs - valveOpenedAtMs) / 1000);
      }

      if (self->publishSnapshot(cfg, snap, valveChanged, snap.actuator.valveOpen,
                                durationSec, snap.lastInference.fallbackTriggered)) {
        lastPublishMs = nowMs;
        prevValveOpen = snap.actuator.valveOpen;
      }
    }

    self->m_stateStore.updateTelemetryMetrics(
        static_cast<uint32_t>(uxTaskGetStackHighWaterMark(nullptr) * sizeof(StackType_t)));
    ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(1000));
  }

  vTaskDelete(nullptr);
}
