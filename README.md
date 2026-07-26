# Sistema de Irrigacao Inteligente - PIBEX 2026 - IFPE Paulista  

## Descrição do projeto

## Integrantes

## Hardware utilizado
### Microcontrolador escolhido

ESP32

Caracteristicas:

* Processador: CPU Xtensa Dual-Core 32-bit LX6 com clock ajustável até 240 MHz (desempenho de 600 DMIPS).
* Memória RAM: 520 KB de SRAM interna para execução de dados e instruções de programas.
* Memória Flash: Geralmente 4 MB (conectados via SPI) para armazenamento do código.
* Wi-Fi: Integrado padrão 802.11 b/g/n operando na frequência de 2.4 GHz.
* Bluetooth: Versão 4.2 suportando BR/EDR (clássico) e BLE (Bluetooth Low Energy).
* Pinos GPIO: Até 36 pinos digitais multifuncionais (leitura, escrita e interrupções).
* Conversor Analógico (ADC): 2 módulos de 12 bits com até 18 canais para leitura de sensores analógicos.
* Conversor Digital (DAC): 2 canais de 8 bits para geração de sinais analógicos puros
* Canais PWM: Até 16 canais independentes para controle de velocidade de motores e servos.
* Barramentos de Comunicação: 3x UART, 3x SPI, 2x I2C e suporte nativo a barramento CAN Bus 2.0.
* Consumo em Deep Sleep: Consumo ultra baixo de aproximadamente 10 µA a 15 µA mantendo o coprocessador ULP ativo.
* Coprocessador ULP: Um terceiro núcleo de baixíssimo consumo para monitorar sensores enquanto a CPU principal dorme.
* Segurança: Aceleração de hardware criptográfico para AES, SHA-2, RSA, ECC e RNG (gerador de números aleatórios).

### Sensores utilizados

* Temperatura e umidade do ar: DHT22
* Umidade do solo: HW-080


## Datasets
### [Predicting watering the plants](https://www.kaggle.com/datasets/nelakurthisudheer/dataset-for-predicting-watering-the-plants)

Muito completo, mas carece de informações referentes à cultura, peca por se propor a ser genérico demais.

Aplicação dele utilizando diversos algoritmos sendo gradientboots e random forest estratégias a se observar pelo desempenho:  
[Predicting watering the plants: notebook with some strategies](https://www.kaggle.com/code/kawsarahmed0054/smart-irrigation)

---
### [Soil moisture and temperature data in agricultural soil](https://data.mendeley.com/datasets/fpbfmc9vnm/2)

Grande número de leituras e bastante completo. Leituras compreendem o mês de março/22 o que limita demais a generalização do modelo, além de não especificar a cultura cultivada.

---
### [IoT Irrigation Sensor Dataset with Raspberry Pi](https://www.kaggle.com/datasets/umapreethi/raspberry-pi-iot-irrigation-sensor-data)

Promissor, tem mais de 100k de registros que se adequam bem as nossas possibilidades e necessidades.

---

### [Smart Agriculture Dataset](https://www.kaggle.com/datasets/chaitanyagopidesi/smart-agriculture-dataset/data)


Vasta quantidade de dados, mais de 160k de linhas. Informações pertinentes quanto ao estágio da cultura do trigo.
Há uma coluna específica para indicar o momento de atuação do motor, contudo essa coluna que deveria ter valores antagônicos possui 15k de linhas com um valor intruso. Excluí-los seria uma opção?

## Artigos
### [Internet of Things and Wireless Sensor Networks for Smart Agriculture Applications: A Survey](https://ieeexplore.ieee.org/document/10371307)

#### Referência: Md. Najmul Mowla, N. Mowla, S. Shah, E. Alsusa e T. Shongwe, "Internet of Things and Wireless Sensor Networks for Smart Agriculture Applications- A Survey", IEEE Access, vol. 11, pp. 145813-145852, Jan. 2023

Acurácia alta porém com um alto custo de implementação

------------------------------------------

### [Soil Moisture Prediction Using Machine Learning Techniques](https://dl.acm.org/doi/10.1145/3440840.3440854) 

#### Referência: S. Paul e S. Singh, "Soil Moisture Prediction Using Machine Learning Techniques", 2020 The 3rd International Conference on Computational Intelligence and Intelligent Systems, Nov. 2020

Acurácia alta mas limitado a tipos específicos de solo



### [Machine Learning Approaches for Crop Water Requirement Prediction and Optimization](https://www.researchgate.net/publication/399128391_Machine_Learning_Approaches_for_Crop_Water_Requirement_Prediction_and_Optimization) 

----------------------------------------------------

### [IoT and Edge Computing for Real-time Monitoring](https://www.researchgate.net/publication/356482133_An_IoT-Based_Real-Time_Intelligent_Irrigation_System_using_Machine_Learning) 

Acurácia alta mas com escalabilidade limitada

------------

### [Smart Irrigation System with Predictive Analytics using Machine Learning and IoT](https://www.americaspg.com/articleinfo/18/show/1629) 

#### Referência: A. Sleem e Ibrahim Elhenawy, "Smart Irrigation System with Predictive Analytics using Machine Learning and IoT", Journal of Intelligent Systems and Internet of Things, vol. 2, no. 2, pp. 77-83, Jan. 2021

Alta acurácia mas há uma preocupação com a privacidade dos dados

----

### [ Fuzzy Logic Controllers in Smart Irrigation.](https://www.mdpi.com/2673-4591/58/1/93) 

#### Referência: MD Jiabul Hoque, Md. Saiful Islam e Md. Khaliluzzaman, "A Fuzzy Logic-and Internet of Things-Based Smart Irrigation System", Nov. 2023.


Alta acurácia mas há Complexidade no ajuste fino das regras fuzzy (tuning fuzzy rules).

----------

### [Blockchain-IoT integrated for Secure Irrigation Systems](https://ieeexplore.ieee.org/document/9622256)

#### Referência: E. A. Shammar, A. T. Zahary e A. A. Al-Shargabi, "A Survey of IoT and Blockchain Integration: Security Perspective", IEEE Access, vol. 9, pp. 156114-156150, 2021.

Alta acurácia mas há um alto consumo de energia

---

### [Hybrid AI Models for Water Usage Optimization](https://www.mdpi.com/2078-2489/15/6/306)

#### Referência: M. Del-Coco, M. Leo e Pierluigi Carcagni, "Machine Learning for Smart Irrigation in Agriculture: How Far along Are We?", Information, vol. 15, no. 6, pp. 306, Maio 2024.

Alta acurácia mas há um grande desafio em integrar com sistemas já existentes

---

## Outros autores citados na tabela

#### [11] -  B. Johnson e D. Lee, "Weather Forecasting with Decision Trees for Smart Irrigation", Journal of Agriculture IoT, vol. 54, pp. 237-245, Jun. 2021

#### [12] -   C. Zhao, E. Garcia e F. Chang, "Artificial Neural Network-Guided Irrigation System for Optimizing Water Use", IEEE Internet of Things Journal, vol. 9, no. 2, pp. 1258-1265, Fev. 2023. 

#### [13] -  J. Patel, "K-Nearest Neighbors Model for Soil Moisture Control", Sensors and Actuators A: Physical, vol. 47, pp. 102-110, 2022.


#### [14] -  T. Singh e S. Verma, "Artificial Neural Network-Based Smart Irrigation System", IEEE Sensors Journal, vol. 11, no. 6, pp. 874-883, Jul. 2023. 

#### [21] -  M. Khan e H. Zhou, "Hybrid Sensor and Climate Model for Precision Irrigation", Agricultural Informatics Journal, vol. 11, pp. 324-333, 2023.
