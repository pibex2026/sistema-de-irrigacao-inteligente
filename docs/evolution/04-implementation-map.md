# 04 — Mapa Detalhado de Implementação por Camada

Este documento inventaria os módulos, contratos e mecanismos de segurança implementados em cada camada do repositório no estado atual (`3c5ac4c`).

---

## 1. Camada Embarcada (`embedded/`)

### 1.1. Contratos, Configuração e Modelo (`embedded/include/`)
* **`config.hpp`:** Define pinagem (`DHT_SENSOR_PIN = GPIO_NUM_5`, `SOIL_MOISTURE_ADC_PIN = GPIO_NUM_32`, `RELAY_VALVE_PIN = GPIO_NUM_25`, `STATUS_LED_PIN = GPIO_NUM_2`), polaridade (`RELAY_ACTIVE_LOW = true`), constantes de rede/NTP (`BRT3`), limites do LittleFS (`180 KB` por CSV) e parâmetros **FreeRTOS SMP** em `Config::Tasks` (`ACTUATOR_SAFETY_PRIO = 7`, `SENSOR_AI_PRIO = 6`, `HTTPD_PRIO = 4`, `TELEMETRY_PRIO = 3`, `LOGGER_PRIO = 2`, `DNS_CAPTIVE_PRIO = 2`).
* **`types.hpp`:** Contratos `SystemState`, `IrrigationDecision` (`0=OFF`, `1=IRRIGATE`, `2=THERMAL_STRESS_WET_SOIL`), `ActuatorCommandType`, `SystemConfig` (`schemaVersion = 2`), `SensorData` (temperatura em ponto fixo `int16_t` x10, sentinela `-999`), `InferenceResult`, `ActuatorStatus`, `PerformanceStats` (latências, arena TFLM, stack livre por task, fila/descartes do logger) e `SystemSnapshot`.
* **`ConfigSanitizer.hpp`:** Função inline `sanitizeSystemConfig()` isolada para permitir validação unitária em host e no `ConfigManager`.
* **`model_data.h`:** Array `g_model_data[]` (`5.480 B`), parâmetros de quantização INT8, `isValidSensorReading()`, `computeVpdKpa()`, `encodeFeatures27()` e `GOLDEN_VECTORS[6]`.

### 1.2. Núcleo (`embedded/src/core/`)
* **`ConfigManager`:** Persistência na partição `nvs` (namespace `"irrig_cfg"`), auto-recuperação em `ESP_ERR_NVS_NO_FREE_PAGES`, sanitização via `sanitizeSystemConfig()` e leitura/escrita protegida por mutex.
* **`SystemStateStore`:** Armazenamento centralizado do `SystemSnapshot` protegido por mutex único, com métodos granulares por produtor (`updateFromSensorAi`, `updateFromActuator`, `updateLoggerMetrics`, `updateTelemetryMetrics`).
* **`TimeUtils`:** Timestamp ISO-8601 thread-safe (`localtime_r`) com fallback explícito `UPTIME_<seg>s` quando o relógio não está sincronizado (`tm_year < 120`), além de `getMillis()` baseado em `esp_timer_get_time()`.

### 1.3. Drivers de Sensores (`embedded/src/drivers/`)
* **`Dht22Driver`:** Leitura 1-wire em `GPIO_NUM_5` com seção crítica (`portMUX_TYPE`) apenas na janela de captura dos 40 bits, validação de *checksum* e intervalo mínimo de `2 s`.
* **`SensorsManager`:** Integra DHT22 e HW-080 (`ADC_UNIT_1`, `ADC_CHANNEL_4`, `ADC_ATTEN_DB_12`, calibração eFuse). Usa mediana de 10 amostras protegida por `SemaphoreHandle_t`, rejeitando curto (`<= 10`) e fio solto (`>= 4090`).

### 1.4. IA Embarcada e Atuação (`embedded/src/ai/` e `embedded/src/control/`)
* **`ModelManager`:** Carrega `g_model_data` via `espressif/esp-tflite-micro` (arena `12 KB`), rejeita sentinelas antes da normalização, aplica fallback físico de estresse térmico (`temp >= 32 °C && soil >= 65%`) e executa `runGoldenSelfTest()` no boot.
* **`ActuatorManager`:** Inicializa o pino do relé em estado de repouso antes de `GPIO_MODE_OUTPUT` (evita *glitch* no boot), controla `STATUS_LED_PIN` por estado, exige `2` confirmações seguidas para abrir (*debounce*) e fecha imediatamente em `OFF`, `THERMAL_STRESS_WET_SOIL`, `ERROR`, `maxIrrigationMs` ou `emergencyStop()`. Expõe `manualOverride()` protegido por mutex.

### 1.5. Armazenamento, Rede e Testes (`embedded/src/storage/`, `net/`, `test/`)
* **`FileLogger`:** Monta LittleFS (`/littlefs` na partição `spiffs`), opera de forma assíncrona via fila de `24` slots em RAM e `logger_task` (descartando o item mais antigo se cheia), realiza rotação em `180 KB` (`*.prev.csv`) e leitura reversa sem OOM.
* **`NetworkManager`:** Gerencia `esp_wifi` (STA + SoftAP `ESP-Irrigacao`), reconexão com *backoff* exponencial via `esp_timer`, SNTP (`a.st1.ntp.br` / `pool.ntp.org`), DHCP RFC 8910, varredura Wi-Fi e tarefa `dns_captive`.
* **`LocalWebServer`:** `esp_http_server` + `cJSON` + mDNS (`irrigacao.local`), `22` rotas HTTP, validação de `X-Api-Pin` em rotas de escrita, `readRequestBody()` em loop e `readFileChunk()` com trava curta de mutex.
* **`TelemetryClient`:** `esp_http_client` acordado por `xTaskNotifyGive` (ou timeout de `1 s`), envia `POST` JSON com `X-Device-Key` para `cfg.backendUrl`.
* **UI & Testes:** `embedded/view/index.html` comprimido via `embedded/scripts/gzip_data.py` para `embedded/data/index.html.gz`; suítes Unity em host (`embedded/test/run_host_tests.sh`): `test_tinyml_golden.cpp`, `test_config_sanitize.cpp` e `test_actuator_logic.cpp`.

---

## 2. Camada de Inteligência Artificial (`ai-model/`)

* **`datasets/`:** Contém os 4 datasets avaliados; o oficial é `Smart Agriculture Dataset/cropdata_updated.csv` (`16.411` $\rightarrow$ `16.283` registros únicos após deduplicação).
* **`scripts/`:**
  - `train_tinyml_v2.py`: Pipeline oficial (`SEED = 42`), engenharia psicrométrica (`27` features), *Full-8 Mask Context Dropout*, pesos `{0: 1.0, 1: 1.15, 2: 2.20}`, `GroupSplit`, quantização Full INT8, exportação de `model_data.h` e modo *fine-tuning* (`--field-inferences`).
  - `benchmark_architectures.py` e `generate_plots.py`: Benchmark de 10 arquiteturas e geração das 12 pranchas gráficas.
* **`notebooks/`:** `train_tinyml_v2_colab.ipynb` (versão Colab/Jupyter sincronizada com a v2).
* **`output/`:** `model_int8.tflite` (`5.480 B`), `model_float32.tflite` (`8.216 B`), `model_data.h`, `train_tinyml_v2_report.json`, `benchmark_summary.json` e `figures/` (`13` imagens PNG).

---

## 3. Camada Web (`web/back/`, `web/front/`) e Infraestrutura

* **Backend (`web/back/`):**
  - `configs/env.js`: Validação fail-fast de `JWT_SECRET` em produção e leitura de `DEVICE_API_KEY`.
  - `configs/db.js` e `schema.sql`: Pool `pg` e tabelas `usuarios`, `leituras_sensores` e `historico_irrigacao`.
  - `middlewares/`: `authMiddleware.js` (JWT), `deviceAuthMiddleware.js` (`X-Device-Key`) e `rateLimitMiddleware.js` (`20 req / 15 min`).
  - `routes/`, `controllers/`, `services/`, `models/`: Fluxos de autenticação (`/api/auth/register`, `/login`, `/me`, `/password`) e telemetria (`/api/telemetry`, `/api/irrigation/history`, `/api/irrigation/manual`).
  - `tests/api.test.js`: `7` testes unitários com `node:test`. *(Nota: `configs/firebase.js` permanece sem referências.)*
* **Frontend (`web/front/`):** `index.html` (Tailwind CDN) e `c.js` (login/cadastro, validação de sessão `/api/auth/me`, polling controlado de telemetria, tabela de histórico e botão de acionamento manual).
* **Infraestrutura Raiz:** `docker-compose.yml` (PostgreSQL 15 com `healthcheck` + `backend`), `.github/workflows/ci.yml` e `.env.example`.
