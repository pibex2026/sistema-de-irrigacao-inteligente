# Modelo TinyML (`PhysMLP-27` Full INT8) e Pipeline de Treinamento

> **Contexto:** Síntese técnica do pipeline de Inteligência Artificial Embarcada em `ai-model/train_tinyml_v2.py` e sua integração determinística com o firmware ESP32 (`embedded/include/model_data.h` e `ModelManager`).

---

## 1. Dados, Pré-processamento e Engenharia de Atributos

* **Dataset Oficial:** `ai-model/datasets/Smart Agriculture Dataset/cropdata_updated.csv`.
  - **Bruto:** `16.411` registros $\rightarrow$ **Deduplicado:** **`16.283` registros únicos** (`128` duplicatas exatas removidas pré-split).
  - **Tuplas físicas únicas `(MOI, temp, humidity)`:** `2.886`.
* **Divisão & Validação Dupla:**
  - **Split Estratificado Limpo (`70% / 15% / 15%`):** estratificado por `crop ID + result`.
  - **GroupSplit Estrito (`0% Sensor-Tuple Leakage`):** garante zero sobreposição de leituras físicas entre treino, validação e teste.
* **Vetor de Entrada (`27` features `float32` $\rightarrow$ quantizadas para `int8`):**
  - `[0..2]`: `temp` (`[0..50 °C]`), `humidity` (`[0..100 %]`), `soilMoisture` (`[0..100 %]`) normalizados em `[0.0, 1.0]`.
  - `[3..9]`: *One-Hot* `soil_type` (`7` tipos; `0 = UNKNOWN` $\rightarrow$ vetor zero).
  - `[10..14]`: *One-Hot* `crop ID` (`5` culturas; `0 = UNKNOWN` $\rightarrow$ fallback hortaliças locais).
  - `[15..22]`: *One-Hot* `Seedling Stage` (`8` estágios; `0 = UNKNOWN` $\rightarrow$ vetor zero).
  - `[23..26]`: **4 features psicrométricas/hidrotérmicas:** `VPD_norm` (equação de Tetens / `8.0 kPa`), `EvapDemand` (`T * (100 - RH) / 4000`), `Temp/MOI ratio` e `MOI * Temp` coupling.

---

## 2. Arquitetura, Treinamento e Métricas (`train_tinyml_v2_report.json`)

* **Arquitetura:** `Input(27) -> Dense(32, ReLU) -> Dense(16, ReLU) -> Dense(3, Softmax)`.
* **Estratégia de Treino:** *Full-8 Mask Context Dropout* (todas as $2^3 = 8$ combinações de contexto, resolvendo o ponto cego do Modo 6 `[0, C, 0]`), validação limpa (`is_training=False`), `EarlyStopping` + `ReduceLROnPlateau` e pesos calibrados `{0: 1.0, 1: 1.15, 2: 2.20}`.

| Cenário de Contexto | Split Estratificado (Acc / Macro-F1 / ECE) | GroupSplit 0% Leakage (Acc / Macro-F1 / ECE) |
| :--- | :---: | :---: |
| **M1: Completo `[S, C, St]`** | `98,20%` / `95,29%` / `0,0054` | `97,43%` / `93,99%` / `0,0026` |
| **M4: Cultura+Estágio `[0, C, St]`** | `97,26%` / `93,01%` / `0,0064` | — |
| **M5: Apenas Solo `[S, 0, 0]`** | `90,34%` / `78,14%` / `0,0144` | — |
| **M6: Apenas Cultura `[0, C, 0]`** | `89,28%` / `76,85%` / `0,0128` | `87,30%` / `74,85%` / `0,0125` |
| **M8: Zero Contexto `[0, 0, 0]`** | `90,54%` / `76,84%` / `0,0171` | `86,56%` / `74,88%` / `0,0323` |

---

## 3. Quantização Full INT8, Segurança e Contrato C++17

* **Tamanho:** **`5.480 bytes`** (`model_int8.tflite`) vs `8.216 bytes` (`model_float32.tflite`) — redução de **`33,3%`**.
* **Operadores no Grafo TFLite:** `3x FULLY_CONNECTED` + `1x SOFTMAX` (`11` tensores).
* **Fidelidade Keras F32 vs TFLite INT8:** concordância de decisão **`98,50%`** (`mean_abs_diff = 0,00660`).
* **Proteção contra Sentinelas:** `isValidSensorReading()` rejeita `-128`, `-999`, `255`, `NaN` e faixas físicas impossíveis antes da normalização em Python e em C++.
* **Fallback Físico de Estresse Térmico (`ModelManager`):** Quando `temp >= 32 °C` e `soil >= 65%`, bloqueia `IRRIGATE` e também promove `OFF` para `THERMAL_STRESS_WET_SOIL` se `P(2) >= thermalStressThreshold` (`0,40`), resolvendo o empate numérico do Golden Vector Caso 3 (`P0=0,434`, `P2=0,434`).
* **Golden Test Vectors Compiláveis:** `TinyMLConfig::GOLDEN_VECTORS[6]` é exportado automaticamente em `embedded/include/model_data.h` e verificado no boot do ESP32 por `ModelManager::runGoldenSelfTest()`.

---

## 4. Ciclo Contínuo de *Fine-Tuning* com Dados de Campo

O script `ai-model/train_tinyml_v2.py` aceita logs reais exportados do LittleFS do ESP32 (`/logs/readings.csv` e `/logs/inference.csv`) para ajuste fino incremental:

```bash
/usr/bin/python3 ai-model/train_tinyml_v2.py \
  --field-inferences caminho/para/inference.csv \
  --epochs-ft 15 \
  --lr-ft 5e-4
```
