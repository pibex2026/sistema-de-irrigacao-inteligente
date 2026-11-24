# 01 — Linha do Tempo, Etapas de Desenvolvimento e Mapa de Commits

Este documento reconstrói a cronologia completa do projeto **Sistema de Irrigação Inteligente (PIBEX 2026 — IFPE Paulista)** a partir dos **`84` commits** registrados entre **`2026-07-14`** (`96e0cc2`) e **`2026-11-23`** (`3c5ac4c`).

---

## 1. Visão Geral da Distribuição Temporal

| Período | Fases | Commits | Foco Dominante |
| :--- | :--- | :---: | :--- |
| **14–17 Jul/2026** | Fase 1 | `15` | Fundação do repositório, protótipo Arduino (`SensorsManager`, `FileLogger`, `NetworkManager`) e esqueleto multi-camadas. |
| **18 Jul–20 Set/2026** | Fase 2 | `29` | Backend Node.js/Express + PostgreSQL, documentação inicial e empacotamento web embarcado (`gzip_data.py` + `partitions.csv`). |
| **21 Nov/2026 (Manhã/Tarde)** | Fase 3 | `15` | Auditoria forense de datasets, benchmark de 10 arquiteturas, pipeline TinyML v2 (`PhysMLP-27` INT8) e limpeza do firmware legado. |
| **21–22 Nov/2026** | Fase 4 | `10` | Recomeço limpo em **ESP-IDF v5.5**, observabilidade local (`LocalWebServer` + mDNS) e Portal Cativo RFC 8910. |
| **22–23 Nov/2026** | Fase 5 | `10` | Auditoria de 21 gaps, integração ponta a ponta (ESP32 $\leftrightarrow$ Backend $\leftrightarrow$ Frontend), hardening de segurança, testes e CI. |
| **23 Nov/2026 (Noite)** | Fase 6 | `5` | Migração do firmware para **FreeRTOS SMP (`tskNO_AFFINITY`)**, `SystemStateStore`, `FileLogger` assíncrono e `TelemetryClient` com Task Notification. |

---

## 2. Detalhamento das 6 Fases de Desenvolvimento

### Fase 1 — Fundação do Repositório e Protótipo Arduino
* **Período:** `2026-07-14` a `2026-07-17` | **Commits:** `96e0cc2..617af95` (`15` commits)
* **Objetivo:** Estruturar o repositório multi-camadas e criar a primeira base embarcada sobre PlatformIO + Arduino.
* **Entregas & Arquivos-Chave:**
  - Configuração inicial de `.gitignore` (`ecaf1fa`, `ee28788`) e `embedded/platformio.ini` com bibliotecas `DHT` e `WiFiManager` (`0f61be6`), habilitando C++17 (`14b1402`).
  - Esqueleto de diretórios (`e7dd58a`) com placeholders `ai-model/a.py`, `web/back/a.go` e `web/front/c.js`.
  - Primeiras camadas do firmware Arduino: `SensorsManager` (`5ee2e37`, `c755b93`, `43bf775`), `Utils` para timestamp (`f32c6c8`), `config.h` / `constants.h` (`ce3da99`, `c146e22`), `FileLogger` (`07e2163`) e `NetworkManager` (`30957ad`, `617af95`).

| Commit | Data | Resumo |
| :--- | :--- | :--- |
| `96e0cc2` | 2026-07-14 | `Initial commit` |
| `0f61be6` | 2026-07-14 | Adição das bibliotecas `DHT` e `WiFiManager` no `platformio.ini` |
| `e7dd58a` | 2026-07-14 | Inicialização dos diretórios `ai-model/`, `web/back/` e `web/front/` |
| `14b1402` | 2026-07-17 | Habilitação de C++17 no firmware |
| `07e2163` / `617af95` | 2026-07-17 | Criação das camadas iniciais de `FileLogger` e `NetworkManager` |

---

### Fase 2 — Backend Node.js/PostgreSQL, Pesquisa e Empacotamento Web Embarcado
* **Período:** `2026-07-18` a `2026-09-20` | **Commits:** `59ec21e..540c17c` (`29` commits)
* **Objetivo:** Construir a API inicial de autenticação em Node.js/PostgreSQL, organizar a pesquisa bibliográfica e viabilizar a interface web comprimida no LittleFS do ESP32.
* **Entregas & Arquivos-Chave:**
  - **Backend (`web/back/`):** Conexão PostgreSQL (`59ec21e`), `userModel.js` (`baac353`), `authService.js` (`3932bcd`), `authController.js` (`2d592c2`), `authRoutes.js` (`4a5dec2`), `server.js` + `package.json` (`3954953`), remoção do placeholder `a.go` (`3002086`) e adição de `configs/firebase.js` (`4d647b2`).
  - **Documentação:** `CHECKLISTS.md` (`8821260`), `TODO.md` (`4c44b5b`), artigos no `README.md` (`2a78f17`) e criação da pasta `docs/` (`22a1071`, `3361e8c`).
  - **Web Embarcada & Partições:** `embedded/partitions.csv` (`ed88ba9`, `8ea26c9`), script `embedded/scripts/gzip_data.py` (`610532b`, `e6fc07a`) e reformulação da geração de páginas estáticas (`c330580`, `bdeacbf`: `view/index.html` $\rightarrow$ `data/index.html.gz`).

| Commit | Data | Resumo |
| :--- | :--- | :--- |
| `59ec21e..3954953` | 2026-07-18 | Estruturação completa do backend Express + PostgreSQL + Auth |
| `3002086` | 2026-07-18 | Remoção do arquivo antigo em Go (`web/back/a.go`) |
| `22a1071` / `3361e8c` | 2026-07-27..28 | Organização de artigos, datasets e `TODO.md` em `docs/` |
| `ed88ba9` / `8ea26c9` | 2026-07-30 | Definição da tabela de partições do ESP32 (`partitions.csv`) |
| `c330580` / `bdeacbf` | 2026-08-04 | Reformulação da compressão pré-upload (`view/index.html` $\rightarrow$ `index.html.gz`) |

---

### Fase 3 — Avaliação Forense de Datasets e Evolução TinyML v1 $\rightarrow$ v2
* **Período:** `2026-11-21` | **Commits:** `c3366f3..25f3463` (`15` commits)
* **Objetivo:** Selecionar cientificamente o dataset oficial, auditar o primeiro modelo TinyML, corrigir falhas estatísticas e consolidar o modelo **`PhysMLP-27` Full INT8**.
* **Entregas & Arquivos-Chave:**
  - Levantamento de artigos, avaliação de 4 datasets (com prova MD5 e descoberta da Classe 2 `THERMAL_STRESS_WET_SOIL`) e plano de ação (`c3366f3`).
  - Dockerização do backend + frontend v1 (`a9f628c`, `d3afa56`).
  - Primeiro notebook `train_tinyml_colab.ipynb` (`6e81d39`), seguido por auditoria forense identificando **9 bugs críticos** (`8834fef`) e benchmark de **10 arquiteturas** com **12 pranchas gráficas** (`c85dfc0`).
  - Pipeline v2 (`1c2de20`, `d472dea`, `25f3463`): `train_tinyml_v2.py` gerando `model_int8.tflite` (`5.480 B`) e `embedded/include/model_data.h` com `GOLDEN_VECTORS[6]` e suporte a *fine-tuning* de campo.
  - Limpeza intencional dos arquivos antigos Arduino em `embedded/` (`6f6af13`) preparando o recomeço em ESP-IDF.

| Commit | Data | Resumo |
| :--- | :--- | :--- |
| `c3366f3` | 2026-11-21 | Avaliação de datasets, artigos e criação do plano de ação |
| `a9f628c` | 2026-11-21 | Dockerização do backend PostgreSQL e interface web v1 |
| `8834fef` / `c85dfc0` | 2026-11-21 | Auditoria estatística (9 bugs) e benchmark de 10 arquiteturas |
| `1c2de20` / `25f3463` | 2026-11-21 | Pipeline TinyML v2 (`PhysMLP-27` INT8 `5.480 B`) e `model_data.h` |
| `6f6af13` | 2026-11-21 | Limpeza dos arquivos legados Arduino em `embedded/` |

---

### Fase 4 — Recomeço Limpo em ESP-IDF v5.5, Observabilidade Local e Portal Cativo
* **Período:** `2026-11-21` a `2026-11-22` | **Commits:** `f1f09a8..93ee4c1` (`10` commits)
* **Objetivo:** Reescrever o firmware do zero em **PlatformIO + ESP-IDF v5.5 (C++17 + FreeRTOS)**, entregar interface local rica e provisionamento Wi-Fi por portal cativo.
* **Entregas & Arquivos-Chave:**
  - Bootstrap ESP-IDF (`f1f09a8`, `29` arquivos): `CMakeLists.txt`, `idf_component.yml` (`esp-tflite-micro`, `littlefs`, `mdns`), `sdkconfig.defaults`, `ConfigManager` (NVS), `TimeUtils`, `Dht22Driver`, `SensorsManager` (ADC1 Oneshot + eFuse), `ModelManager`, `ActuatorManager`, `FileLogger` (LittleFS VFS), `NetworkManager`.
  - `LocalWebServer` (`esp_http_server` + mDNS `irrigacao.local`) com endpoints JSON, streaming CSV e abas *Rendimento & Execução* e *Placa & Sistema* (`1bc2c80`, `7f91017`).
  - Reorganização estrutural de `ai-model/` (`3cf8626`) e `docs/` em subdiretórios temáticos (`c560f06`).
  - Portal cativo RFC 8910, servidor DNS UDP (`dns_captive`) e `/api/wifi/scan` (`1dc7ca3`, `b2a58c5`).

| Commit | Data | Resumo |
| :--- | :--- | :--- |
| `f1f09a8` | 2026-11-21 | Configuração completa do projeto ESP-IDF v5.5 com CMake e módulos |
| `1bc2c80` / `7f91017` | 2026-11-21 | Implementação e expansão do `LocalWebServer` e `view/index.html` |
| `3cf8626` / `c560f06` | 2026-11-21 | Reorganização de `ai-model/` e `docs/` em subdiretórios temáticos |
| `1dc7ca3` / `b2a58c5` | 2026-11-22 | Portal cativo RFC 8910, DNS UDP e varredura Wi-Fi |

---

### Fase 5 — Auditoria de Lacunas, Integração Ponta a Ponta, Hardening e Testes
* **Período:** `2026-11-22` a `2026-11-23` | **Commits:** `8291295..8e755b8` (`10` commits)
* **Objetivo:** Diagnosticar lacunas entre camadas e fechar o fluxo completo de telemetria, segurança, testes automatizados e CI.
* **Entregas & Arquivos-Chave:**
  - Auditoria estática documentando **21 lacunas** (`8291295`: `docs/gap-analysis.md` e `functional-gap-planner.md`).
  - Integração E2E: `.env` + `DEVICE_API_KEY` (`e0335d1`), `TelemetryClient` no ESP32 + correções de mutex/ADC/body loop/logs (`34b5c7d`), API REST de telemetria + `authMiddleware` + `deviceAuthMiddleware` (`f6bb3c9`), dashboard real no frontend (`8399c50`, `678cfb0`).
  - Hardening: controle manual seguro da válvula + `X-Api-Pin` + mutex no `ActuatorManager` (`083ecc0`), rate limiting + CORS configurável + `PATCH /api/auth/password` + healthcheck DB (`f17c198`), fix de `apActive` e scan offline (`1456ed1`).
  - Verificação: CI GitHub Actions + healthcheck Postgres no `docker-compose.yml` (`2d87467`), `7` testes em `node --test` (`f17c198`) e `3` suítes Unity em host com `ConfigSanitizer.hpp` (`8e755b8`).

| Commit | Data | Resumo |
| :--- | :--- | :--- |
| `8291295` | 2026-11-22 | Análise de lacunas funcionais (21 gaps confirmados) |
| `34b5c7d` / `f6bb3c9` / `8399c50` | 2026-11-23 | Integração ponta a ponta: ESP32 `TelemetryClient` $\rightarrow$ Backend $\rightarrow$ Frontend |
| `083ecc0` / `f17c198` | 2026-11-23 | Hardening: `X-Api-Pin`, `manualOverride`, rate limit, CORS e testes API |
| `2d87467` / `8e755b8` | 2026-11-23 | Workflow de CI e testes unitários em host (Unity C++ + stubs) |

---

### Fase 6 — Arquitetura FreeRTOS SMP, Estado Centralizado e I/O Assíncrono
* **Período:** `2026-11-23` | **Commits:** `8fbe408..3c5ac4c` (`5` commits)
* **Objetivo:** Eliminar acoplamento de núcleo único (`Core 1` monolítico) e bloqueios de I/O em Flash/rede, adotando escalonamento **SMP dinâmico (`tskNO_AFFINITY`)**.
* **Entregas & Arquivos-Chave:**
  - `SystemStateStore` centralizado, constantes `Config::Tasks` e `ActuatorCommand` (`8fbe408`).
  - `FileLogger` assíncrono com fila de `24` slots em RAM + `logger_task` (`7145eb0`).
  - Migração de `LocalWebServer`, `TelemetryClient` (`xTaskNotifyGive`/`ulTaskNotifyTake`) e `NetworkManager` para SMP e `SystemStateStore` (`19a7a98`).
  - Separação de `actuator_safety` (prio `7`, `100 ms`) e `sensor_ai_task` (prio `6`, `250 ms`) comunicando-se via `g_actuatorCmdQueue` (`0dfe3dd`).
  - Atualização da documentação arquitetural e criação do resumo executivo (`3c5ac4c`).
