# Análise Técnica e Seleção de Datasets para o Modelo TinyML (ESP32)

> **Projeto:** Sistema de Irrigação Inteligente - PIBEX 2026 - IFPE Campus Paulista  
> **Escopo:** Avaliação estatística, estrutural, agronômica e experimental (benchmark de rede neural MLP) dos 4 conjuntos de dados baixados em `ai-model/datasets/`, visando selecionar e justificar cientificamente o dataset ideal para o treinamento do modelo **TinyML** embarcado no ESP32.

---

## 1. Critérios de Avaliação e Restrições de Hardware

O modelo de Inteligência Artificial embarcada (*TinyML*) será executado localmente no microcontrolador **ESP32** (`ModelManager`) alimentado exclusivamente pelas leituras físicas do módulo `SensorsManager`:

1. **Temperatura do Ar (`temp` em °C):** medida pelo sensor digital **DHT22** (GPIO 5).
2. **Umidade Relativa do Ar (`humidity` em %):** medida pelo sensor digital **DHT22** (GPIO 5).
3. **Umidade do Solo (`soilMoisture` em %):** medida pelo sensor **HW-080** via conversor analógico-digital de 12 bits (ADC1 - GPIO 32), mapeada de `0%` a `100%`.
4. **Saída do Modelo (`target` binário `0` ou `1`):** Probabilidade de necessidade hídrica para acionar (`1 = ON`) ou desligar (`0 = OFF`) a válvula solenoide/relé (GPIO 25).

Portanto, para que um dataset seja viável e fisicamente coerente com o protótipo:
- As **3 variáveis de entrada** precisam existir no dataset nas mesmas unidades físicas (`°C`, `%`, `%`).
- A **variável alvo (decisão de irrigação)** deve ser explicável majoritariamente por essas 3 variáveis (se o rótulo original do dataset depender de sensores ausentes no nosso hardware, como vento, NPK, pH ou pressão barométrica, o modelo sofrerá de *sub-especificação / ruído epistêmico*).
- As faixas climáticas e o comportamento agronômico devem ser coerentes com a realidade da agricultura familiar (hortaliças e lavouras comunitárias em clima tropical/semiúmido como Paulista-PE).

---

## 2. Inspeção Detalhada dos Arquivos Baixados

A inspeção via script Python (`csv`, `hashlib`, estatística descritiva e correlação de Pearson) sobre os arquivos presentes em `ai-model/datasets/` revelou fatos técnicos fundamentais:

| Pasta / Dataset | Arquivo(s) Encontrado(s) | Hash MD5 | Linhas $\times$ Colunas | Valores Ausentes (Missing) |
| :--- | :--- | :--- | :--- | :--- |
| **1. Dataset for Predicting watering the plants** | `TARP.csv` | `94cb7d6f6f8f` | $100.000 \times 15$ | `76.005` nulos em Clima/Ar; `97.800` nulos em NPK/pH/Chuva |
| **2. IoT Irrigation Sensor Dataset with Raspberry Pi** | `iotsensordata.csv`<br>`livesensor_log1.csv` | `94cb7d6f6f8f`<br>`87183d85c904` | $100.000 \times 15$<br>$305 \times 37$ | `iotsensordata.csv` é **cópia idêntica** ao `TARP.csv`.<br>`livesensor_log1.csv` tem apenas 305 linhas. |
| **3. Smart Agriculture Dataset** | `cropdata_updated.csv` | `a4525c1f52ed` | $16.411 \times 7$ | **Zero (`0`) valores nulos** em todas as colunas |
| **4. Soil moisture and temperature data in agricultural soil** | `Manzano2022COMPAG_data.csv`<br>`Manzano2022COMPAG_coordinates.csv` | `b83f169e9f4c`<br>`31c549...` | $15.777 \times 4$<br>$12 \times 3$ | Zero nulos, mas **não possui Umidade do Ar nem Coluna Alvo (`Target`)** |

---

## 3. Avaliação Individual e Justificativa Técnica de Descarte ou Seleção

### 3.1. [DESCARTADO] Soil moisture and temperature data in agricultural soil (`Manzano2022COMPAG_data.csv`)
* **Fonte:** [Mendeley Data (Manzano et al., 2022)](https://data.mendeley.com/datasets/fpbfmc9vnm/2)
* **Estrutura Real:** Contém `15.777` registros com 4 colunas: `t` (timestamp de 09/mar/2022 a 31/mar/2022), `Node_ID` (nós 1 a 12), `Temp` ($21,16\text{ °C}$ a $33,65\text{ °C}$ — temperatura **do solo**, não do ar) e `Moist` ($30,61\%$ a $95,10\%$).
* **Motivos Técnicos para Descarte:**
  1. **Não é um dataset rotulado para aprendizado supervisionado de atuação:** Não existe coluna alvo (`Status` / `Result` / `Pump`) indicando quando irrigar.
  2. **Incompatibilidade de Sensores:** Não possui a variável **Umidade Relativa do Ar (%)** fornecida pelo nosso DHT22, e a temperatura medida é intrassolo ($z_T$), diferente da temperatura ambiente lida pelo DHT22.
  3. **Janela Temporal Curta:** Compreende apenas 22 dias de março de 2022 em uma única cultura (cana-de-açúcar), impedindo generalização sazonal.

---

### 3.2. [DESCARTADO] Dataset for Predicting watering the plants (`TARP.csv`)
* **Fonte:** [Kaggle - Nelakurthi Sudheer](https://www.kaggle.com/datasets/nelakurthisudheer/dataset-for-predicting-watering-the-plants)
* **Estrutura Real:** `100.000` linhas e `15` colunas (`Soil Moisture`, `Temperature`, `Soil Humidity`, `Time`, `Air temperature (C)`, `Wind speed (Km/h)`, `Air humidity (%)`, `Wind gust (Km/h)`, `Pressure (KPa)`, `ph`, `rainfall`, `N`, `P`, `K`, `Status`). Target: `ON` ($53.650$) e `OFF` ($46.350$).
* **Descobertas Críticas na Análise Exploratória:**
  1. **Mutilação por Dados Faltantes (Missing Data severo):**
     - As colunas `Air temperature (C)` e `Air humidity (%)` estão **vazias (`NaN`) em `76.005` das `100.000` linhas (76% de perda)**.
     - As colunas agronômicas `ph`, `rainfall`, `N`, `P`, `K` estão **vazias em `97.800` linhas (97,8% de perda)**.
  2. **Ausência de Correlação Climática Real com o Target:**
     - Nas `23.995` linhas onde `Air temperature (C)` e `Air humidity (%)` existem, a correlação de Pearson com a decisão `Status` (`ON`/`OFF`) é **praticamente zero**:
       - $r(\text{Air temperature}, \text{Status}) = +0,0097$ (média `ON`: $24,32\text{ °C}$ vs. média `OFF`: $24,19\text{ °C}$)
       - $r(\text{Air humidity}, \text{Status}) = -0,0076$ (média `ON`: $58,31\%$ vs. média `OFF`: $58,77\%$)
     - Mesmo usando as 3 primeiras colunas sem nulos (`Soil Moisture`, `Temperature`, `Soil Humidity`), a correlação de `Soil Humidity` com o acionamento é $r = +0,0240$ (quase aleatória).
  3. **Desempenho Fraco em Rede Neural MLP (3 Entradas):**
     - Ao treinar uma rede MLP compatível com TinyML usando apenas as 3 features disponíveis no nosso ESP32, a acurácia de teste estagna em **$65,47\%$** (usando `Air temp`, `Air humidity`, `Soil Moisture`) ou **$69,96\%$** (usando `Temperature`, `Soil Humidity`, `Soil Moisture`), pois o rótulo `Status` original desse dataset depende fortemente da coluna sintética `Time` e de regras não capturadas pelas 3 variáveis ambientais.

---

### 3.3. [DESCARTADO] IoT Irrigation Sensor Dataset with Raspberry Pi (`iotsensordata.csv` e `livesensor_log1.csv`)
* **Fonte:** [Kaggle - Uma Preethi](https://www.kaggle.com/datasets/umapreethi/raspberry-pi-iot-irrigation-sensor-data)
* **Descobertas Críticas na Análise Exploratória:**
  1. **Duplicata Exata (`iotsensordata.csv`):** O cálculo de hash criptográfico provou que `iotsensordata.csv` possui exatamente o mesmo hash MD5 (`94cb7d6f6f8f3a...`), mesmo tamanho (`3,2 MB`) e mesmos `100.000` registros que o arquivo `TARP.csv` do dataset anterior. Logo, sofre exatamente dos mesmos problemas estruturais (76% de valores nulos em ar/clima e baixa acurácia com 3 features).
  2. **Amostragem Insuficiente e Viés de Experimento (`livesensor_log1.csv`):**
     - O segundo arquivo (`livesensor_log1.csv`) é um log de telemetria de bancada contendo apenas **`305` linhas** e `37` colunas internas de debug de um modelo SNN/Fuzzy.
     - A temperatura do ar (`air_temp_c`) varia apenas entre **$22,6\text{ °C}$ e $23,4\text{ °C}$** em 304 das 305 linhas (experimento feito em sala fechada com temperatura constante), e a umidade do ar (`humidity_pct`) fica restrita à faixa de **$51,0\%$ a $61,8\%$**.
     - Apenas `32` linhas possuem `pump_status = ON` (contra `249` `OFF` e `24` `SKIP_LOW_CONF`). Além disso, em `249` linhas a coluna `override_applied = TRUE` forçou a bomba para `OFF` mesmo quando a rede neural deles previu `ON`. Treinar um modelo com 305 linhas em temperatura constante de $23\text{ °C}$ tornaria o ESP32 incapaz de operar no clima real de Pernambuco ($20\text{ °C}$ a $40\text{ °C}$).

---

### 3.4. [SELECIONADO] Smart Agriculture Dataset (`cropdata_updated.csv`)
* **Fonte:** [Kaggle - Chaitanya Gopidesi](https://www.kaggle.com/datasets/chaitanyagopidesi/smart-agriculture-dataset/data)
* **Estrutura Real:** `16.411` registros, `7` colunas e **nenhum valor nulo (`0 missing values`)**:
  - `crop ID`: 5 culturas (`Wheat`: 6.213, `Chilli`: 2.764, `Potato`: 2.504, `Carrot`: 2.475, `Tomato`: 2.455).
  - `soil_type`: 7 tipos de solo (`Clay Soil`, `Sandy Soil`, `Red Soil`, `Loam Soil`, `Black Soil`, `Alluvial Soil`, `Chalky Soil`).
  - `Seedling Stage`: 8 estágios fenológicos (`Germination`, `Seedling Stage`, `Vegetative Growth`, `Flowering`, `Pollination`, `Fruit/Grain/Bulb Formation`, `Maturation`, `Harvest`).
  - **`MOI` (Soil Moisture Index):** `1%` a `100%` (média: $43,70\%$) $\rightarrow$ Correspondência direta com o sensor **HW-080** mapeado (`0-100%`).
  - **`temp` (Temperatura do Ar):** `13 °C` a `46 °C` (média: $28,83\text{ °C}$) $\rightarrow$ Correspondência direta com o **DHT22** e com a faixa térmica real do Nordeste brasileiro.
  - **`humidity` (Umidade Relativa do Ar):** `15%` a `91%` (média: $63,49\%$) $\rightarrow$ Correspondência direta com o **DHT22**.
  - **`result` (Decisão de Irrigação):** `0` ($9.062$ linhas), `1` ($6.227$ linhas) e `2` ($1.122$ linhas).

#### Coerência Física e Agronômica Comprovada nos Dados
Ao analisar as correlações de Pearson entre as 3 variáveis de sensor e a decisão binária de irrigação (`0 = Não Irrigar`, `1 = Irrigar`), observamos forte alinhamento com a física de evapotranspiração:
- **$r(\text{temp}, \text{result}) = +0,5727$:** Quanto maior a temperatura ambiente (média de $35,07\text{ °C}$ na classe `1` vs. $23,73\text{ °C}$ na classe `0`), maior a necessidade de irrigação.
- **$r(\text{humidity}, \text{result}) = -0,5307$:** Quanto menor a umidade relativa do ar (média de $50,07\%$ na classe `1` vs. $74,45\%$ na classe `0`), maior a demanda evaporativa e a necessidade de irrigação.
- **$r(\text{MOI}, \text{result}) = -0,2822$:** Quanto menor a umidade do solo (média de $32,72\%$ na classe `1` vs. $48,28\%$ na classe `0`), maior a necessidade de acionar a válvula.

---

## 4. Resolução da Dúvida Técnica sobre a Classe Intrusa (`result == 2`)

No documento anterior (`docs/DATASETS.md`), havia o seguinte questionamento sobre o *Smart Agriculture Dataset*:
> *"Há uma coluna específica para indicar o momento de atuação do motor, contudo essa coluna que deveria ter valores antagônicos possui 15k de linhas [na verdade, 1.122 linhas de 16.411] com um valor intruso (`2`). Excluí-los seria uma opção?"*

### Diagnóstico Estatístico da Classe `result == 2`
Inspecionando a distribuição das `1.122` linhas onde `result == 2` (6,84% do dataset):

| Classe (`result`) | Quantidade (%) | `MOI` (Umidade do Solo) | `temp` (Temperatura) | `humidity` (Umidade do Ar) | Interpretação Agronômica |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **`0` (OFF)** | $9.062$ ($55,22\%$) | Média: **$48,28\%$** ($1..100$) | Média: **$23,73\text{ °C}$** ($13..46$) | Média: **$74,45\%$** ($15..91$) | Clima ameno/úmido ou solo já úmido $\rightarrow$ **Não Irrigar**. |
| **`1` (ON)** | $6.227$ ($37,94\%$) | Média: **$32,72\%$** ($1..80$) | Média: **$35,07\text{ °C}$** ($25..46$) | Média: **$50,07\%$** ($15..80$) | Solo seco + calor alto + ar seco $\rightarrow$ **Irrigar**. |
| **`2` (Intruso)** | $1.122$ ($6,84\%$) | Média: **$67,63\%$** ($30..90$)<br>*(80% têm MOI $\ge 58\%$)* | Média: **$35,41\text{ °C}$** ($25..46$) | Média: **$49,37\%$** ($15..80$) | Calor alto ($>35\text{ °C}$) e ar seco, **mas o solo já está muito úmido** (média $67,6\%$), representando alerta de estresse térmico / não encharcar. |

### Comparativo Experimental: Impacto do Contexto Agronômico (`soil_type`, `crop ID`, `Seedling Stage`) na Classe `result == 2`

Durante a entrevista de arquitetura (*design review*), levantou-se uma questão crucial: **por que a Classe `2` se sobrepõe às Classes `0` e `1` quando olhamos apenas para `(MOI, temp, humidity)`?**

A resposta física é a **Capacidade de Campo dependente do solo e da cultura**: $48\%$ de umidade em um **Solo Arenoso** (*Sandy Soil*, que drena rápido) exige resposta diferente de $48\%$ em um **Solo Argiloso** (*Clay Soil*, que retém água), bem como uma planta em **Germinação** difere de uma em **Maturação**. Como o tipo de solo, a cultura plantada e o estágio de crescimento são **parâmetros de contexto do canteiro** (que o agricultor pode informar e o ESP32 armazenar em arquivo de configuração no `LittleFS`), calculamos a separabilidade exata da Classe `2` e o Limite Teórico Bayesiano das **3 classes (`0`, `1` e `2`)** nas `16.411` linhas conforme adicionamos essas variáveis:

| Configuração de Entradas do Modelo | Acurácia Máxima Teórica (3 Classes: `0`, `1`, `2`) | Identificabilidade da Classe `2` (`1.122` amostras) | Comportamento Operacional |
| :--- | :--- | :--- | :--- |
| **1. Apenas 3 Sensores** (`MOI`, `temp`, `humidity`) | **$91,51\%$** *(Multiclasse)*<br>**$97,32\%$** *(Binário Hortaliças)* | `292 / 1122` (**$26,0\%$**) | Funciona sem nenhuma configuração do usuário, mas sozinho não distingue todas as nuances da Classe `2`. |
| **2. Sensores + Tipo de Solo** (`+ soil_type`) | **$96,06\%$** | `821 / 1122` (**$73,2\%$**) | Salto expressivo (+47,2% na Classe 2) apenas sabendo o solo do canteiro. |
| **3. Sensores + Solo + Cultura** (`+ soil_type + crop ID`) | **$97,98\%$** | `960 / 1122` (**$85,6\%$**) | Configurado uma única vez no início do plantio. |
| **4. Contexto Completo** (`+ soil_type + crop ID + Seedling Stage`) | **$100,00\%$** | `1122 / 1122` (**$100,0\%$**) | Determinismo total (zero conflito entre as 16.411 amostras). |

---

### Decisão Arquitetural Final (Alinhada para o Projeto)

Com base nos experimentos acima, o sistema adotará uma **Arquitetura Multiclasse (`0 = Não Irrigar`, `1 = Irrigar`, `2 = Alerta de Estresse Térmico com Solo Úmido`) Resiliente a Contexto Opcional (Modality Dropout / Fallback)**:

1. **Saída Multiclasse (3 Estados com Softmax):**
   - **Classe `0` (`OFF`):** Condição hídrica adequada. Válvula fechada.
   - **Classe `1` (`IRRIGATE`):** Necessidade hídrica detectada. O `ActuatorManager` abre a válvula solenoide (respeitando *timeout* e *cooldown*).
   - **Classe `2` (`THERMAL_STRESS_WET_SOIL`):** Temperatura crítica ($>32\text{ °C}$) com solo já úmido (acima da capacidade de campo). O `ActuatorManager` **mantém a válvula fechada** para evitar asfixia radicular/cozimento das raízes e o `FileLogger` registra o alerta de estresse térmico no CSV.
2. **Entradas Contextuais Opcionais (Não-Bloqueantes):**
   - O modelo receberá as **3 leituras obrigatórias dos sensores** (`temp`, `humidity`, `MOI`) + as **3 variáveis de contexto agronômico opcionais** (`crop ID`, `soil_type`, `Seedling Stage`).
   - **Treinamento com *Context Dropout* (Máscara de Ausência / Token `UNKNOWN = 0`):** Durante o treinamento em Python (`train_tinyml.py`), o dataset será aumentado (*Data Augmentation*) mascarando aleatoriamente `soil_type`, `crop ID` e/ou `Seedling Stage` com o índice `0` (`UNKNOWN / NÃO INFORMADO`) em parte dos batches.
   - **Garantia de Execução Autônoma no ESP32:** Se o agricultor **não informar** o solo, a cultura ou o estágio (ou informar apenas parte deles), o ESP32 preenche os campos ausentes com `0` (`UNKNOWN`) ou com o perfil padrão de **Hortaliças Locais** salvo no `LittleFS` e **executa a inferência normalmente sem jamais travar ou impedir o funcionamento da rede**. Quando o agricultor informa o contexto completo, a precisão do modelo escala até o teto de $\approx 98\% - 100\%$.
3. **Adaptação em Campo + Re-treinamento Futuro (Ciclo Contínuo):**
   - **Nível 1 (Configuração em Memória Local):** Limiar de confiança (*threshold*), calibração do ADC do sensor de solo (`ADC_DRY`, `ADC_WET`) e contexto do canteiro (`crop`, `soil`, `stage`) ficam salvos em arquivo de configuração no `LittleFS` / NVS do ESP32, ajustáveis sem regravar o firmware.
   - **Nível 2 (Fine-Tuning com Dados Reais de Paulista-PE):** Os arquivos `/littlefs/readings.csv` e `/littlefs/inference.csv` coletados pelo ESP32 em campo alimentarão o script `train_tinyml.py` para ajuste fino (*fine-tuning*) do modelo em ciclos futuros do PIBEX.

---

## 5. Resumo Comparativo Final (Benchmark)

| Dataset | Amostras Úteis (3 Features + Target) | Correlação com o Target (`MOI`, `Temp`, `Hum`) | Acurácia MLP TinyML (`3 -> 8 -> 1`) | F1-Score MLP | Veredito |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **Smart Agriculture Dataset** (`cropdata_updated.csv`, sem classe 2) | **$15.289$** *(Todas)*<br>**$9.513$** *(Hortaliças)* | Forte em todas as 3:<br>$(-0.28, +0.57, -0.53)$ | **$95,55\%$** *(Todas)*<br>**$97,32\%$** *(Hortaliças)* | **$94,43\%$**<br>**$96,91\%$** | **ESCOLHIDO (Padrão Ouro para o Projeto)** |
| **Predicting Watering the Plants** (`TARP.csv`) | $23.995$ *(com dados de Ar)*<br>$100.000$ *(Temp/SoilHum)* | Nula nas variáveis do ar:<br>$(-0.32, +0.01, -0.01)$ | $65,47\%$<br>$69,96\%$ | $69,56\%$<br>$74,62\%$ | **Descartado** (76% de nulos no ar e rótulo descorrelacionado do DHT22) |
| **IoT Irrigation Sensor RPi** (`iotsensordata.csv` / `livesensor_log1.csv`) | $100.000$ *(duplicata TARP)*<br>$281$ *(livesensor)* | Idêntico ao TARP /<br>Temp fixa em $23\text{ °C}$ | $65,47\%$ /<br>Overfitting ($n=281$) | $69,56\%$ | **Descartado** (`iotsensordata.csv` é cópia do TARP; `livesensor` só tem 305 linhas a $23\text{ °C}$) |
| **Soil Moisture & Temp (Mendeley)** (`Manzano2022COMPAG_data.csv`) | $0$ *(Sem target e sem umidade do ar)* | N/A (Sem variável alvo) | N/A | N/A | **Descartado** (Faltam feature do DHT22 e rótulo de irrigação) |
