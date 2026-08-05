#include "NetworkManager.h"



static const char* TAG = "Network Manager";
Preferences prefs;

NetworkManager::NetworkManager(uint16_t webServerPort)  
    : _server(webServerPort), _useStaticIP(false) {
}

bool NetworkManager::begin(NetworkMode mode, const char* apName, const char* ssid, const char* pass) {
    bool connected = false;

    // Se o usuário configurou um IP estático, aplicamos as configurações
    if (_useStaticIP) {
        // Aplica para o WiFi nativo (usado no FIXED e AP)
        WiFi.config(_localIP, _gateway, _subnet);
        // Aplica para o WiFiManager
        _wm.setSTAStaticIPConfig(_localIP, _gateway, _subnet);
        ESP_LOGI(TAG, "Succesfully conected with static IP");
    }


    switch (mode) {
        case NetworkMode::MODE_WIFI_MANAGER:
            
            connected = _wm.autoConnect(apName);
            break;

        case NetworkMode::MODE_FIXED_CREDENTIALS:
            connected = connectFixedCredentials(ssid, pass);
            break;

        case NetworkMode::MODE_AP_ONLY:
            connected = startAPOnly(apName);
            break;
    }

    // Se a conexão foi bem sucedida (ou o AP subiu) e temos um nome mDNS, iniciamos ele
    if (connected && _mdnsName.length() > 0) {
        if (MDNS.begin(_mdnsName.c_str())) {
            Serial.printf("mDNS iniciado. Acesse: http://%s.local\n", _mdnsName.c_str());
        }
    }

    // Inicia o servidor Web
    _server.begin();

    return connected;
}

void NetworkManager::resetCredentials() {
    _wm.resetSettings();
    prefs.clear();
    ESP_LOGW(TAG, "Reset wifi credencials");
}

IPAddress getDefaultGateway() {
    return IPAddress(Config::Network::DEFAULT_GATEWAY);
}

IPAddress getDefaultSubnet() {
    return IPAddress(Config::Network::DEFAULT_SUBNET);
}

void NetworkManager::setStaticIP(IPAddress ip) {
    _localIP = ip;
    
    _gateway = getDefaultGateway(); 
    _subnet = getDefaultSubnet();   
    _useStaticIP = true;
}


void NetworkManager::setStaticIP(IPAddress ip, IPAddress gateway, IPAddress subnet) {
    _localIP = ip;
    _gateway = gateway;
    _subnet = subnet;
    _useStaticIP = true;
}

bool NetworkManager::setMDNSName(const char* name) {
    _mdnsName = String(name);
    return true;
}

IPAddress NetworkManager::getIP() {
    if (WiFi.getMode() == WIFI_AP || WiFi.getMode() == WIFI_AP_STA) {
        return WiFi.softAPIP();
    }
    return WiFi.localIP();
}

String NetworkManager::getIPString() {
    return getIP().toString();
}

WebServer& NetworkManager::getServer() {
    return _server;
}

void NetworkManager::handle() {
    _server.handleClient();
}

// --- MÉTODOS PRIVADOS AUXILIARES ---

bool NetworkManager::connectFixedCredentials(const char* ssid, const char* pass) {
    if (!ssid || !pass) return false;
    
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, pass);
    
    Serial.print("Conectando ao WiFi");
    uint8_t retries = 0;
    while (WiFi.status() != WL_CONNECTED && retries < 20) {
        delay(500);
        Serial.print(".");
        retries++;
    }
    Serial.println();
    
    return WiFi.status() == WL_CONNECTED;
}

bool NetworkManager::startAPOnly(const char* apName) {
    WiFi.mode(WIFI_AP);
    return WiFi.softAP(apName); // Sem senha, rede aberta. Pode alterar adicionando senha.
}


void saveStaticIP(IPAddress ip, IPAddress gateway, IPAddress subnet) {
    prefs.begin("network", false);

    prefs.putBool("useStatic", true);

    prefs.putUInt("ip", (uint32_t)ip);
    prefs.putUInt("gateway", (uint32_t)gateway);
    prefs.putUInt("subnet", (uint32_t)subnet);

    prefs.end();
}

// --- FUNÇÃO PARA CARREGAR NO BOOT ---
bool loadStaticIP(IPAddress &ip, IPAddress &gateway, IPAddress &subnet) {
    // Abre a partição "network" no modo Somente Leitura (true)
    prefs.begin("network", true);

    // Lê se o IP estático está habilitado (retorna 'false' se a chave não existir)
    bool useStatic = prefs.getBool("useStatic", false);

    if (useStatic) {
        // Lê os inteiros salvos da Flash e reconverte para IPAddress
        ip = IPAddress(prefs.getUInt("ip", 0));
        gateway = IPAddress(prefs.getUInt("gateway", 0));
        subnet = IPAddress(prefs.getUInt("subnet", 0));
    }

    prefs.end();
    return useStatic; // Retorna true se tinha configuração estática salva
}

