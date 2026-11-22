# Sistema de Irrigação Inteligente - PIBEX 2026 - IFPE Paulista

Solução tecnológica e sustentável para automação agrícola de baixo custo, integrando **Internet das Coisas (IoT)**, **Inteligência Artificial Embarcada (TinyML)** e **Painel Web de Telemetria** para apoiar pequenos agricultores locais.

---

## 📌 Descrição do Projeto

O objetivo deste projeto é desenvolver um ecossistema integrado de monitoramento ambiental e acionamento inteligente de irrigação. Utilizando microcontroladores ESP32 alimentados por energia solar, o sistema monitora temperatura ambiente, umidade do ar e umidade do solo, tomando decisões autônomas no próprio hardware (*Edge Computing / TinyML*) para otimizar o uso da água e evitar desperdícios em lavouras comunitárias.

---

## 👥 Integrantes

* Projeto Institucional de Extensão (PIBEX 2026) - IFPE Campus Paulista

---

## 🛠️ Arquitetura do Repositório

```text
├── embedded/          # Firmware ESP32 (PlatformIO + ESP-IDF v5.5, FreeRTOS, LittleFS e esp_http_server)
├── ai-model/          # Pipeline de IA (Smart Agriculture Dataset, PhysMLP-27, quantização INT8 e TinyML)
├── web/
│   ├── back/          # API REST Node.js / Express com autenticação JWT e PostgreSQL
│   └── front/         # Interface Web de telemetria e controle do produtor
├── docs/              # Levantamento bibliográfico, datasets e planejamento
└── docker-compose.yml # Orquestração do ambiente backend + PostgreSQL
```

---

## ⚡ Hardware Utilizado

### Microcontrolador Escolhido: ESP32

* **Processador:** CPU Xtensa Dual-Core 32-bit LX6 com clock de até 240 MHz.
* **Memória:** 520 KB de SRAM interna e 4 MB de Flash SPI externa.
* **Conectividade:** Wi-Fi 802.11 b/g/n (2.4 GHz) e Bluetooth v4.2 / BLE.
* **Periféricos:** 2x ADCs de 12 bits, GPIOs com suporte a interrupções e canais PWM.
* **Armazenamento Local:** Sistema de arquivos LittleFS montado em partição dedicada para logs CSV.

### Sensores e Atuadores

* **Temperatura e Umidade do Ar:** Sensor DHT22 (GPIO 5).
* **Umidade do Solo:** Sensor resistivo/capacitivo HW-080 (ADC1 - GPIO 32).
* **Atuador:** Válvula solenoide / Bomba d'água acionada por Módulo Relé (GPIO 25).

---

## 🚀 Como Executar

### 1. Backend e Banco de Dados (Docker)
```bash
docker compose up -d
```
A API estará acessível em `http://localhost:8080/api`.

### 2. Firmware Embarcado (ESP32 via PlatformIO + ESP-IDF)
```bash
cd embedded
pio run -t upload
pio run -t uploadfs # Faz o upload da interface web comprimida (.gz) para a partição LittleFS
```

### 3. Pipeline do Modelo TinyML (Python)
```bash
cd ai-model
pip install -r requirements.txt
python3 train_tinyml_v2.py
```

---

## 📚 Documentação e Referências

* [Planejamento e Tarefas (TODO)](docs/TODO.md)
* [Checklists de Execução](CHECKLISTS.md)
* [Datasets de Referência](docs/DATASETS.md)
* [Artigos e Literatura Científica](docs/ARTICLES.md)
