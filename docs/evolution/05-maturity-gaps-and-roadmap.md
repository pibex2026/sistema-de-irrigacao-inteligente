# 05 — Matriz de Maturidade, Rastreamento de Lacunas e Roadmap

Este documento consolida a avaliação de maturidade atual do projeto (`3c5ac4c`), rastreia o fechamento das **21 lacunas** identificadas em [gap-analysis.md](../gap-analysis.md) (`93ee4c1`) e lista as dívidas técnicas e próximos passos.

---

## 1. Matriz de Maturidade por Subsistema

| Subsistema | Maturidade Atual | Evidência Principal |
| :--- | :--- | :--- |
| **Pipeline TinyML (`ai-model/`)** | **Maduro (Validado em Software)** | `PhysMLP-27` INT8 (`5.480 B`), `98,20%` Acc M1, `97,43%` GroupSplit, `6` *Golden Vectors*. |
| **Núcleo Firmware ESP-IDF SMP (`embedded/`)** | **Funcional + Testado em Host** | `6` tasks SMP (`tskNO_AFFINITY`), `SystemStateStore`, `FileLogger` assíncrono, `3` suítes Unity em host. |
| **Conectividade & Portal Cativo (`NetworkManager`)** | **Funcional (Revisão Pendente)** | STA/SoftAP, backoff, SNTP, RFC 8910 e `/api/wifi/scan`; revisão funcional do `dns_captive` pendente. |
| **Backend REST & PostgreSQL (`web/back/`)** | **Maduro (Testado + CI)** | Auth JWT, `X-Device-Key`, rate limit, CORS, `SELECT 1` healthcheck, `7` testes `node --test` no CI. |
| **Frontend Web (`web/front/`)** | **Funcional (MVP)** | Login/cadastro, sessão `/api/auth/me`, telemetria real e controle manual; falta UI de troca de senha. |
| **Hardware Físico & Energia Solar** | **Pendente de Bancada / Campo** | Protocolo de 8 testes em bancada e perfil energético (`WIFI_PS_MIN_MODEM` / `deep_sleep`) por validar. |

---

## 2. Auditoria de Fechamento dos 21 Gaps (`gap-analysis.md`: `93ee4c1` $\rightarrow$ `3c5ac4c`)

| # | Lacuna Original (`93ee4c1`) | Status | Commit(s) | Observação Técnica |
| :-: | :--- | :---: | :--- | :--- |
| **1** | `JWT_SECRET` hardcoded e credenciais fixas no Compose | **Resolvido** | `e0335d1`, `f6bb3c9` | `validateEnv()` falha em produção sem `JWT_SECRET`; Compose lê `.env`. |
| **2** | Backend sem middleware de autenticação JWT | **Resolvido** | `f6bb3c9` | `authMiddleware.js` + `GET /api/auth/me` implementados. |
| **3** | Mutex do LittleFS travado durante streaming HTTP | **Resolvido** | `34b5c7d`, `7145eb0` | `readFileChunk()` usa trava curta e `FileLogger` escreve via fila assíncrona. |
| **4** | *Data race* no ADC1 e falta de validação na calibração | **Resolvido** | `34b5c7d` | `SensorsManager` protege ADC1 com mutex e valida faixas/diferença `dry != wet`. |
| **5** | `httpd_req_recv()` sem loop em `POST /api/config` e `/wifi` | **Resolvido** | `34b5c7d` | Helper `readRequestBody()` lê em loop até `content_len`. |
| **6** | Alerta térmico resetando `lastLogMs` de `readings.csv` | **Resolvido** | `34b5c7d`, `0dfe3dd` | Separados `lastReadingsLogMs` e `lastInferenceLogMs`. |
| **7** | Camada Web desconectada (dados `Math.random()`) | **Resolvido** | `f6bb3c9`, `8399c50` | Endpoints `/api/telemetry` e `/api/irrigation/*` conectados ao dashboard. |
| **8** | Firmware sem cliente para sincronizar com `web/back` | **Resolvido** | `34b5c7d`, `19a7a98` | `TelemetryClient` publica snapshots/eventos via `esp_http_client`. |
| **9** | `cooldownRemainingMs` hardcoded e omitido em `/api/status` | **Resolvido** | `34b5c7d` | `m_activeCooldownMs` armazenado e exposto em `/api/status`. |
| **10** | E-mail não normalizado e falta de validação de tipos | **Resolvido** | `f17c198` | `trim().toLowerCase()` e validação de tipos/tamanhos em `authService.js`. |
| **11** | SoftAP aberto e rotas de escrita no ESP32 sem proteção | **Resolvido** | `083ecc0` | Rotas de escrita exigem header `X-Api-Pin`. |
| **12** | Sem override manual seguro da válvula no firmware | **Resolvido** | `083ecc0` | `ActuatorManager::manualOverride()` + `POST /api/actuator/manual`. |
| **13** | Frontend sem validar JWT no boot e vazando `setInterval` | **Resolvido** | `8399c50` | `DOMContentLoaded` chama `/api/auth/me`; `telemetryIntervalId` limpo no logout. |
| **14** | UI embarcada ignorava falhas HTTP e omitia campos | **Resolvido** | `083ecc0` | Tratamento de `res.ok` e campos `thermalStressThreshold` / `logIntervalMs`. |
| **15** | Sem CORS configurável, rate limit e troca de senha | **Resolvido** | `f17c198` | `CORS_ORIGIN`, `rateLimitMiddleware.js` e `PATCH /api/auth/password`. |
| **16** | Ausência total de testes automatizados e CI | **Parcial** | `2d87467`, `f17c198`, `8e755b8` | Backend (`node --test`) está no CI; testes Unity C++ (`run_host_tests.sh`) ainda fora do CI. |
| **17** | Documentação dessincronizada com o código | **Resolvido** | `c5f91e3`, `3c5ac4c` | `todo.md`, `embedded-web-api.md` e `firmware-espidf.md` atualizados. |
| **18** | `firebase-admin` e `configs/firebase.js` sem uso | **Aberto** | — | Dependência e arquivo ainda presentes em `web/back/`. |
| **19** | Leitura de `apActive` sem mutex e scan offline | **Resolvido** | `1456ed1` | `apActive` protegido e reconexão reagendada após falha de scan. |
| **20** | XSS potencial em `logWifi` via `innerHTML` | **Resolvido** | `b2a58c5` / `083ecc0` | `logWifi` em `embedded/view/index.html` usa `createElement` + `textContent`. |
| **21** | `GET /api` sem testar Postgres e Compose sem `healthcheck` | **Resolvido** | `2d87467`, `f17c198` | `GET /api` executa `SELECT 1` e `docker-compose.yml` usa `pg_isready`. |

---

## 3. Dívidas Técnicas e Lacunas Atuais Identificadas

1. **Consumo de `pending_command` no `TelemetryClient`:** `web/back/services/telemetryService.js` já retorna `pending_command` na resposta de `POST /api/telemetry`, mas `TelemetryClient::publishSnapshot()` ainda não faz o *parse* dessa resposta para acionar `ActuatorManager::manualOverride()`.
2. **Revisão funcional do DNS Captive Portal (`NetworkManager` / `dns_captive`):** Pendente de validação em campo/dispositivos móveis (registrado em [todo.md](../planning/todo.md)).
3. **Frontend Web (`web/front/`):** Falta interface para troca de senha (`PATCH /api/auth/password`) e modularização do arquivo `c.js`.
4. **CI Unificado:** Incluir `bash embedded/test/run_host_tests.sh` e checagem de sintaxe Python (`ai-model/`) no `.github/workflows/ci.yml`.
5. **Limpeza Residual:** Remover `firebase-admin` / `web/back/configs/firebase.js` (Gap #18) e arquivar/remover `functional-gap-planner.md` da raiz.
6. **Validação em Bancada Física & Energia Solar:** Executar os 8 testes de aceitação de [action-plan.md](../planning/action-plan.md), validar perfil energético (`WIFI_PS_MIN_MODEM` / `deep_sleep` + chaveamento MOSFET para o HW-080) e iniciar ciclo de *fine-tuning* com `inference.csv` real em Paulista-PE.

---

## 4. Roadmap Recomendado

* **Curto Prazo (Fechamento de Software & CI):**
  1. Integrar `pending_command` no `TelemetryClient` do ESP32.
  2. Adicionar `embedded/test/run_host_tests.sh` ao `.github/workflows/ci.yml`.
  3. Remover `firebase-admin` / `configs/firebase.js` e adicionar UI de troca de senha em `web/front/`.
* **Médio Prazo (Bancada Física & Conectividade):**
  1. Executar os 8 testes físicos em bancada (DHT22, HW-080, relé, *Golden Self-Test*, fail-safe, rotação LittleFS).
  2. Validar e corrigir o comportamento do `dns_captive` em Android/iOS.
  3. Medir consumo energético com `WIFI_PS_MIN_MODEM` e dimensionar bateria/painel solar.
* **Longo Prazo (Campo & Ciclo Contínuo de IA — PIBEX 2026):**
  1. Implantar protótipo piloto em lavoura comunitária de Paulista-PE.
  2. Coletar logs `/littlefs/inference.csv` reais e executar *fine-tuning* incremental via `train_tinyml_v2.py --field-inferences`.
