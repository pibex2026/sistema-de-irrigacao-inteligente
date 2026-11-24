# 02 — Viradas de Arquitetura (ADRs Retrospectivos)

Este documento registra as **10 principais viradas de arquitetura** ocorridas ao longo do desenvolvimento, explicando o estado anterior, o gatilho da mudança, a solução adotada e o impacto verificado.

---

## Resumo das Viradas

| ID | Área | Antes | Depois | Marco(s) |
| :--- | :--- | :--- | :--- | :--- |
| **ADR-01** | Backend | Placeholder Go + ThingSpeak/Firebase | Node.js/Express 5 + PostgreSQL 15 + Docker | `e7dd58a`, `3002086`, `59ec21e`, `a9f628c` |
| **ADR-02** | Dados & IA | Datasets incompatíveis / binário | *Smart Agriculture Dataset* (3 classes) | `c3366f3` |
| **ADR-03** | Modelo TinyML | MLP v1 (23 feat., vazamento 91,85%, F32) | `PhysMLP-27` v2 (VPD, Full-8, INT8 `5.480 B`) | `8834fef`, `c85dfc0`, `1c2de20`, `25f3463` |
| **ADR-04** | Firmware | Arduino monolítico (`loop()`, `WiFiManager`) | Reescrita ESP-IDF v5.5 (C++17 + FreeRTOS) | `6f6af13`, `f1f09a8` |
| **ADR-05** | UI Embarcada | Página estática simples | SPA gzip + mDNS + Portal Cativo RFC 8910 | `c330580`, `1bc2c80`, `1dc7ca3`, `b2a58c5` |
| **ADR-06** | Persistência | Constantes em código (`config.h`) | NVS schema v2 + `ConfigSanitizer` + LittleFS 180 KB | `f1f09a8`, `34b5c7d`, `8e755b8` |
| **ADR-07** | Integração | Hardware isolado + dashboard `Math.random()` | Fluxo E2E (`TelemetryClient` $\rightarrow$ API $\rightarrow$ Postgres) | `8291295`, `34b5c7d`, `f6bb3c9`, `8399c50` |
| **ADR-08** | Segurança | Credenciais hardcoded e rotas abertas | `JWT_SECRET` fail-fast, `X-Device-Key`, `X-Api-Pin`, Rate Limit | `e0335d1`, `083ecc0`, `f17c198` |
| **ADR-09** | Verificação | Sem testes automatizados nem CI | Unity em host (C++) + `node --test` + GitHub Actions | `2d87467`, `f17c198`, `8e755b8` |
| **ADR-10** | Concorrência | `controlTask` monolítica presa ao Core 1 | FreeRTOS SMP (`tskNO_AFFINITY`), filas e `SystemStateStore` | `8fbe408..3c5ac4c` |

---

## Detalhamento das Viradas

### ADR-01 — Backend: Placeholder Go / Nuvem Terceirizada $\rightarrow$ Node.js + Express + PostgreSQL
* **Estado Anterior:** O commit `e7dd58a` criou `web/back/a.go`, enquanto `docs/planning/checklists.md` inicialmente citava `ThingSpeak` e `4d647b2` adicionou `configs/firebase.js`.
* **Gatilho:** Necessidade de API REST própria, persistência relacional para usuários/sensores/histórico e empacotamento simples via Docker.
* **Nova Arquitetura:** Backend em **Node.js/Express + PostgreSQL** (`59ec21e..3954953`), remoção de `a.go` (`3002086`) e orquestração via `docker-compose.yml` (`a9f628c`). *(Nota: `firebase-admin` e `web/back/configs/firebase.js` permaneceram sem uso como vestígio histórico.)*

### ADR-02 — Formulação de IA: Datasets Incompatíveis $\rightarrow$ *Smart Agriculture Dataset* (3 Classes)
* **Estado Anterior:** Avaliação de datasets populares (*Predicting watering the plants* / `TARP.csv`, *IoT Irrigation Sensor Dataset*, *Manzano2022*).
* **Gatilho:** Auditoria estatística (`c3366f3`) provou que `TARP.csv` tinha `76%` de nulos nas variáveis de ar e correlação nula com DHT22, e que `iotsensordata.csv` era cópia bit-a-bit idêntica por hash MD5 (`94cb7d6f6f8f`).
* **Nova Arquitetura:** Adoção do *Smart Agriculture Dataset* (`cropdata_updated.csv`) com **3 classes**: `0 = OFF`, `1 = IRRIGATE`, `2 = THERMAL_STRESS_WET_SOIL` (`1.122` ocorrências com alta temperatura e solo encharcado).

### ADR-03 — Pipeline TinyML: MLP v1 $\rightarrow$ `PhysMLP-27` v2 Full INT8
* **Estado Anterior:** `train_tinyml_colab.ipynb` (`6e81d39`) com `SyntaxError`, sem deduplicação, `91,85%` de vazamento de tuplas `(MOI, temp, humidity)`, `class_weight="balanced"` derrubando precisão da Classe 2 para `34,25%`, ausência do modo `[0, C, 0]` e falsa quantização (`7.620 B` em Float32).
* **Gatilho:** Auditoria forense (`8834fef`) e benchmark experimental de 10 arquiteturas (`c85dfc0`).
* **Nova Arquitetura (`1c2de20`, `25f3463`):** `PhysMLP-27` (`27 -> 32 -> 16 -> 3`) com `4` features psicrométricas (VPD Tetens, demanda evaporativa, razões), *Full-8 Mask Context Dropout*, pesos calibrados `{0: 1.0, 1: 1.15, 2: 2.20}`, `GroupSplit` (0% vazamento), quantização **Full INT8 (`5.480 B`)** com `98,50%` de concordância e `6` *Golden Test Vectors* compiláveis.

### ADR-04 — Firmware: Arduino Monolítico $\rightarrow$ Recomeço Limpo em ESP-IDF v5.5
* **Estado Anterior:** `framework = arduino` com `WiFiManager` bloqueante, `analogRead()` sem calibração, `millis()` com overflow em 49,7 dias e `loop()` único.
* **Gatilho:** Conflitos de C++17/`flatbuffers` no TinyML, falta de determinismo em tempo real e risco de travamento da válvula por bloqueios de rede.
* **Nova Arquitetura (`6f6af13` $\rightarrow$ `f1f09a8`):** Reescrita completa sobre **PlatformIO + ESP-IDF v5.5.4 (C++17)** com `espressif/esp-tflite-micro` (ESP-NN), `adc_oneshot` + calibração eFuse, `esp_timer_get_time()`, `esp_task_wdt` (`30 s`) e drivers nativos (`Dht22Driver`).

### ADR-05 — Interface Embarcada & Provisionamento: Página Simples $\rightarrow$ SPA Gzip + Portal Cativo RFC 8910
* **Estado Anterior:** HTML simples sem compressão ou provisionamento dinâmico completo.
* **Nova Arquitetura (`c330580`, `1bc2c80`, `7f91017`, `1dc7ca3`, `b2a58c5`):** Compressão automática `view/index.html` $\rightarrow$ `data/index.html.gz` via `gzip_data.py`, `esp_http_server` + mDNS (`irrigacao.local`), abas de *Rendimento & Execução* e *Placa & Sistema*, varredura `/api/wifi/scan` e Portal Cativo RFC 8910 (`/captive-portal-api` + `dns_captive`).

### ADR-06 — Configuração & Logs: Hardcoded $\rightarrow$ NVS Versionado + `ConfigSanitizer` + LittleFS Anti-OOM
* **Nova Arquitetura (`f1f09a8`, `34b5c7d`, `8e755b8`):** `ConfigManager` sobre NVS (`schemaVersion = 2`) com sanitização isolada em `ConfigSanitizer.hpp` (testável em host) e `FileLogger` sobre LittleFS VFS com rotação em `180 KB` e leitura reversa sem carregar arquivos inteiros na heap.

### ADR-07 — Integração Sistêmica: Camadas Isoladas $\rightarrow$ Fluxo Ponta a Ponta
* **Estado Anterior:** Até `8291295`, o firmware operava isolado e `web/front/c.js` exibia dados simulados com `Math.random()`.
* **Nova Arquitetura (`34b5c7d`, `f6bb3c9`, `8399c50`):** `TelemetryClient` (`esp_http_client`) no ESP32 publica leituras e eventos de válvula em `POST /api/telemetry`, persistidos no PostgreSQL e consumidos pelo dashboard autenticado.

### ADR-08 — Segurança & Controle Manual: Rotas Abertas $\rightarrow$ Autenticação em Múltiplas Camadas
* **Nova Arquitetura (`e0335d1`, `083ecc0`, `f17c198`):** Remoção de fallback de `JWT_SECRET` em produção, `authMiddleware` (JWT) para usuário, `deviceAuthMiddleware` (`X-Device-Key`) para o ESP32, `X-Api-Pin` para rotas de escrita no `LocalWebServer`, rate limiting (`20 req / 15 min`), CORS configurável e `ActuatorManager::manualOverride()` com mutex e respeito a `maxIrrigationMs`.

### ADR-09 — Verificação: Sem Testes $\rightarrow$ Suítes em Host (C++ Unity) + API (`node --test`) + CI
* **Nova Arquitetura (`2d87467`, `f17c198`, `8e755b8`):** `7` testes unitários no backend (`web/back/tests/api.test.js`), CI no GitHub Actions (`.github/workflows/ci.yml`), `healthcheck` no Postgres e `3` suítes Unity em host (`embedded/test/run_host_tests.sh`) usando stubs do ESP-IDF.

### ADR-10 — Concorrência no Firmware: Dual-Core Fixado $\rightarrow$ FreeRTOS SMP Dinâmico
* **Estado Anterior:** `controlTask` monolítica fixada no Core 1 executava sensores, IA, atuação, gravação em Flash e atualização de snapshot; `LocalWebServer` e `TelemetryClient` fixados no Core 0.
* **Gatilho:** Desacoplar segurança hidráulica da latência de IA/Flash e aproveitar ambos os núcleos dinamicamente.
* **Nova Arquitetura (`8fbe408..3c5ac4c`):** Todas as `6` tasks com `tskNO_AFFINITY`, separação entre `actuator_safety` (prio `7`, `100 ms`) e `sensor_ai_task` (prio `6`, `250 ms`) via `g_actuatorCmdQueue`, `SystemStateStore` centralizado, `FileLogger` assíncrono (`logger_task` + fila de `24` slots) e `TelemetryClient` acordado por `xTaskNotifyGive`.
