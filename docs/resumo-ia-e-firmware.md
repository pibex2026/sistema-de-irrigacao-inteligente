# Resumo Estruturado: Inteligência Artificial e Firmware (ESP32)

Este documento apresenta uma visão simples e organizada das **tecnologias** e **estratégias** adotadas na seleção, treinamento, validação e testes do modelo de IA (**TinyML**) e na construção do **firmware** do Sistema de Irrigação Inteligente.

---

## 1. Modelo de Inteligência Artificial (TinyML)

O objetivo da IA é decidir, diretamente no microcontrolador, entre **3 classes**: `0` (*Desligado / OFF*), `1` (*Irrigar / IRRIGATE*) e `2` (*Estresse Térmico com Solo Úmido / THERMAL_STRESS_WET_SOIL*).

### 1.1. Tecnologias Utilizadas
* **Linguagem e ML:** Python 3, **TensorFlow / Keras** (modelagem e treino) e **Scikit-learn** (divisão estratificada, *GroupSplit* e métricas).
* **Manipulação de Dados e Visualização:** **Pandas**, **NumPy** e **Matplotlib**.
* **Embarcamento (TinyML):** **TensorFlow Lite (`TFLiteConverter`)** com quantização **Full Integer (`INT8`)** e exportação automática para header C/C++ ([model_data.h](file:///home/victor/projects/sistema-de-irrigacao-inteligente/embedded/include/model_data.h)).

### 1.2. Seleção de Dados e Arquitetura
* **Base de Dados:** *Smart Agriculture Dataset* ([cropdata_updated.csv](file:///home/victor/projects/sistema-de-irrigacao-inteligente/ai-model/datasets/Smart%20Agriculture%20Dataset/cropdata_updated.csv)).
  * Passou por limpeza prévia removendo **128 duplicatas exatas** (`16.411` $\rightarrow$ `16.283` registros únicos).
* **Engenharia de Atributos (`27` entradas):**
  * **3 sensores normalizados (`[0, 1]`):** temperatura (`0–50 °C`), umidade do ar (`0–100%`) e umidade do solo (`0–100%`).
  * **20 atributos de contexto (*One-Hot*):** tipo de solo (`7`), cultura (`5`) e estágio da planta (`8`).
  * **4 atributos físicos/psicrométricos:** `VPD_norm` (Déficit de Pressão de Vapor pela equação de Tetens), `EvapDemand` (demanda evaporativa), razão `Temp/MOI` e acoplamento `MOI * Temp`.
* **Escolha da Arquitetura (`PhysMLP-27`):**
  * Em um benchmark experimental com **10 arquiteturas** (incluindo MLPs, ResMLP, Árvore de Decisão, Random Forest e GBDT), a **`PhysMLP-27`** (`27 -> 32 ReLU -> 16 ReLU -> 3 Softmax`) foi escolhida pelo melhor equilíbrio entre acurácia, calibração de probabilidades e baixo uso de memória Flash.

### 1.3. Estratégias de Treinamento e Otimização
* **Full-8 Mask Context Dropout:** O modelo é treinado com todas as $2^3 = 8$ combinações de ausência ou presença de `(Solo, Cultura, Estágio)`. Assim, o sistema continua funcionando bem mesmo se o usuário não configurar todos os dados da plantação.
* **Pesos de Classe Calibrados (`{0: 1.0, 1: 1.15, 2: 2.20}`):** Substituiu o balanceamento automático agressivo (`class_weight="balanced"`), eliminando o excesso de falsos alarmes na Classe 2 e melhorando a calibração (ECE).
* **Controle de Convergência:** Uso de `EarlyStopping` e `ReduceLROnPlateau` sobre um conjunto de validação limpo (`is_training=False`, sem distorção artificial dos dados).
* **Quantização Full INT8:** Conversão usando `representative_dataset` cobrindo os 8 modos de contexto. O tamanho do modelo caiu de `8.216 bytes` (Float32) para **`5.480 bytes`** (`-33,3%`).
* **Ajuste Fino Contínuo (*Fine-Tuning*):** O script [train_tinyml_v2.py](file:///home/victor/projects/sistema-de-irrigacao-inteligente/ai-model/scripts/train_tinyml_v2.py) permite retreinar o modelo incrementalmente usando logs reais (`inference.csv`) coletados pelo ESP32 em campo.

### 1.4. Estratégias de Validação e Testes
* **Validação Dupla (Anti-Vazamento):**
  1. **Split Estratificado (`70% / 15% / 15%`):** mantém a proporção de culturas e classes.
  2. **`GroupSplit` Estrito (`0%` de vazamento):** garante que nenhuma combinação física idêntica `(MOI, temp, humidity)` apareça simultaneamente no treino e no teste, medindo a generalização real.
* **Avaliação nos 8 Modos de Contexto:**
  * **Com contexto completo (M1):** `98,20%` de acurácia (`97,43%` no `GroupSplit`).
  * **Sem nenhum contexto (M8 — só sensores):** `90,54%` de acurácia (`86,56%` no `GroupSplit`).
* **Fidelidade da Quantização:** Verificação entre o modelo Keras Float32 e o TFLite INT8 acusou **`98,50%`** de concordância de decisão.
* **Golden Test Vectors no Boot:** 6 casos de teste conhecidos (`GOLDEN_VECTORS[6]`) são exportados para o C++ e validados automaticamente na inicialização do ESP32 via `ModelManager::runGoldenSelfTest()`.

---

## 2. Firmware do Projeto (ESP32)

O firmware foi desenvolvido em **C++17** com foco em determinismo, segurança operacional e tolerância a falhas de rede ou sensores.

### 2.1. Tecnologias Utilizadas
* **Build e Framework:** **PlatformIO** + **ESP-IDF v5.5.4** (`-std=gnu++17`) sobre placa ESP32 Dual-Core (`esp32doit-devkit-v1`).
* **Sistema Operacional em Tempo Real:** **FreeRTOS Dual-Core** nativo do ESP-IDF.
* **Runtime de IA Embarcada:** **`espressif/esp-tflite-micro`** (`^1.3.0`) com aceleração de kernels **ESP-NN** (arena de `12 KB`).
* **Armazenamento Local:**
  * **LittleFS (`joltwallet/littlefs`)**: partição de `512 KB` para gravação de logs CSV (`/littlefs/readings.csv` e `/littlefs/inference.csv`) e interface web compactada (`index.html.gz`).
  * **NVS (*Non-Volatile Storage*)**: persistência das configurações do usuário (`ConfigManager`).
* **Rede e Interface Web:** **`esp_wifi`** (STA + SoftAP), **SNTP** (horário), **`esp_http_server`**, **`cJSON`** e **`espressif/mdns`** (`http://irrigacao.local`).

### 2.2. Estratégias de Arquitetura e Confiabilidade
* **Escalonamento SMP Dinâmico com Prioridades (`tskNO_AFFINITY`):**
  * Em vez de prender cada função a um único núcleo, as tasks de aplicação usam **`tskNO_AFFINITY`**, permitindo que ambos os núcleos do ESP32 executem dinamicamente as tarefas prontas de maior prioridade:
    1. **`actuator_safety` (Prio `7`, WDT `30 s`):** Supervisiona *timeout* hidráulico, *cooldown*, LED e comandos de válvula a cada `100 ms`.
    2. **`sensor_ai_task` (Prio `6`, WDT `30 s`):** Lê DHT22/HW-080, executa inferência TinyML e envia comandos por fila sem bloquear em Flash ou rede.
    3. **`httpd` (Prio `4`):** Serve a API local e interface Web lendo snapshots thread-safe de `SystemStateStore`.
    4. **`telemetry_task` (Prio `3`):** Acorda por *Task Notification* em mudanças de válvula (ou intervalo periódico) para enviar telemetria HTTP.
    5. **`logger_task` e `dns_captive` (Prio `2`):** Gravação assíncrona no LittleFS via fila de `24` slots (descartando o item mais antigo se cheia) e DNS do portal cativo.
* **Máquina de Estados Finitos (FSM):**
  * Fluxo determinístico: `INIT` $\rightarrow$ `READ_SENSORS` $\rightarrow$ `RUN_INFERENCE` $\rightarrow$ `EXECUTE_ACTION` (com estado `ERROR` para parada de emergência e recuperação automática).
* **Proteção contra Falhas de Sensores (Sentinelas):**
  * O `SensorsManager` lê o **DHT22** (GPIO 5) e o sensor de solo **HW-080** (GPIO 32, usando mediana de 10 leituras ADC1 para filtrar ruídos).
  * Leituras inválidas ou sentinelas de erro (`-128`, `-999`, `255`, `NaN`, curto-circuito ou fio solto) são bloqueadas por `isValidSensorReading()` antes de entrarem na rede neural. Após 3 falhas seguidas, o sistema entra em modo seguro (`ERROR`).
* **Segurança na Atuação e Fallback Físico:**
  * **Atuador (`ActuatorManager`):** Exige **2 confirmações seguidas** para abrir o relé (*debounce*), mas **fecha imediatamente** ao menor sinal de parada, timeout ou erro. Conta ainda com tempo máximo de irrigação (*timeout*) e espera mínima entre ciclos (*cooldown*).
  * **Fallback de Estresse Térmico:** Se `temp >= 32 °C` e `soil >= 65%`, o firmware bloqueia a irrigação por segurança e promove o alerta térmico caso a probabilidade da Classe 2 atinja o limiar configurado (`0,40`).
* **Gestão Enxuta de Memória e Logs:**
  * Uso reduzido de recursos: **`15,6%` de RAM estática** (`51.152 B`) e **`53,4%` da Flash `app0`** (`944.247 B`), preservando espaço para atualizações **OTA**.
  * Os arquivos de log no LittleFS possuem rotação automática em `180 KB` e controle por *mutex* para evitar falta de memória (*anti-OOM*).
