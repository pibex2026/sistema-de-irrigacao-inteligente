#include "NetworkManager.hpp"
#include "TimeUtils.hpp"
#include "config.hpp"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_sntp.h"
#include "esp_wifi.h"
#include "mdns.h"
#include <algorithm>
#include <cstdlib>
#include <cstring>

static const char *TAG = "NetworkManager";

NetworkManager::NetworkManager() {
  m_mutex = xSemaphoreCreateMutex();
}

NetworkManager::~NetworkManager() {
  if (m_reconnectTimer != nullptr) {
    esp_timer_stop(m_reconnectTimer);
    esp_timer_delete(m_reconnectTimer);
  }
  if (m_apTimeoutTimer != nullptr) {
    esp_timer_stop(m_apTimeoutTimer);
    esp_timer_delete(m_apTimeoutTimer);
  }
  if (m_mutex != nullptr) {
    vSemaphoreDelete(m_mutex);
  }
}

esp_err_t NetworkManager::init() {
  esp_err_t err = esp_netif_init();
  if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) return err;

  err = esp_event_loop_create_default();
  if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) return err;

  esp_netif_create_default_wifi_sta();
  esp_netif_create_default_wifi_ap();

  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init(&cfg));

  ESP_ERROR_CHECK(esp_event_handler_instance_register(
      WIFI_EVENT, ESP_EVENT_ANY_ID, &NetworkManager::wifiEventHandler, this, nullptr));
  ESP_ERROR_CHECK(esp_event_handler_instance_register(
      IP_EVENT, IP_EVENT_STA_GOT_IP, &NetworkManager::ipEventHandler, this, nullptr));

  esp_timer_create_args_t recArgs{};
  recArgs.callback = &NetworkManager::reconnectTimerCallback;
  recArgs.arg = this;
  recArgs.name = "wifi_reconn";
  ESP_ERROR_CHECK(esp_timer_create(&recArgs, &m_reconnectTimer));

  esp_timer_create_args_t apArgs{};
  apArgs.callback = &NetworkManager::apTimeoutCallback;
  apArgs.arg = this;
  apArgs.name = "ap_timeout";
  ESP_ERROR_CHECK(esp_timer_create(&apArgs, &m_apTimeoutTimer));

  ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_FLASH));

  wifi_config_t savedSta{};
  esp_wifi_get_config(WIFI_IF_STA, &savedSta);
  const bool hasSavedSsid = (std::strlen(reinterpret_cast<const char *>(savedSta.sta.ssid)) > 0);

  if (hasSavedSsid) {
    ESP_LOGI(TAG, "Credenciais Wi-Fi encontradas na NVS (SSID=%s). Conectando STA...",
             reinterpret_cast<const char *>(savedSta.sta.ssid));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());
  } else {
    ESP_LOGW(TAG, "Nenhuma credencial Wi-Fi salva. Iniciando SoftAP '%s' (modo offline/provisionamento).",
             Config::Network::DEFAULT_AP_SSID);
    startSoftApFallback();
  }

  return ESP_OK;
}

void NetworkManager::startSoftApFallback() {
  wifi_config_t apCfg{};
  std::strncpy(reinterpret_cast<char *>(apCfg.ap.ssid), Config::Network::DEFAULT_AP_SSID,
               sizeof(apCfg.ap.ssid) - 1);
  apCfg.ap.ssid_len = static_cast<uint8_t>(std::strlen(Config::Network::DEFAULT_AP_SSID));
  apCfg.ap.channel = 1;
  apCfg.ap.max_connection = 4;

  const size_t passLen = std::strlen(Config::Network::DEFAULT_AP_PASS);
  if (passLen >= 8) {
    std::strncpy(reinterpret_cast<char *>(apCfg.ap.password), Config::Network::DEFAULT_AP_PASS,
                 sizeof(apCfg.ap.password) - 1);
    apCfg.ap.authmode = WIFI_AUTH_WPA2_PSK;
  } else {
    apCfg.ap.authmode = WIFI_AUTH_OPEN;
  }

  wifi_mode_t curMode = WIFI_MODE_NULL;
  esp_wifi_get_mode(&curMode);
  const wifi_mode_t nextMode = (curMode == WIFI_MODE_STA) ? WIFI_MODE_APSTA : WIFI_MODE_AP;

  esp_wifi_set_mode(nextMode);
  esp_wifi_set_config(WIFI_IF_AP, &apCfg);
  if (curMode == WIFI_MODE_NULL) {
    esp_wifi_start();
  }

  xSemaphoreTake(m_mutex, portMAX_DELAY);
  m_status.apActive = true;
  xSemaphoreGive(m_mutex);

  esp_timer_stop(m_apTimeoutTimer);
  esp_timer_start_once(m_apTimeoutTimer,
                       static_cast<uint64_t>(Config::Network::PORTAL_TIMEOUT_S) * 1000000ULL);
  ESP_LOGI(TAG, "SoftAP ativo em 192.168.4.1 (timeout=%lus).",
           static_cast<unsigned long>(Config::Network::PORTAL_TIMEOUT_S));
}

void NetworkManager::stopSoftAp() {
  wifi_mode_t curMode = WIFI_MODE_NULL;
  if (esp_wifi_get_mode(&curMode) != ESP_OK) return;

  if (curMode == WIFI_MODE_APSTA) {
    esp_wifi_set_mode(WIFI_MODE_STA);
  } else if (curMode == WIFI_MODE_AP) {
    // Mantem STA em espera caso nao haja credenciais, desligando radio AP para economia
    esp_wifi_set_mode(WIFI_MODE_STA);
  }
  xSemaphoreTake(m_mutex, portMAX_DELAY);
  m_status.apActive = false;
  xSemaphoreGive(m_mutex);
  ESP_LOGI(TAG, "SoftAP encerrado.");
}

esp_err_t NetworkManager::setStaCredentials(const char *ssid, const char *password) {
  if (ssid == nullptr || std::strlen(ssid) == 0) return ESP_ERR_INVALID_ARG;

  wifi_config_t staCfg{};
  std::strncpy(reinterpret_cast<char *>(staCfg.sta.ssid), ssid, sizeof(staCfg.sta.ssid) - 1);
  if (password != nullptr) {
    std::strncpy(reinterpret_cast<char *>(staCfg.sta.password), password, sizeof(staCfg.sta.password) - 1);
  }

  wifi_mode_t curMode = WIFI_MODE_NULL;
  esp_wifi_get_mode(&curMode);
  if (curMode == WIFI_MODE_AP || curMode == WIFI_MODE_APSTA) {
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
  } else {
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
  }

  ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &staCfg));
  m_retryCount = 0;
  m_backoffMs = 10000;
  esp_wifi_disconnect();
  return esp_wifi_connect();
}

void NetworkManager::triggerReconnect() {
  esp_wifi_connect();
}

void NetworkManager::initSntpOnce() {
  if (m_sntpStarted) return;
  setenv("TZ", Config::Network::TIMEZONE_POSIX, 1);
  tzset();

  esp_sntp_setoperatingmode(ESP_SNTP_OPMODE_POLL);
  esp_sntp_setservername(0, Config::Network::NTP_SERVER_PRIMARY);
  esp_sntp_setservername(1, Config::Network::NTP_SERVER_SECONDARY);
  esp_sntp_init();
  m_sntpStarted = true;
  ESP_LOGI(TAG, "SNTP iniciado (%s, TZ=%s).",
           Config::Network::NTP_SERVER_PRIMARY, Config::Network::TIMEZONE_POSIX);
}

void NetworkManager::wifiEventHandler(void *arg, esp_event_base_t, int32_t eventId, void *eventData) {
  static_cast<NetworkManager *>(arg)->onWifiEvent(eventId, eventData);
}

void NetworkManager::ipEventHandler(void *arg, esp_event_base_t, int32_t eventId, void *eventData) {
  static_cast<NetworkManager *>(arg)->onIpEvent(eventId, eventData);
}

void NetworkManager::reconnectTimerCallback(void *arg) {
  auto *self = static_cast<NetworkManager *>(arg);
  ESP_LOGI(TAG, "Tentativa assincrona de reconexao Wi-Fi...");
  esp_wifi_connect();
}

void NetworkManager::apTimeoutCallback(void *arg) {
  static_cast<NetworkManager *>(arg)->stopSoftAp();
}

void NetworkManager::onWifiEvent(int32_t eventId, void *) {
  if (eventId == WIFI_EVENT_STA_START) {
    wifi_config_t savedSta{};
    esp_wifi_get_config(WIFI_IF_STA, &savedSta);
    if (std::strlen(reinterpret_cast<const char *>(savedSta.sta.ssid)) > 0) {
      esp_wifi_connect();
    }
  } else if (eventId == WIFI_EVENT_STA_DISCONNECTED) {
    xSemaphoreTake(m_mutex, portMAX_DELAY);
    m_status.staConnected = false;
    m_status.ip[0] = '\0';
    m_status.rssi = 0;
    xSemaphoreGive(m_mutex);

    m_retryCount++;
    if (m_retryCount == 3 && !m_status.apActive) {
      ESP_LOGW(TAG, "3 falhas STA; ativando SoftAP temporario de respaldo.");
      startSoftApFallback();
    }

    ESP_LOGW(TAG, "Wi-Fi desconectado. Proxima tentativa em %lu ms (operando offline).",
             static_cast<unsigned long>(m_backoffMs));
    esp_timer_stop(m_reconnectTimer);
    esp_timer_start_once(m_reconnectTimer, static_cast<uint64_t>(m_backoffMs) * 1000ULL);
    m_backoffMs = std::min<uint32_t>(m_backoffMs * 2, 300000);
  }
}

void NetworkManager::onIpEvent(int32_t eventId, void *eventData) {
  if (eventId != IP_EVENT_STA_GOT_IP) return;
  auto *event = static_cast<ip_event_got_ip_t *>(eventData);

  wifi_ap_record_t apInfo{};
  int8_t rssi = 0;
  char ssid[33] = "";
  if (esp_wifi_sta_get_ap_info(&apInfo) == ESP_OK) {
    rssi = apInfo.rssi;
    std::strncpy(ssid, reinterpret_cast<const char *>(apInfo.ssid), sizeof(ssid) - 1);
  }

  xSemaphoreTake(m_mutex, portMAX_DELAY);
  m_status.staConnected = true;
  m_status.rssi = rssi;
  std::strncpy(m_status.ssid, ssid, sizeof(m_status.ssid) - 1);
  std::snprintf(m_status.ip, sizeof(m_status.ip), IPSTR, IP2STR(&event->ip_info.ip));
  xSemaphoreGive(m_mutex);

  m_retryCount = 0;
  m_backoffMs = 10000;
  ESP_LOGI(TAG, "Conectado! IP=%s, SSID=%s, RSSI=%d dBm", m_status.ip, m_status.ssid, rssi);

  initSntpOnce();

  if (!m_mdnsStarted) {
    if (mdns_init() == ESP_OK) {
      mdns_hostname_set(Config::Network::DEFAULT_MDNS_HOST);
      mdns_instance_name_set("Irrigacao Inteligente PIBEX 2026");
      mdns_service_add("Irrigacao Web", "_http", "_tcp", 80, nullptr, 0);
      m_mdnsStarted = true;
      ESP_LOGI(TAG, "mDNS ativo: http://%s.local", Config::Network::DEFAULT_MDNS_HOST);
    }
  }

  if (m_status.apActive) {
    stopSoftAp();
  }
}

NetworkStatus NetworkManager::getStatus() const {
  NetworkStatus copy{};
  xSemaphoreTake(m_mutex, portMAX_DELAY);
  copy = m_status;
  xSemaphoreGive(m_mutex);
  copy.ntpSynced = TimeUtils::isNtpSynced();
  if (copy.staConnected) {
    wifi_ap_record_t apInfo{};
    if (esp_wifi_sta_get_ap_info(&apInfo) == ESP_OK) {
      copy.rssi = apInfo.rssi;
    }
  }
  return copy;
}
