# Análise de Lacunas Funcionais — Sistema de Irrigação Inteligente (PIBEX 2026)

- **Data da análise:** 2026-11-22
- **Escopo:** Repositório completo (`embedded/`, `web/back/`, `web/front/`, `ai-model/`, `docs/` e infraestrutura Docker)
- **Commit analisado:** `93ee4c1`

---

## Diagnóstico Atual

O repositório implementa um **Sistema de Irrigação Inteligente (PIBEX 2026 — IFPE Campus Paulista)** composto por três camadas: firmware embarcado em **ESP32 (PlatformIO + ESP-IDF v5.5, C++17, FreeRTOS Dual-Core, `esp-tflite-micro`, LittleFS, `esp_http_server`)**, pipeline de IA em **Python/TensorFlow (`PhysMLP-27` Full INT8)** e aplicação Web (**Node.js/Express + PostgreSQL + HTML/Tailwind/JS vanilla**). O projeto encontra-se em estágio de **MVP/protótipo funcional heterogêneo**: o pipeline TinyML (`ai-model/`) e o núcleo autônomo do firmware (`embedded/`) estão maduros — destacando-se a quantização INT8 real (`5.480 B`) sincronizada com `embedded/include/model_data.h`, validação por *Golden Test Vectors*, *fail-safe* de relé e portal cativo DNS/DHCP RFC 8910 —, enquanto a camada Web (`web/`) é apenas um esqueleto de autenticação desconectado do hardware e sem persistência real de telemetria. Fontes consultadas: `README.md`, documentação em `docs/`, manifestos (`platformio.ini`, `package.json`, `requirements.txt`, `docker-compose.yml`), código-fonte completo de `embedded/` e `web/`, scripts de `ai-model/` e `git log` (commit `93ee4c1`).

---

## Escopo e Limites da Análise

- **O que foi lido:** todos os módulos versionados do repositório (`embedded/`, `web/back/`, `web/front/`, `ai-model/scripts/train_tinyml_v2.py`, `docs/`, `docker-compose.yml`, `.env.example`, `README.md`) e histórico recente do Git.
- **O que ficou de fora:** binários e datasets brutos em `ai-model/datasets/`, imagens em `ai-model/output/figures/`, dependências gerenciadas (`embedded/managed_components/`, `node_modules/`) e testes físicos em bancada com hardware (ESP32, DHT22, HW-080 e relé).
- **Suposições assumidas:** contexto de projeto de extensão agrícola de baixo custo (*single-tenant* / pequeno produtor), priorizando confiabilidade embarcada, segurança básica e fechamento do fluxo ponta a ponta sem exigir alta disponibilidade em nuvem ou arquitetura distribuída na Fase 1.

---

## Lacunas Identificadas

| # | Prioridade | Tipo | Categoria | Lacuna | Evidência | Impacto (e probabilidade) | Esforço | Confiança |
| :-- | :-- | :-- | :-- | :-- | :-- | :-- | :-- | :-- |
| 1 | **Alta** | Bug | Segurança | `JWT_SECRET` possui fallback hardcoded no código e `docker-compose.yml` fixa senhas/chaves de dev | `web/back/services/authService.js:49`, `docker-compose.yml:10,28-29` | Qualquer pessoa com acesso ao repo pode forjar JWTs se a env faltar; alta probabilidade em deploy descuidado | P | Confirmado |
| 2 | **Alta** | Lacuna funcional | Segurança | Backend sem middleware de autenticação JWT nas rotas | `web/back/server.js:16-20`, `web/back/routes/authRoutes.js:1-8` | Impede proteger endpoints de telemetria, histórico e acionamento manual; pré-requisito direto do fluxo Web | P | Confirmado |
| 3 | **Alta** | Bug | Resiliência | Mutex do LittleFS (`m_logger.getMutex()`) fica travado durante todo o streaming HTTP de `index.html.gz` e CSVs | `embedded/src/net/LocalWebServer.cpp:112-131`, `embedded/src/net/LocalWebServer.cpp:555-579` | Cliente lento em Wi-Fi bloqueia `controlTask` (Core 1) ao gravar logs, podendo disparar *Task Watchdog Panic* (30s) e resetar o ESP32 | P | Confirmado |
| 4 | **Alta** | Bug | Resiliência | *Data race* no ADC1 durante calibração e ausência de validação de falha elétrica/colisão `dry == wet` | `embedded/src/net/LocalWebServer.cpp:523-547`, `embedded/src/drivers/SensorsManager.cpp:67-82` | Leitura concorrente entre Core 0 e Core 1 sem mutex; aceita ADC em curto/fio solto e retorna `{"ok":true}` mesmo quando `sanitize()` descarta o valor | P | Confirmado |
| 5 | **Alta** | Bug | Resiliência | `httpd_req_recv()` chamado apenas uma vez sem loop em `POST /api/config` e `POST /api/wifi` | `embedded/src/net/LocalWebServer.cpp:412`, `embedded/src/net/LocalWebServer.cpp:493` | Payloads TCP fragmentados chegam incompletos ao `cJSON_Parse`, quebrando salvamento de config/Wi-Fi em celulares | P | Confirmado |
| 6 | **Alta** | Bug | Dados | Alerta de estresse térmico reseta `lastLogMs` a cada 10s e impede gravação de `readings.csv` | `embedded/src/main.cpp:91-93`, `embedded/src/main.cpp:143-145` | Durante períodos quentes com solo úmido, o histórico de sensores deixa completamente de ser persistido | P | Confirmado |
| 7 | **Alta** | Lacuna funcional | Fluxo | Camada Web desconectada: sem endpoints de telemetria/histórico/válvula, dashboard usa `Math.random()` e botão manual é inerte | `web/back/server.js:16-20`, `web/back/schema.sql:15-31`, `web/front/c.js:124-138`, `web/front/index.html:72,105` | O produtor vê dados falsos e status `ESP32 Conectado` fixo; fluxo central do painel Web não funciona | M | Confirmado |
| 8 | **Alta** | Lacuna funcional | Fluxo | Firmware não possui cliente para sincronizar telemetria/eventos com `web/back` | `embedded/src/main.cpp:19-25`, `embedded/src/net/` | Mesmo quando conectado ao Wi-Fi STA, o ESP32 nunca envia leituras ao backend PostgreSQL; as camadas não se comunicam | M | Confirmado |
| 9 | **Média** | Bug | Fluxo | `ActuatorManager::getStatus()` calcula `cooldownRemainingMs` com default hardcoded e `/api/status` omite o campo | `embedded/src/control/ActuatorManager.cpp:194`, `embedded/src/net/LocalWebServer.cpp:219-224` | Tempo restante de cooldown fica incorreto sempre que `cooldownMs` é customizado na NVS | P | Confirmado |
| 10 | **Média** | Bug | Dados | E-mail não normalizado (`trim`/`toLowerCase`) e falta de validação de tipos em `/api/auth/*` | `web/back/controllers/authController.js:6-29`, `web/back/services/authService.js:6-40` | Permite duplicidade por maiúsculas/minúsculas e erros 500 com payloads malformados | P | Confirmado |
| 11 | **Média** | Lacuna funcional | Segurança | SoftAP aberto (`DEFAULT_AP_PASS = ""`) e rotas de escrita no ESP32 sem proteção (PIN/token) | `embedded/include/config.hpp:45`, `embedded/src/net/LocalWebServer.cpp:78-83` | Qualquer dispositivo na LAN ou alcance do SoftAP pode alterar calibração, timeout da válvula e credenciais Wi-Fi | P | Confirmado |
| 12 | **Média** | Lacuna funcional | Fluxo | Sem override manual seguro da válvula no firmware (`ActuatorManager` + `LocalWebServer` + `view/index.html`) | `embedded/src/control/ActuatorManager.hpp:10-15`, `embedded/src/net/LocalWebServer.cpp:64-86` | Produtor não consegue testar ou acionar/fechar a válvula manualmente mantendo o *timeout* hidráulico de segurança | P | Confirmado |
| 13 | **Média** | Bug | Fluxo | Frontend Web não valida JWT salvo no `localStorage` e vaza `setInterval` a cada ciclo login/logout | `web/front/c.js:104-152` | Token expirado/adulterado continua abrindo o dashboard; múltiplos timers acumulam requisições/atualizações | P | Confirmado |
| 14 | **Média** | Bug | Fluxo | Painel embarcado ignora falha HTTP em `saveConfig()`/`calibrate()` e omite campos `thermalStressThreshold` e `logIntervalMs` | `embedded/view/index.html:353-371` | Usuário recebe alerta de sucesso mesmo se a requisição falhar e não vê valores ajustados por `sanitize()` | P | Confirmado |
| 15 | **Média** | Lacuna funcional | Segurança | Backend sem restrição de CORS, sem rate limiting em `/api/auth/*` e sem fluxo de troca de senha | `web/back/server.js:13`, `web/back/routes/authRoutes.js:5-6` | Exposição a força bruta no login e impossibilidade de o produtor alterar senha comprometida | P | Confirmado |
| 16 | **Média** | Lacuna funcional | Testes | Ausência total de testes automatizados e CI (backend, firmware Unity e pipeline Python) | `web/back/package.json:7` e ausência de diretórios `test/` ou `.github/workflows/` | Regressões em autenticação, FSM, sanitização ou contrato TinyML passam despercebidas | M | Confirmado |
| 17 | **Média** | Lacuna funcional | Docs | Documentação de tarefas e rotas dessincronizada com a implementação real | `docs/planning/todo.md:98-113`, `docs/architecture/embedded-web-api.md:35-47` | Induz o time a assumir prontas entregas críticas que ainda não existem no código | P | Confirmado |
| 18 | **Baixa** | Melhoria | Operação | Dependência `firebase-admin` e `configs/firebase.js` não utilizados em nenhum ponto do backend | `web/back/package.json:19`, `web/back/configs/firebase.js:1-26` | Aumenta tempo de build e tamanho da imagem Docker sem uso funcional | P | Confirmado |
| 19 | **Baixa** | Bug | Resiliência | Leitura de `m_status.apActive` sem mutex e `scanNetworks()` não reagenda reconexão se o scan falhar offline | `embedded/src/net/NetworkManager.cpp:242-256`, `embedded/src/net/NetworkManager.cpp:372,423` | Pode atrasar ou travar reconexão STA automática após falha de varredura em modo offline | P | Confirmado |
| 20 | **Baixa** | Melhoria | Segurança | `logWifi(..., true)` interpola `net.ssid` via `innerHTML` sem escape na interface do ESP32 | `embedded/view/index.html:489` | XSS local caso o dispositivo conecte a um SSID contendo tags HTML; probabilidade baixa em uso rural | P | Confirmado |
| 21 | **Baixa** | Melhoria | Operação | `GET /api` não testa conexão com o PostgreSQL e `docker-compose.yml` não tem `healthcheck` no `db` | `web/back/server.js:18-20`, `docker-compose.yml:4-32` | Backend pode subir antes do Postgres aceitar conexões e reportar falso status operacional | P | Confirmado |

---

## Divergências Documentação × Código

- **Prometido como concluído (`[x]`) e não implementado:**
  - `docs/planning/todo.md:100,111`: marca `[x] Criação de página de troca de senha`, `[x] Criação de dashboard para monitoramento de métricas em tempo real e histórico`, `[x] Modularização de componentes e serviços (HTTP)` e `[x] Implementar middleware para rotas autenticadas e filtros de segurança` — nenhum desses itens existe de forma funcional em `web/`.
  - `docs/planning/todo.md:28-35`: ainda referencia bibliotecas antigas de Arduino (`WiFiManager`, `EspWebServer`, `ESPmDNS`, `StorageManager`, `config.h`/`utils.h`) já substituídas por ESP-IDF (`NetworkManager`, `LocalWebServer`, `FileLogger`, `config.hpp`/`TimeUtils`).
  - `docs/planning/todo.md:78`: aponta para `ai-model/train_tinyml_v2.py`, mas o script foi movido para `ai-model/scripts/train_tinyml_v2.py`.
  - `docs/planning/checklists.md:22,42`: cita `ThingSpeak` como nuvem IoT, enquanto o código implementado adota backend próprio em Node.js + PostgreSQL.
- **Implementado no código e ausente na documentação:**
  - `docs/architecture/embedded-web-api.md:35-47`: não documenta os endpoints já implementados em `LocalWebServer.cpp`: `GET /api/wifi/scan`, `GET /captive-portal-api` (RFC 8910) e as rotas de redirecionamento de portal cativo (`/generate_204`, `/hotspot-detect.html`, etc.).
- **Configuração desatualizada:**
  - `.env.example` e `web/back/.env.example` usam senha `postgres` na `DATABASE_URL`, enquanto `docker-compose.yml` define `postgrespassword`.

---

## Plano de Evolução

### Fase 1: Correções críticas e fechamento de fluxos
- **#1** Falhar no boot do `web/back` se `JWT_SECRET` estiver ausente em `NODE_ENV=production` (sem fallback hardcoded) e ler credenciais do `docker-compose.yml` via `.env`.
- **#2** Criar `web/back/middlewares/authMiddleware.js` validando `Authorization: Bearer <token>` com `jsonwebtoken` e um endpoint `GET /api/auth/me`.
- **#3** Reduzir o escopo do `m_logger.getMutex()` em `LocalWebServer.cpp` (trava curta por bloco lido, sem segurar o mutex durante `httpd_resp_send_chunk`), eliminando risco de *Task Watchdog Panic* no Core 1.
- **#4** Proteger `SensorsManager::readSoilAdcMedian()` com `SemaphoreHandle_t` e validar faixa válida + diferença mínima entre `soilAdcDry` e `soilAdcWet` antes de salvar e responder `200` na calibração.
- **#5** Criar helper `readRequestBody()` com loop acumulando `httpd_req_recv()` até `content_len` (tratando `HTTPD_SOCK_ERR_TIMEOUT`) para `handlePostConfig` e `handlePostWifi`.
- **#6** Separar `lastReadingsLogMs` e `lastInferenceLogMs` em `embedded/src/main.cpp` para que alertas térmicos imediatos não bloqueiem a gravação periódica de `readings.csv`.
- **#7** Implementar rotas REST em `web/back` (`POST/GET /api/telemetry`, `GET /api/irrigation/history`, `POST /api/irrigation/manual`) conectadas às tabelas de `schema.sql` (autenticadas por JWT para usuário e chave de dispositivo `X-Device-Key` para o ESP32) e conectar `web/front/c.js` a dados reais e ao botão `#toggleValveBtn`.
- **#8** Adicionar cliente HTTP nativo (`esp_http_client`, já incluso no ESP-IDF) em `embedded/src/net/` (task ou timer no Core 0) para publicar snapshots/eventos no backend configurado na NVS sem bloquear `controlTask`.

### Fase 2: Experiência e produtividade
- **#9** Passar `cfg.cooldownMs` (ou armazenar `m_activeCooldownMs` ao iniciar cooldown) em `ActuatorManager` e expor `cooldownRemainingMs` em `GET /api/status`.
- **#10** Normalizar `email = email.trim().toLowerCase()` e validar tipos/tamanhos de `nome`, `email` e `senha` no `authController`/`authService`.
- **#11** Definir senha WPA2 configurável para o SoftAP (ou derivada do MAC) e exigir PIN/token simples no header para rotas `POST /api/*` do `LocalWebServer`.
- **#12** Adicionar `manualOverride(bool open, const SystemConfig &cfg)` em `ActuatorManager` (respeitando `maxIrrigationMs` e `emergencyStop`) + endpoint `POST /api/actuator/manual` e botão na `embedded/view/index.html`.
- **#13** Validar sessão no `DOMContentLoaded` chamando `GET /api/auth/me` e armazenar/limpar o handle do `setInterval` no logout em `web/front/c.js`.
- **#14** Tratar `res.ok` com feedback visual sem `alert` em `saveConfig()`/`calibrate()`, recarregar `loadConfig()` após salvar e adicionar campos `thermalStressThreshold` e `logIntervalMs` em `embedded/view/index.html`.
- **#15** Configurar `CORS_ORIGIN` via env em `server.js`, adicionar rate limiting simples em memória (ou `express-rate-limit`) em `/api/auth/*` e criar endpoint autenticado `PATCH /api/auth/password`.
- **#16** Adicionar testes automatizados mínimos: `node --test` para `web/back` (auth + telemetria + casos de erro), testes Unity nativos em `embedded/test/` (sanitização NVS, FSM/Actuator timeout/cooldown, *Golden Vectors*) e workflow básico no GitHub Actions.
- **#17** Sincronizar `docs/planning/todo.md`, `docs/architecture/embedded-web-api.md`, `docs/planning/checklists.md` e `.env.example` com o estado real do código.

### Fase 3: Maturidade e escala
- **#18** Remover `firebase-admin` de `web/back/package.json` e apagar `web/back/configs/firebase.js` se não houver plano de uso imediato.
- **#19** Proteger todas as leituras de `m_status.apActive` com `m_mutex` e reagendar `m_reconnectTimer` em `NetworkManager::scanNetworks()` caso o scan falhe sem disparo de `STA_DISCONNECTED`.
- **#20** Substituir `innerHTML` por montagem segura de DOM (`createElement`/`textContent`) em `logWifi` (`embedded/view/index.html:489`).
- **#21** Atualizar `GET /api` para executar `SELECT 1` no PostgreSQL (retornando `503` se indisponível) e adicionar `healthcheck` no serviço `db` em `docker-compose.yml`.

---

## Pendências de Validação

Todos os **21 achados** deste relatório foram verificados diretamente no código-fonte e na documentação com confiança **Confirmado**. Não há achados classificados como **Hipótese** ou **Provável** pendentes de verificação estática.

> **Observação de bancada:** a validação funcional em hardware físico (sensor DHT22, HW-080, módulo relé e execução dos *Golden Test Vectors* no ESP32) permanece fora do escopo desta auditoria estática e deve ser executada após os ajustes da Fase 1.
