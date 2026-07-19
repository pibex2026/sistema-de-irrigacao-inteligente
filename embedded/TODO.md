# To-Do's - Pibex 2026 - Projeto de Irrigação Inteligente (ESP32)

# Embarcado
## 1. Arquitetura Geral & Configurações de Base
- [x] Criar arquivo de configuração global (`config.h`)
  - [x] Implementar estrutura usando C++ moderno (`namespace Config`)
  - [x] Definir pinagem dos sensores usando `constexpr uint8_t` (Mapear analógicos no ADC1: GPIO 32/33)
  - [x] Definir intervalos de amostragem e constantes de tempo
- [x] Criar biblioteca utilitária independente de frameworks (`utils.h` / `utils.cpp`)
  - [x] Implementar função `getFormattedTimestamp()` usando `<time.h>` padrão e `localtime_r` (thread-safe)
  - [x] Adicionar validação de sincronização NTP (retornar `nullptr` se o ano for anterior a 2020)
- [ ] Implementar Máquina de Estados (State Machine) no loop principal (`main.cpp`)
  - [ ] Criar `enum class SystemState { INIT, READ_SENSORS, RUN_INFERENCE, EXECUTE_ACTION, ERROR }`

## 2. Módulo de Sensores (`SensorsManager`)
- [x] Criar assinaturas em `SensorsManager.h` e implementação em `SensorsManager.cpp`
- [x] Definir struct pública `SensorData` (Temperatura, Umidade do Ar, Umidade do Solo)
- [x] Implementar inicialização física no método `begin()`
- [x] Otimizar armazenamento e precisão das variáveis:
  - [x] **Umidade do Ar:** converter `float` para `uint8_t` (0 a 100%) com arredondamento seguro
  - [x] **Temperatura:** converter para ponto fixo `int16_t` (multiplicado por 10) para preservar 1 casa decimal sem usar `float` (ou `int8_t` sem decimais)
- [x] Implementar tratamento de erros usando valores sentinela (ex: `-999` ou `255`) e validação com `isnan`
- [ ] Integrar leitura física do sensor de umidade do solo (ADC1) com mapeamento percentual (`map()`)

## 3. Módulo de Conectividade (`NetworkManager`)
- [x] Criar classe `NetworkManager` para encapsular a lógica de rede
- [x] Integrar `WiFiManager` para provisionamento dinâmico de rede sem credenciais hardcoded
- [x] Configurar `EspWebServer` para criar uma interface de monitoramento local
- [x] Configurar `ESPmDNS` para acesso amigável na rede local.
- [x] Sincronizar o relógio interno do ESP32 com o servidor NTP (`pool.ntp.org`) apenas uma vez após a conexão Wi-Fi ser estabelecida

## 4. Módulo de Armazenamento (`StorageManager`)
- [x] Inicializar o sistema de arquivos `LittleFS`
- [x] Criar funções para salvar e ler dados históricos (logs de sensores com timestamp)
- [x] Criar mecanismo para salvar configurações de calibração sem precisar regravar o firmware
- [x] Criar função para resgatar arquivos da memória no formato .csv.
- [x] Criar estrutura para criação e validação dos arquivos .csv na memória.

## 5. Inteligência & Atuação (`ModelManager` & `ActuatorManager`)
- [ ] Criar classe `ModelManager` para gerenciar a Rede Neural
  - [ ] Implementar normalização dos inputs dos sensores antes de alimentar o modelo (ex: `umidade / 100.0f`)
  - [ ] Executar a inferência e obter o limiar de decisão
- [ ] Criar classe `ActuatorManager` para isolar o controle dos pinos de saída (Relé/Válvula de irrigação)
- [ ] Vincular a decisão da Rede Neural à ativação física da irrigação

---

## Boas Práticas e Convenções de Código (Checklist de Revisão)
* **Nomenclatura:**
  - Classes, Structs e Enums em `PascalCase` (ex: `SensorsManager`)
  - Funções e variáveis locais em `camelCase` (ex: `readSensors()`, `rawAnalogValue`)
  - Atributos privados com prefixo `m_` (ex: `m_dht`) ou underscore inicial (ex: `_dht`)
  - Constantes e macros em `UPPER_CASE` (ex: `DHT_SENSOR_PIN`)
* **Estrutura de Arquivos:**
  - Manter assinaturas estritamente nos arquivos `.h` (o "cardápio")
  - Manter lógica e implementações nos arquivos `.cpp` (a "cozinha")
* **Otimização de Hardware:**
  - Evitar uso excessivo de tipos `float` na RAM e na persistência (priorizar `uint8_t` e `int16_t` escalado)
  - Manter as tarefas pesadas de rede separadas do processamento de sensores (se necessário, usar FreeRTOS dividindo as tarefas entre o Core 0 e o Core 1 do ESP32)