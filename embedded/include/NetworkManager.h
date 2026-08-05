#pragma once

#include <WiFi.h>
#include <DNSServer.h>
#include <ESPmDNS.h>
#include <WebServer.h>
#include <WiFiManager.h>
#include "config.h" 
#include "esp_log.h"
#include <Preferences.h>

// Enum para os modos de operação da rede
enum class NetworkMode {
    MODE_WIFI_MANAGER,     // Tenta conectar; se falhar/não achar rede, abre o AP do WiFiManager
    MODE_FIXED_CREDENTIALS,// Conecta diretamente usando SSID e Senha fornecidos
    MODE_AP_ONLY           // Sobe diretamente como Ponto de Acesso (Access Point) isolado
};

class NetworkManager {
public:
    // Construtor com porta padrão do WebServer (80)
    NetworkManager(uint16_t webServerPort = 80);

    // Inicializa a rede de acordo com o modo selecionado
    bool begin(NetworkMode mode, const char* apName = "ESP32_Config", const char* ssid = nullptr, const char* pass = nullptr);

    // Gerenciamento de credenciais
    void resetCredentials();

    // Sobrecarga de métodos para IP Estático
    void setStaticIP(IPAddress ip);
    void setStaticIP(IPAddress ip, IPAddress gateway, IPAddress subnet);

    // Configuração do mDNS
    bool setMDNSName(const char* name);

    // Recuperação de informações da rede
    IPAddress getIP();
    String getIPString();

    // Retorna a referência do WebServer para criação de rotas REST/Arquivos Estáticos
    WebServer& getServer();

    // Mantém o WebServer e o mDNS rodando (deve ser chamado no loop() principal)
    void handle();

private:
    WebServer _server;
    WiFiManager _wm;
    
    String _mdnsName;
    bool _useStaticIP;
    IPAddress _localIP;
    IPAddress _gateway;
    IPAddress _subnet;

    // Métodos auxiliares internos
    bool connectFixedCredentials(const char* ssid, const char* pass);
    bool startAPOnly(const char* apName);
};
