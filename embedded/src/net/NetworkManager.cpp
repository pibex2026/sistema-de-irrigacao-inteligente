#include "NetworkManager.hpp"
#include "TimeUtils.hpp"
#include "config.hpp"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_sntp.h"
#include "esp_wifi.h"
#include "lwip/sockets.h"
#include "mdns.h"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>

static const char *TAG = "NetworkManager";

static const char *wifiDisconnectReasonToStr(uint8_t reason) {
  switch (reason) {
  case WIFI_REASON_NO_AP_FOUND:
  case WIFI_REASON_NO_AP_FOUND_W_COMPATIBLE_SECURITY:
  case WIFI_REASON_NO_AP_FOUND_IN_AUTHMODE_THRESHOLD:
  case WIFI_REASON_NO_AP_FOUND_IN_RSSI_THRESHOLD:
    return "Rede nao encontrada (SSID fora de alcance)";
  case WIFI_REASON_AUTH_FAIL:
  case WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT:
  case WIFI_REASON_HANDSHAKE_TIMEOUT:
  case WIFI_REASON_MIC_FAILURE:
  case WIFI_REASON_AKMP_INVALID:
    return "Senha incorreta ou falha de autenticacao";
  case WIFI_REASON_ASSOC_FAIL:
  case WIFI_REASON_ASSOC_EXPIRE:
    return "Falha de associacao com o roteador";
  case WIFI_REASON_BEACON_TIMEOUT:
    return "Perda de sinal (beacon timeout)";
  default:
    return "Falha na conexao Wi-Fi";
  }
}

NetworkManager::NetworkManager() {
  m_mutex = xSemaphoreCreateMutex();
}

NetworkManager::~NetworkManager() {
  stopDnsCaptiveServer();
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
  m_apNetif = esp_netif_create_default_wifi_ap();

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
    m_wifiStarted = true;
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
  apCfg.ap.beacon_interval = 100;
  apCfg.ap.ssid_hidden = 0;

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
  const wifi_mode_t nextMode = (m_wifiStarted && curMode == WIFI_MODE_STA) ? WIFI_MODE_APSTA : WIFI_MODE_AP;

  ESP_ERROR_CHECK(esp_wifi_set_mode(nextMode));
  ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &apCfg));
  if (!m_wifiStarted) {
    ESP_ERROR_CHECK(esp_wifi_start());
    m_wifiStarted = true;
  }

  configureDhcpCaptivePortal();

  xSemaphoreTake(m_mutex, portMAX_DELAY);
  m_status.apActive = true;
  xSemaphoreGive(m_mutex);

  wifi_config_t savedSta{};
  esp_wifi_get_config(WIFI_IF_STA, &savedSta);
  const bool hasSavedSsid = (std::strlen(reinterpret_cast<const char *>(savedSta.sta.ssid)) > 0);

  esp_timer_stop(m_apTimeoutTimer);
  if (hasSavedSsid) {
    esp_timer_start_once(m_apTimeoutTimer,
                         static_cast<uint64_t>(Config::Network::PORTAL_TIMEOUT_S) * 1000000ULL);
    ESP_LOGI(TAG, "SoftAP + DNS Captive ativo em 192.168.4.1 (timeout=%lus).",
             static_cast<unsigned long>(Config::Network::PORTAL_TIMEOUT_S));
  } else {
    ESP_LOGI(TAG, "SoftAP + DNS Captive ativo em 192.168.4.1 (permanente ate configurar Wi-Fi).");
  }
  startDnsCaptiveServer();
}

void NetworkManager::stopSoftAp() {
  stopDnsCaptiveServer();
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
  esp_timer_stop(m_reconnectTimer);
  m_retryCount = 0;
  m_backoffMs = 10000;

  xSemaphoreTake(m_mutex, portMAX_DELAY);
  m_status.staConnected = false;
  m_status.staConnecting = true;
  m_status.lastDisconnectReason = 0;
  m_status.lastError[0] = '\0';
  m_status.ip[0] = '\0';
  std::strncpy(m_status.ssid, ssid, sizeof(m_status.ssid) - 1);
  m_status.ssid[sizeof(m_status.ssid) - 1] = '\0';
  xSemaphoreGive(m_mutex);

  esp_wifi_disconnect();
  const esp_err_t err = esp_wifi_connect();
  if (err != ESP_OK) {
    xSemaphoreTake(m_mutex, portMAX_DELAY);
    m_status.staConnecting = false;
    std::snprintf(m_status.lastError, sizeof(m_status.lastError), "Erro ao iniciar conexao (%s)",
                  esp_err_to_name(err));
    xSemaphoreGive(m_mutex);
  }
  return err;
}

std::vector<WifiScanEntry> NetworkManager::scanNetworks(size_t maxResults) {
  std::vector<WifiScanEntry> results;
  if (!m_wifiStarted) return results;

  wifi_mode_t curMode = WIFI_MODE_NULL;
  if (esp_wifi_get_mode(&curMode) != ESP_OK) return results;

  if (curMode == WIFI_MODE_AP) {
    if (esp_wifi_set_mode(WIFI_MODE_APSTA) != ESP_OK) {
      ESP_LOGE(TAG, "Falha ao alternar para APSTA para scan Wi-Fi.");
      return results;
    }
  }

  // Evita ESP_ERR_WIFI_STATE caso o STA esteja em tentativa de conexao ativa
  bool wasConnected = false;
  xSemaphoreTake(m_mutex, portMAX_DELAY);
  wasConnected = m_status.staConnected;
  xSemaphoreGive(m_mutex);
  if (!wasConnected) {
    esp_timer_stop(m_reconnectTimer);
    esp_wifi_disconnect();
  }

  wifi_scan_config_t scanCfg{};
  scanCfg.show_hidden = false;
  scanCfg.scan_type = WIFI_SCAN_TYPE_ACTIVE;

  ESP_LOGI(TAG, "Iniciando varredura de redes Wi-Fi...");
  const esp_err_t scanErr = esp_wifi_scan_start(&scanCfg, true);
  if (scanErr != ESP_OK) {
    ESP_LOGW(TAG, "Falha no scan Wi-Fi: %s", esp_err_to_name(scanErr));
    return results;
  }

  uint16_t apCount = 0;
  if (esp_wifi_scan_get_ap_num(&apCount) != ESP_OK || apCount == 0) {
    ESP_LOGI(TAG, "Scan Wi-Fi concluido: 0 redes encontradas.");
    return results;
  }

  const uint16_t fetchCount = std::min<uint16_t>(apCount, 24);
  std::vector<wifi_ap_record_t> records(fetchCount);
  uint16_t actualCount = fetchCount;
  if (esp_wifi_scan_get_ap_records(&actualCount, records.data()) != ESP_OK) {
    return results;
  }

  for (uint16_t i = 0; i < actualCount; ++i) {
    const char *rawSsid = reinterpret_cast<const char *>(records[i].ssid);
    if (rawSsid[0] == '\0') continue;

    // Deduplica SSIDs repetidos (mantem o de maior RSSI)
    auto it = std::find_if(results.begin(), results.end(), [&](const WifiScanEntry &e) {
      return std::strncmp(e.ssid, rawSsid, sizeof(e.ssid)) == 0;
    });
    if (it != results.end()) {
      if (records[i].rssi > it->rssi) {
        it->rssi = records[i].rssi;
        it->authmode = static_cast<uint8_t>(records[i].authmode);
        it->channel = records[i].primary;
      }
      continue;
    }

    WifiScanEntry entry{};
    std::strncpy(entry.ssid, rawSsid, sizeof(entry.ssid) - 1);
    entry.rssi = records[i].rssi;
    entry.authmode = static_cast<uint8_t>(records[i].authmode);
    entry.channel = records[i].primary;
    results.push_back(entry);
  }

  std::sort(results.begin(), results.end(),
            [](const WifiScanEntry &a, const WifiScanEntry &b) { return a.rssi > b.rssi; });
  if (results.size() > maxResults) {
    results.resize(maxResults);
  }

  ESP_LOGI(TAG, "Scan Wi-Fi concluido: %u redes unicas retornadas.", static_cast<unsigned>(results.size()));
  return results;
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

void NetworkManager::onWifiEvent(int32_t eventId, void *eventData) {
  if (eventId == WIFI_EVENT_STA_START) {
    wifi_config_t savedSta{};
    esp_wifi_get_config(WIFI_IF_STA, &savedSta);
    if (std::strlen(reinterpret_cast<const char *>(savedSta.sta.ssid)) > 0) {
      xSemaphoreTake(m_mutex, portMAX_DELAY);
      m_status.staConnecting = true;
      xSemaphoreGive(m_mutex);
      esp_wifi_connect();
    }
  } else if (eventId == WIFI_EVENT_AP_STACONNECTED) {
    ESP_LOGI(TAG, "Cliente conectado ao SoftAP.");
  } else if (eventId == WIFI_EVENT_AP_STADISCONNECTED) {
    ESP_LOGI(TAG, "Cliente desconectado do SoftAP.");
  } else if (eventId == WIFI_EVENT_STA_DISCONNECTED) {
    uint8_t reason = 0;
    if (eventData != nullptr) {
      reason = static_cast<wifi_event_sta_disconnected_t *>(eventData)->reason;
    }
    xSemaphoreTake(m_mutex, portMAX_DELAY);
    m_status.staConnected = false;
    m_status.staConnecting = false;
    m_status.ip[0] = '\0';
    m_status.rssi = 0;
    m_status.lastDisconnectReason = reason;
    std::snprintf(m_status.lastError, sizeof(m_status.lastError), "%s (cod %u)",
                  wifiDisconnectReasonToStr(reason), static_cast<unsigned>(reason));
    xSemaphoreGive(m_mutex);

    m_retryCount++;
    if (m_retryCount == 3 && !m_status.apActive) {
      ESP_LOGW(TAG, "3 falhas STA; ativando SoftAP temporario de respaldo.");
      startSoftApFallback();
    }

    ESP_LOGW(TAG, "Wi-Fi desconectado (reason=%u). Proxima tentativa em %lu ms (operando offline).",
             static_cast<unsigned>(reason), static_cast<unsigned long>(m_backoffMs));
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
  m_status.staConnecting = false;
  m_status.lastDisconnectReason = 0;
  m_status.lastError[0] = '\0';
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
    // Mantem SoftAP por 15s para que a pagina em 192.168.4.1 exiba o IP obtido antes de encerrar o AP
    esp_timer_stop(m_apTimeoutTimer);
    esp_timer_start_once(m_apTimeoutTimer, 15ULL * 1000000ULL);
    ESP_LOGI(TAG, "SoftAP sera encerrado em 15s (novo IP STA: %s).", m_status.ip);
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

void NetworkManager::configureDhcpCaptivePortal() {
  if (m_apNetif == nullptr) return;
  static const char kCaptiveUri[] = "http://192.168.4.1/captive-portal-api";
  esp_netif_dhcps_stop(m_apNetif);
  const esp_err_t err = esp_netif_dhcps_option(
      m_apNetif, ESP_NETIF_OP_SET, ESP_NETIF_CAPTIVEPORTAL_URI,
      const_cast<char *>(kCaptiveUri), std::strlen(kCaptiveUri));
  if (err != ESP_OK) {
    ESP_LOGW(TAG, "Falha ao definir DHCP Option 114: %s", esp_err_to_name(err));
  } else {
    ESP_LOGI(TAG, "DHCP Option 114 (RFC 8910) configurado: %s", kCaptiveUri);
  }
  esp_netif_dhcps_start(m_apNetif);
}

void NetworkManager::startDnsCaptiveServer() {
  if (m_dnsRunning) return;
  m_dnsRunning = true;
  xTaskCreatePinnedToCore(dnsCaptiveTask, "dns_captive", 3072, this, 3, &m_dnsTaskHandle, 0);
}

void NetworkManager::stopDnsCaptiveServer() {
  m_dnsRunning = false;
  if (m_dnsSocket >= 0) {
    lwip_close(m_dnsSocket);
    m_dnsSocket = -1;
  }
}

void NetworkManager::dnsCaptiveTask(void *arg) {
  auto *self = static_cast<NetworkManager *>(arg);
  const int sock = lwip_socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  if (sock < 0) {
    ESP_LOGE(TAG, "Falha ao criar socket UDP DNS Captive.");
    self->m_dnsRunning = false;
    self->m_dnsTaskHandle = nullptr;
    vTaskDelete(nullptr);
    return;
  }
  self->m_dnsSocket = sock;

  int reuse = 1;
  lwip_setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
  struct timeval tv{1, 0};
  lwip_setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_port = htons(53);
  addr.sin_addr.s_addr = htonl(INADDR_ANY);
  if (lwip_bind(sock, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) < 0) {
    ESP_LOGE(TAG, "Falha ao fazer bind na porta UDP 53 (DNS Captive).");
    lwip_close(sock);
    self->m_dnsSocket = -1;
    self->m_dnsRunning = false;
    self->m_dnsTaskHandle = nullptr;
    vTaskDelete(nullptr);
    return;
  }
  ESP_LOGI(TAG, "Servidor DNS Captive ativo na porta UDP 53.");

  uint8_t rx[512];
  uint8_t tx[512];
  while (self->m_dnsRunning) {
    sockaddr_in clientAddr{};
    socklen_t clientLen = sizeof(clientAddr);
    const int len = lwip_recvfrom(sock, rx, sizeof(rx), 0,
                                  reinterpret_cast<sockaddr *>(&clientAddr), &clientLen);
    if (len < 12 || len > 480) continue;
    // Responde apenas queries (QR == 0)
    if (rx[2] & 0x80) continue;

    // Localiza fim da Question (QNAME + QTYPE + QCLASS)
    int idx = 12;
    while (idx < len && rx[idx] != 0) {
      idx += rx[idx] + 1;
    }
    if (idx + 5 > len) continue;
    const uint16_t qtype = (static_cast<uint16_t>(rx[idx + 1]) << 8) | rx[idx + 2];
    const int qEnd = idx + 5;

    std::memcpy(tx, rx, static_cast<size_t>(qEnd));
    tx[2] = 0x85; // QR=1, AA=1, RD=1
    tx[3] = 0x80; // RA=1, RCODE=0
    tx[8] = 0x00;
    tx[9] = 0x00; // NSCOUNT = 0
    tx[10] = 0x00;
    tx[11] = 0x00; // ARCOUNT = 0

    if (qtype == 1) {
      // QTYPE A: responde com 192.168.4.1
      tx[6] = 0x00;
      tx[7] = 0x01; // ANCOUNT = 1
      const uint8_t ans[16] = {
          0xC0, 0x0C, 0x00, 0x01, 0x00, 0x01,
          0x00, 0x00, 0x00, 0x3C, 0x00, 0x04,
          192, 168, 4, 1
      };
      std::memcpy(tx + qEnd, ans, sizeof(ans));
      lwip_sendto(sock, tx, static_cast<size_t>(qEnd + sizeof(ans)), 0,
                  reinterpret_cast<sockaddr *>(&clientAddr), clientLen);
    } else {
      // QTYPE AAAA / HTTPS / etc.: responde NODATA valido (ANCOUNT = 0)
      tx[6] = 0x00;
      tx[7] = 0x00;
      lwip_sendto(sock, tx, static_cast<size_t>(qEnd), 0,
                  reinterpret_cast<sockaddr *>(&clientAddr), clientLen);
    }
  }

  if (self->m_dnsSocket >= 0) {
    lwip_close(self->m_dnsSocket);
    self->m_dnsSocket = -1;
  }
  self->m_dnsTaskHandle = nullptr;
  vTaskDelete(nullptr);
}
