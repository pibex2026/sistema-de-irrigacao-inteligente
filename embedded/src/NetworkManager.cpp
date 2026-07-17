#include "NetworkManager.h"

NetworkManager::NetworkManager() {
  m_autoReconnect;
  m_hostname;
  m_useStaticIP;
  m_staticIP;
  m_staticGateway;
  m_staticSubnet;
  m_primaryDNS;
  m_secondaryDNS;
  m_useCustomDNS;
  m_mdnsStarted;
  m_defaultApName;
  m_defaultApPassword;
  m_portalTimeout;
  m_inPortal;
  m_lastReconnectAttempt;
  m_ipStr;
  m_ssidStr;
}

void NetworkManager::begin() {
  WiFi.mode(WIFI_STA);

  if (m_hostname.length() > 0) {
    WiFi.setHostname(m_hostname.c_str());
  }

  // Apply network settings (static IP / custom DNS)
  if (m_useStaticIP) {
    WiFi.config(m_staticIP, m_staticGateway, m_staticSubnet, m_primaryDNS,
                m_secondaryDNS);
  } else if (m_useCustomDNS) {
    WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE, m_primaryDNS,
                m_secondaryDNS);
  } else {
    // DHCP with default DNS
    WiFi.config(0U, 0U, 0U);
  }

  m_wifiManager.setConfigPortalTimeout(m_portalTimeout);
  // Start non-blocking auto-connect (portal will be active if needed)
  m_wifiManager.autoConnect(m_defaultApName.c_str(),
                            m_defaultApPassword.c_str());
}

void NetworkManager::loop() {
  m_wifiManager.process(); // handle DNS/config portal requests
  m_inPortal = m_wifiManager.getConfigPortalActive();

  // Auto-reconnect logic when not in portal
  if (!m_inPortal && !isConnected() && m_autoReconnect) {
    unsigned long now = millis();
    if (now - m_lastReconnectAttempt > 30000) { // 30 second interval
      reconnect();
      m_lastReconnectAttempt = now;
    }
  }

  updateNetworkInfo();
}

bool NetworkManager::isConnected() const {
  return WiFi.status() == WL_CONNECTED;
}

const char *NetworkManager::getSSID() const { return m_ssidStr; }

const char *NetworkManager::getIP() const { return m_ipStr; }

void NetworkManager::setHostname(const char *hostname) {
  m_hostname = hostname;
  if (WiFi.isConnected()) {
    // Hostname takes effect only on next connection
  }
}

void NetworkManager::disconnect() { WiFi.disconnect(false, true); }

bool NetworkManager::reconnect() {
  if (isConnected()) {
    return true; // already connected
  }

  WiFi.disconnect(true);

  // Apply current IP/DNS settings before connecting
  if (m_useStaticIP) {
    WiFi.config(m_staticIP, m_staticGateway, m_staticSubnet, m_primaryDNS,
                m_secondaryDNS);
  } else if (m_useCustomDNS) {
    WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE, m_primaryDNS,
                m_secondaryDNS);
  } else {
    WiFi.config(0U, 0U, 0U);
  }

  WiFi.begin(); // use last saved credentials
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 15000) {
    delay(100);
  }
  return isConnected();
}

void NetworkManager::setAutoReconnect(bool autoReconnect) {
  m_autoReconnect = autoReconnect;
}

void NetworkManager::setStaticIP(const IPAddress &ip, const IPAddress &gateway,
                                 const IPAddress &subnet) {
  m_useStaticIP = true;
  m_staticIP = ip;
  m_staticGateway = gateway;
  m_staticSubnet = subnet;
  // Keep existing DNS or defaults
}

void NetworkManager::setDHCP() {
  m_useStaticIP = false;
  // If custom DNS is set, it will be handled separately
}

void NetworkManager::setDNS(const IPAddress &primaryDNS,
                            const IPAddress &secondaryDNS) {
  m_useCustomDNS = true;
  m_primaryDNS = primaryDNS;
  m_secondaryDNS =
      (secondaryDNS == INADDR_NONE) ? IPAddress(0, 0, 0, 0) : secondaryDNS;
}

IPAddress NetworkManager::getLocalIP() const { return WiFi.localIP(); }

IPAddress NetworkManager::getGatewayIP() const { return WiFi.gatewayIP(); }

IPAddress NetworkManager::getSubnetMask() const { return WiFi.subnetMask(); }

IPAddress NetworkManager::getPrimaryDNS() const { return WiFi.dnsIP(0); }

IPAddress NetworkManager::getSecondaryDNS() const { return WiFi.dnsIP(1); }

String NetworkManager::getMACAddress() const { return WiFi.macAddress(); }

int8_t NetworkManager::getRSSI() const { return WiFi.RSSI(); }

bool NetworkManager::startMDNS(const char *hostname) {
  if (m_mdnsStarted)
    stopMDNS();
  if (MDNS.begin(hostname)) {
    m_mdnsStarted = true;
    return true;
  }
  return false;
}

void NetworkManager::stopMDNS() {
  if (m_mdnsStarted) {
    MDNS.end();
    m_mdnsStarted = false;
  }
}

void NetworkManager::setAPName(const char *apName) { m_defaultApName = apName; }

void NetworkManager::setAPPassword(const char *apPassword) {
  m_defaultApPassword = apPassword;
}

void NetworkManager::setConfigPortalTimeout(unsigned long seconds) {
  m_portalTimeout = seconds;
}

void NetworkManager::resetSettings() {
  m_wifiManager.resetSettings();
  delay(100);
  ESP.restart();
}

bool NetworkManager::isInConfigPortal() const { return m_inPortal; }

String NetworkManager::getConnectionStatus() const {
  if (isConnected()) {
    return "Connected";
  } else if (m_inPortal) {
    return "Configuring";
  }
  return "Disconnected";
}

// Private helper: refresh cached text representations
void NetworkManager::updateNetworkInfo() {
  if (isConnected()) {
    WiFi.localIP().toString().toCharArray(m_ipStr, sizeof(m_ipStr));
    WiFi.SSID().toCharArray(m_ssidStr, sizeof(m_ssidStr));
  } else {
    m_ipStr[0] = '\0';
    m_ssidStr[0] = '\0';
  }
}
