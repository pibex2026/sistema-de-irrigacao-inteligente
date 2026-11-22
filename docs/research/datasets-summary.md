# Levantamento e Avaliação de Datasets para o Modelo TinyML

> Para a análise estatística completa, matrizes de correlação, provas de hash MD5 e benchmark experimental com rede neural MLP, consulte o relatório detalhado em **[datasets-evaluation.md](datasets-evaluation.md)**.

---

### 1. [Smart Agriculture Dataset](https://www.kaggle.com/datasets/chaitanyagopidesi/smart-agriculture-dataset/data) — **[DATASET ESCOLHIDO]**
* **Arquivo local:** `ai-model/datasets/Smart Agriculture Dataset/cropdata_updated.csv` (`16.411` registros, `0` valores nulos).
* **Colunas:** `crop ID`, `soil_type`, `Seedling Stage`, `MOI` (Umidade do Solo: `1%` a `100%`), `temp` (Temperatura do Ar: `13 °C` a `46 °C`), `humidity` (Umidade do Ar: `15%` a `91%`), `result` (`0`, `1`, `2`).
* **Justificativa da Escolha:**
  1. **100% Compatível com o Hardware ESP32:** Possui exatamente as 3 grandezas físicas medidas pelo protótipo (**DHT22**: `temp`, `humidity` e **HW-080**: `MOI`) nas mesmas unidades e faixas climáticas reais.
  2. **Forte Correlação Física com a Decisão de Irrigação:** Ao contrário dos outros datasets, todas as 3 variáveis possuem correlação significativa e fisicamente coerente com o acionamento da bomba ($r_{\text{temp}} = +0,5727$, $r_{\text{humidity}} = -0,5307$, $r_{\text{MOI}} = -0,2822$).
  3. **Resolução sobre o Valor Intruso (`result == 2`) e Contexto Agronômico Opcional:**
     - A inspeção estatística revelou que a classe `2` ocorre em **`1.122` linhas** ($6,84\%$ do total, e não 15k) e caracteriza situações de **alta temperatura ($35,4\text{ °C}$), mas com solo já muito úmido (média de `MOI` = $67,63\%$ — acima da capacidade de campo)**, onde irrigar causaria encharcamento e asfixia radicular.
     - Além disso, a análise provou que adicionar o contexto do canteiro (`soil_type`, `crop ID`, `Seedling Stage`) eleva a identificabilidade da Classe `2` de $26,0\%$ (só 3 sensores) para **$73,2\%$** (`+ soil_type`), **$85,6\%$** (`+ soil_type + crop ID`) e **$100,00\%$** (contexto completo).
     - **Decisão Arquitetural Validada:** O modelo TinyML será treinado com as **3 classes (`0 = Não Irrigar`, `1 = Irrigar`, `2 = Alerta de Estresse Térmico com Solo Úmido`)** priorizando o perfil de **Hortaliças Locais**, recebendo as 3 leituras dos sensores + as 3 variáveis de contexto agronômico **opcionais** (via *Context Dropout* com token `UNKNOWN = 0`, garantindo que a rede neural execute normalmente no ESP32 mesmo quando o usuário não informar nenhum dado de solo/cultura).

---

### 2. [Predicting watering the plants](https://www.kaggle.com/datasets/nelakurthisudheer/dataset-for-predicting-watering-the-plants) — **[DESCARTADO]**
* **Arquivo local:** `ai-model/datasets/Dataset for Predicting watering the plants/TARP.csv` (`100.000` linhas, `15` colunas).
* **Notebook de referência:** [Predicting watering the plants: notebook with some strategies](https://www.kaggle.com/code/kawsarahmed0054/smart-irrigation)
* **Motivos Técnicos para Descarte:**
  - **76% de Valores Nulos nas Variáveis do Ar:** As colunas `Air temperature (C)` e `Air humidity (%)` estão vazias em `76.005` das `100.000` linhas; `ph`, `rainfall`, `N`, `P`, `K` estão vazias em `97.800` linhas ($97,8\%$).
  - **Correlação Nula com o Sensor DHT22:** Nas `23.995` linhas onde temperatura e umidade do ar existem, a correlação com o rótulo `Status` (`ON`/`OFF`) é estatisticamente nula ($r_{\text{Air temp}} = +0,0097$ e $r_{\text{Air hum}} = -0,0076$).
  - **Baixa Acurácia com 3 Features:** Treinando a MLP apenas com as 3 variáveis que o ESP32 possui, a acurácia não passa de **$65,47\%$ a $69,96\%$**, pois o target original foi gerado dependendo de outras colunas não disponíveis no nosso hardware.

---

### 3. [IoT Irrigation Sensor Dataset with Raspberry Pi](https://www.kaggle.com/datasets/umapreethi/raspberry-pi-iot-irrigation-sensor-data) — **[DESCARTADO]**
* **Arquivos locais:**
  - `ai-model/datasets/IoT Irrigation Sensor Dataset with Raspberry Pi/iotsensordata.csv` (`100.000` linhas)
  - `ai-model/datasets/IoT Irrigation Sensor Dataset with Raspberry Pi/livesensor_log1.csv` (`305` linhas)
* **Motivos Técnicos para Descarte:**
  - **Arquivo Principal Duplicado:** A verificação por hash MD5 (`94cb7d6f6f8f`) comprovou que `iotsensordata.csv` é uma **cópia bit-a-bit idêntica** ao `TARP.csv` do dataset *Predicting watering the plants*, herdando todos os seus defeitos.
  - **Log Real Insuficiente e Enviesado:** O arquivo `livesensor_log1.csv` possui apenas `305` linhas coletadas em ambiente fechado com temperatura quase fixa ($22,6\text{ °C}$ a $23,4\text{ °C}$ em 304 linhas) e apenas `32` acionamentos de bomba (`ON`), sendo insuficiente para generalizar no clima de Pernambuco.

---

### 4. [Soil moisture and temperature data in agricultural soil](https://data.mendeley.com/datasets/fpbfmc9vnm/2) — **[DESCARTADO]**
* **Arquivo local:** `ai-model/datasets/Soil moisture and temperature data in agricultural soil/Soil moisture and temperature data in agricultural soil/Manzano2022COMPAG_data.csv` (`15.777` linhas).
* **Motivos Técnicos para Descarte:**
  - **Ausência de Variável Alvo (Unlabeled):** Não possui coluna indicando necessidade de irrigação ou estado da válvula/bomba.
  - **Ausência de Umidade do Ar:** Registra apenas timestamp (`t`), nó (`Node_ID`), temperatura intrassolo (`Temp`) e umidade do solo (`Moist`), durante apenas 22 dias de março/2022 em lavoura de cana-de-açúcar.
