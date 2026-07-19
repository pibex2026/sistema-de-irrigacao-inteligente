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

