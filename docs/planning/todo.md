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
  - [x] Implementar Máquina de Estados (State Machine) no loop principal (`main.cpp`)
  - [x] Criar `enum class SystemState { INIT, READ_SENSORS, RUN_INFERENCE, EXECUTE_ACTION, ERROR }`

## 2. Módulo de Sensores (`SensorsManager`)

- [x] Criar assinaturas em `SensorsManager.h` e implementação em `SensorsManager.cpp`
- [x] Definir struct pública `SensorData` (Temperatura, Umidade do Ar, Umidade do Solo)
- [x] Implementar inicialização física no método `begin()`
- [x] Otimizar armazenamento e precisão das variáveis:
- [x] **Umidade do Ar:** converter `float` para `uint8_t` (0 a 100%) com arredondamento seguro
- [x] **Temperatura:** converter para ponto fixo `int16_t` (multiplicado por 10) para preservar 1 casa decimal sem usar `float` (ou `int8_t` sem decimais)
- [x] Implementar tratamento de erros usando valores sentinela (ex: `-999` ou `255`) e validação com `isnan`
- [x] Integrar leitura física do sensor de umidade do solo (ADC1) com mapeamento percentual (`map()`)

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

- [x] Criar classe `ModelManager` para gerenciar a Rede Neural (`espressif/esp-tflite-micro` INT8)
- [x] Implementar normalização e engenharia psicrométrica (`encodeFeatures27`) antes de alimentar o modelo
- [x] Executar a inferência, validar *Golden Test Vectors* e aplicar limiar de decisão + fallback térmico
- [x] Criar classe `ActuatorManager` para isolar o controle dos pinos de saída (Relé/Válvula com *timeout*, *cooldown* e *fail-safe*)
- [x] Vincular a decisão da Rede Neural à ativação física da irrigação via FSM no Core 1 (FreeRTOS + Watchdog)

---

## Boas Práticas e Convenções de Código (Checklist de Revisão)

- **Nomenclatura:**
  - Classes, Structs e Enums em `PascalCase` (ex: `SensorsManager`)
  - Funções e variáveis locais em `camelCase` (ex: `readSensors()`, `rawAnalogValue`)
  - Atributos privados com prefixo `m_` (ex: `m_dht`) ou underscore inicial (ex: `_dht`)
  - Constantes e macros em `UPPER_CASE` (ex: `DHT_SENSOR_PIN`)
- **Estrutura de Arquivos:**
  - Manter assinaturas estritamente nos arquivos `.h` (o "cardápio")
  - Manter lógica e implementações nos arquivos `.cpp` (a "cozinha")
- **Otimização de Hardware:**
  - Evitar uso excessivo de tipos `float` na RAM e na persistência (priorizar `uint8_t` e `int16_t` escalado)
  - Manter as tarefas pesadas de rede separadas do processamento de sensores (se necessário, usar FreeRTOS dividindo as tarefas entre o Core 0 e o Core 1 do ESP32)

  # Modelo de IA

* [x] **1. Preparação de Dados (Dataset)**
  - [x] Pesquisa, seleção e download de datasets relevantes de clima e solo (`Smart Agriculture Dataset`)
  - [x] Pré-processamento e limpeza de dados (deduplicação de 128 registros e proteção de sentinelas)
  - [x] Normalização dos inputs (Temperatura, Umidade do Ar, Umidade do Solo + 4 features VPD)
  - [x] Separação do dataset em conjuntos de Treino, Validação e Teste (70/15/15 + `GroupSplit`)
* [x] **2. Desenvolvimento e Treinamento**
  - [x] Definição da arquitetura da Rede Neural (`PhysMLP-27` 27 -> 32 -> 16 -> 3)
  - [x] Configuração do pipeline de treinamento (`EarlyStopping`, `ReduceLROnPlateau`, pesos calibrados)
  - [x] Execução do treinamento do modelo (`ai-model/train_tinyml_v2.py`)
* [x] **3. Otimização e Validação**
  - [x] Ajuste de hiperparâmetros e benchmark de 10 arquiteturas
  - [x] Avaliação de métricas de performance (Acurácia, Macro-F1, ECE, Matriz de Confusão nos 8 modos)
* [x] **4. Quantização e Exportação (TinyML)**
  - [x] Aplicação de quantização (conversão Full Integer Quantization `int8` com `representative_dataset`)
  - [x] Exportação do modelo treinado para o formato TensorFlow Lite (`model_int8.tflite` - 5.480 B)
  - [x] Conversão do arquivo `.tflite` para array hexadecimal C/C++ compatível com o ESP32 (`embedded/include/model_data.h`)
  - [x] Integração do array no código fonte ESP-IDF (`ModelManager` + `GOLDEN_VECTORS`) (restam testes em bancada física)

  # Aplicação web

  ## Geral

- [x] Documentar API e regras de negócio
- [] Definir estrutura de _payload_ (JSON)
- [x] Configurar containerização da aplicação e do banco de dados (Docker)

  ## Frontend

- [x] Criação de página de login
- [x] Criação de página de cadastro de usuário
- [x] Criação de página de troca de senha
- [x] Criação de dashboard para monitoramento de métricas em tempo real e histórico
- [x] Modularização de componentes e serviços (HTTP)
- [] Implementação de _guards_ de rota para proteção de telas autenticadas
- [] Configuração de _interceptors_ para injeção automática do token JWT nas requisições

  ## Backend (Node.js)

- [x] Modelagem do banco de dados relacional e criação de tabelas
- [x] Criar entidades básicas, DTOs e repositórios
- [x] Criar rotas e controllers REST
- [x] Implementar middleware para rotas autenticadas e filtros de segurança
- [x] Implementar login com JWT
- [x] Criar conexão com banco de dados
