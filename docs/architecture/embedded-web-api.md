# Interface Web Local, Telemetria e Endpoints HTTP (`LocalWebServer`)

> **Contexto:** Documentação da camada de observabilidade, configuração local e coleta de logs embarcada no ESP32 via `esp_http_server` + `espressif/mdns`.

---

## 1. Acesso e Empacotamento

* **Endereços locais:**
  - Via rede local (mDNS): `http://irrigacao.local`
  - Via IP atribuído pelo roteador (STA) ou via SoftAP de provisionamento: `http://192.168.4.1` (SSID padrão: `ESP-Irrigacao`).
* **Empacotamento:** `embedded/view/index.html` é comprimido automaticamente em `data/index.html.gz` pelo script `embedded/scripts/gzip_data.py` durante `pio run -t buildfs` / `uploadfs` e servido diretamente do LittleFS com header `Content-Encoding: gzip`.

---

## 2. Páginas da Interface Web (`embedded/view/index.html`)

1. **Visão Geral & Config:**
   - Telemetria ao vivo (DHT22, umidade do solo, ADC bruto, decisão da IA, probabilidades `[P0, P1, P2]`, estado da válvula e latência).
   - Formulário persistido na NVS (`ConfigManager`): `soilType`, `cropId`, `seedlingStage`, limiar de confiança, *timeout* da válvula, *cooldown* e intervalo de leitura.
   - Calibração instantânea de solo seco/úmido (`POST /api/calibrate/dry` e `wet`), provisionamento Wi-Fi e download dos CSVs.
2. **Rendimento & Execução:**
   - Contadores de inferências (`total`, `OFF`, `IRRIGATE`, `THERMAL_STRESS`), total de leituras vs erros de sensor, acionamentos físicos da válvula, latência (`última`, `média`, `mínima`, `máxima` em µs), arena TFLM utilizada e *stack* livre da `controlTask` (Core 1).
   - Tabela em tempo real com as **últimas 15 inferências** gravadas em `/littlefs/inference.csv`.
3. **Placa & Sistema:**
   - Identificação do chip (`ESP32 Xtensa Dual-Core LX6`), núcleos, revisão de silício, MAC, versão do ESP-IDF, motivo do último reset e uptime.
   - Clock da CPU e XTAL em MHz (`esp_clk_tree`).
   - Temperatura (nota técnica sobre ausência de sensor interno oficial no ESP32 clássico no IDF v5.5 + leitura ambiente do DHT22).
   - Uso detalhado de Memória RAM (Heap total, usada, livre, mínimo histórico e maior bloco contíguo) e Armazenamento (Flash total, partição OTA ativa, ocupação do LittleFS e tamanho de `readings.csv`, `inference.csv` e backups `.prev.csv`).

---

## 3. Contrato de Endpoints HTTP

| Método & Rota | Formato | Descrição |
| :--- | :---: | :--- |
| `GET /` | `text/html` (`gzip`) | Serve `/littlefs/index.html.gz` (com fallback HTML mínimo se não montado). |
| `GET /api/status` | `application/json` | Snapshot atual: sensores, última inferência, atuador e status Wi-Fi/SNTP. |
| `GET /api/performance` | `application/json` | `PerformanceStats` + array `recentInferences` (últimos 15 registros do CSV). |
| `GET /api/system` | `application/json` | Diagnóstico de placa, clock CPU/XTAL, temperatura, memória Heap e armazenamento Flash/LittleFS. |
| `GET /api/config` | `application/json` | Configuração atual persistida na NVS (`SystemConfig`). |
| `POST /api/config` | `application/json` | Atualiza e sanitiza parâmetros de contexto, limiares, calibração e temporização na NVS. |
| `POST /api/wifi` | `application/json` | Recebe `{"ssid","password"}` e inicia conexão STA assíncrona. |
| `POST /api/calibrate/dry` | `application/json` | Captura mediana atual do ADC1 e salva como `soilAdcDry` na NVS. |
| `POST /api/calibrate/wet` | `application/json` | Captura mediana atual do ADC1 e salva como `soilAdcWet` na NVS. |
| `GET /logs/readings.csv` | `text/csv` | Download por *chunked streaming* (`1024 B`) do histórico de leituras dos sensores. |
| `GET /logs/inference.csv` | `text/csv` | Download por *chunked streaming* (`1024 B`) do histórico de inferências e acionamentos. |
