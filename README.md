# PCF8591 - Comunicação I2C com STM32

## Descrição

Projeto desenvolvido no contexto da disciplina Sistemas Embarcados 2026.2, utilizando a placa STM32 NUCLEO-L476RG e o módulo PCF8591 para implementar uma comunicação via protocolo I2C.

O sistema permite realizar leituras analógicas (ADC) e controlar uma saída analógica (DAC) por meio de comandos enviados pelo terminal serial.

## Hardware utilizado

- STM32 NUCLEO-L476RG
- Módulo PCF8591 (ADC/DAC)
- Protoboard e jumpers

## Funcionalidades

- Leitura do sensor LDR (AIN0).
- Leitura do termistor (AIN1).
- Leitura do potenciômetro (AIN3).
- Controle da saída DAC (AOUT).
- Recepção de comandos pelo terminal via USART2.
- Comunicação I2C não bloqueante utilizando interrupções.
- Máquina de estados para gerenciamento das operações I2C.

## Comandos disponíveis

| Comando | Função |
|---|---|
| `Read_AIN0` | Lê o LDR |
| `Read_AIN1` | Lê o termistor |
| `Read_AIN3` | Lê o potenciômetro |
| `Set_DAC_<valor>` | Define o valor da saída DAC (0–255) |

## Configuração da comunicação

- I2C1: 100 kHz
- Endereço I2C do PCF8591: 0x48
- USART2: 115200 baud
- Protocolo serial: 8N1

## Ferramentas

- STM32CubeIDE
- STM32CubeMX
- HAL Library

## Alunos
- Ian Teixeira
- Jean Felipe
- Márcio Henrique
