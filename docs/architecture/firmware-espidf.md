# Arquitetura do Firmware ESP32 (PlatformIO + ESP-IDF v5.5)

> **Contexto:** Especificação técnica consolidada do firmware embarcado reescrito do zero em **C++17** sobre **ESP-IDF v5.5.4** (gerenciado via **PlatformIO**), substituindo a antiga base Arduino por arquitetura determinística **FreeRTOS Dual-Core**.

---

## 1. Stack, Build e Particionamento

* **Plataforma:** `espressif32` (`pioarduino` `55.3.39`) | **Placa:** `esp32doit-devkit-v1` (ESP32 Xtensa Dual-Core LX6, 240 MHz, 4 MB Flash).
* **Framework:** `espidf` (`framework-espidf @ 3.50504.0`, ESP-IDF v5.5.4) com `toolchain-xtensa-esp-elf @ 14.2.0` e `-std=gnu++17`.
* **Dependências via IDF Component Manager (`embedded/src/idf_component.yml`):**
  - `espressif/esp-tflite-micro` (`^1.3.0`) — Runtime TinyML com kernels **ESP-NN**.
  - `joltwallet/littlefs` (`^1.14.0`) — Sistema de arquivos POSIX `/littlefs` sobre a partição `spiffs`.
  - `espressif/mdns` (`^1.4.0`) — Resolução local `http://irrigacao.local`.
* **Configurações de Build (`embedded/sdkconfig.defaults`):**
  - Tabela de partições customizada (`partitions.csv`): `nvs` (`20 KB`), `otadata` (`8 KB`), `phy_init` (`4 KB`), `app0` (`1.769.472 B`), `app1` (`1.769.472 B`), `spiffs/LittleFS` (`512 KB`).
  - `CONFIG_ESP_TASK_WDT_EN=y` (`30s`, panic on timeout), `CONFIG_FREERTOS_HZ=1000`, `CONFIG_ESP_MAIN_TASK_STACK_SIZE=8192`, `CONFIG_COMPILER_OPTIMIZATION_SIZE=y`, sem exceptions/RTTI.

### Orçamento de Memória Verificado (`pio run`)
* **RAM Estática:** **`51.152` / `327.680` bytes (`15,6%`)** — DRAM usada `28,3%` (`129.584 B` livres antes das alocações dinâmicas).
* **Flash (`app0`):** **`944.247` / `1.769.472` bytes (`53,4%`)** — folga superior a `825 KB` mantendo OTA funcional.
* **LittleFS (`littlefs.bin`):** `view/index.html` comprimido via `scripts/gzip_data.py` para `data/index.html.gz`.

---

## 2. Organização Modular (`embedded/`)

```text
embedded/
├── include/
│   ├── config.hpp        # Pinagem, polaridade do rele, NTP, limites LittleFS e defaults
│   ├── types.hpp         # Contratos (SystemState, SensorData, SystemConfig, PerformanceStats, SystemSnapshot)
│   └── model_data.h      # Modelo PhysMLP-27 INT8 (5.480 B) + helpers C++17 + GOLDEN_VECTORS[6]
└── src/
    ├── main.cpp          # app_main(), bootstrap e controlTask (Core 1 + FSM + Task WDT)
    ├── core/
    │   ├── ConfigManager # NVS (namespace "irrig_cfg", schema v1, sanitizacao de faixas)
    │   └── TimeUtils     # Timestamp ISO-8601 (localtime_r) + fallback offline UPTIME_<seg>s
    ├── drivers/
    │   ├── Dht22Driver   # Protocolo 1-wire GPIO 5 (portMUX_TYPE, checksum, intervalo >= 2s)
    │   └── SensorsManager# DHT22 + HW-080 (ADC1_CH4 GPIO 32, ADC_ATTEN_DB_12, mediana 10x)
    ├── ai/
    │   └── ModelManager  # esp-tflite-micro INT8, arena 12 KB, sentinelas, fallback termico
    ├── control/
    │   └── ActuatorManager # Rele GPIO 25 (RELAY_ACTIVE_LOW), LED GPIO 2, timeout/cooldown/debounce
    ├── storage/
    │   └── FileLogger    # /littlefs/readings.csv e inference.csv, mutex, rotacao 180 KB, anti-OOM
    └── net/
        ├── NetworkManager# esp_wifi STA + SoftAP temporario, backoff esp_timer, SNTP BRT3, offline
        └── LocalWebServer# esp_http_server + cJSON + mDNS + streaming CSV
```

---

## 3. Concorrência FreeRTOS e Máquina de Estados (FSM)

| Núcleo | Tarefa / Módulo | Responsabilidade |
| :--- | :--- | :--- |
| **Core 1 (`APP_CPU`)** | `controlTask` (stack `8192 B`, prio `5`, tick `250 ms`) | Alimenta `esp_task_wdt`, executa `ActuatorManager::update()` a cada `250 ms` (garantindo corte exato por *timeout*) e conduz a FSM. |
| **Core 0 (`PRO_CPU`)** | `NetworkManager` + `LocalWebServer` | Gerencia eventos Wi-Fi/IP, SNTP e requisições HTTP sem jamais bloquear a leitura de sensores ou o fechamento da válvula. |

### Fluxo da FSM (`SystemState`)
1. **`INIT`:** Inicializa `ModelManager` (`esp-tflite-micro` INT8) e executa `runGoldenSelfTest()`. Em caso de falha do modelo $\rightarrow$ `ERROR`.
2. **`READ_SENSORS`:** No intervalo configurado (`sensorReadIntervalMs`), lê DHT22 (`temperatureX10` em `int16_t` com sentinela `-999`) e HW-080 (mediana de 10 amostras ADC1, detectando curto `<=10` ou fio solto `>=4090`). Após `3` falhas consecutivas $\rightarrow$ `ERROR`. Se válido, grava `readings.csv` no intervalo `logIntervalMs` e segue para `RUN_INFERENCE`.
3. **`RUN_INFERENCE`:** Executa `ModelManager::predict()` e atualiza estatísticas de latência/decisão (`PerformanceStats`).
4. **`EXECUTE_ACTION`:** Aplica `ActuatorManager::applyDecision()` (abertura exige `2` confirmações seguidas; fechamento é imediato), grava `inference.csv` (no intervalo `logIntervalMs` ou imediatamente em alerta térmico), atualiza `SystemSnapshot` e retorna para `READ_SENSORS`.
5. **`ERROR`:** Chama `ActuatorManager::emergencyStop()` (fecha válvula imediatamente + pisca rápido no LED), aguarda próximo intervalo e tenta recuperação automática.
