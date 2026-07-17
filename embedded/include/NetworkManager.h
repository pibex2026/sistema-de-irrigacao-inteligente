#pragma once

#include "WiFi.h"
#include <DNSServer.h>
#include <ESPmDNS.h>
#include <WebServer.h>
#include <WiFiManager.h>
#include "config.h"

class NetworkManager {
public:
  NetworkManager();
  void begin();
  void loop();
  bool isConnected() const;
  const char *getSSID() const;
  const char *getIP() const;

  // --- Configuração de rede (modo, hostname, desconectar) ---
  void setHostname(const char *hostname);
  void disconnect();
  bool reconnect(); // tenta reconectar ao último AP salvo
  void setAutoReconnect(bool autoReconnect);

  // --- IP estático e gateway ---
  void setStaticIP(const IPAddress &ip, const IPAddress &gateway,
                   const IPAddress &subnet);
  void setDHCP(); // volta para IP dinâmico (DHCP)

  // --- Configuração de DNS ---
  void setDNS(const IPAddress &primaryDNS,
              const IPAddress &secondaryDNS = INADDR_NONE);

  // --- Recuperar informações de rede ---
  IPAddress getLocalIP() const;
  IPAddress getGatewayIP() const;
  IPAddress getSubnetMask() const;
  IPAddress getPrimaryDNS() const;
  IPAddress getSecondaryDNS() const;
  String getMACAddress() const;
  int8_t getRSSI() const; // intensidade do sinal em dBm

  // --- mDNS (para resolver .local) ---
  bool startMDNS(const char *hostname);
  void stopMDNS();

  // --- WiFiManager customizações ---
  void setAPName(const char *apName);         // nome do portal de configuração
  void setAPPassword(const char *apPassword); // senha do portal (opcional)
  void setConfigPortalTimeout(unsigned long seconds);
  void resetSettings(); // apaga credenciais salvas e reinicia

  // --- Status detalhado ---
  // true se o portal cativo estiver ativo
  bool isInConfigPortal() const;

  // descrição textual do estado
  String getConnectionStatus() const;

  void updateNetworkInfo();

private:
  WiFiManager m_wifiManager;
  bool m_autoReconnect = true;
  String m_hostname;
  bool m_useStaticIP = false;
  IPAddress m_staticIP = IPAddress(0, 0, 0, 0);
  IPAddress m_staticGateway = IPAddress(0, 0, 0, 0);
  IPAddress m_staticSubnet = IPAddress(255, 255, 255, 0);
  IPAddress m_primaryDNS = IPAddress(8, 8, 8, 8);
  IPAddress m_secondaryDNS = IPAddress(8, 8, 4, 4);
  String m_defaultApName = Config::Network::DEFAULT_AP_NAME;
  String m_defaultApPassword = Config::Network::DEFAULT_AP_PASSWORD; // vazia = portal aberto
  unsigned long m_portalTimeout = 240;     // segundos
  bool m_inPortal = false;
  bool m_useCustomDNS = false;
  bool m_mdnsStarted = false;
  uint8_t m_lastReconnectAttempt = 0;
  char m_ipStr[16] = "";
  char m_ssidStr[32] = "";
};