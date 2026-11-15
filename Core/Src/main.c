
/* USER CODE BEGIN Header */
/**
  * @file           : main.c
  * @brief          : PCF8591 - Comunicacao I2C e USART2
  */
/* USER CODE END Header */

#include "main.h"

/* USER CODE BEGIN Includes */
#include <stdio.h>
#include <string.h>
#include <stdint.h>
/* USER CODE END Includes */

/* Private variables */
I2C_HandleTypeDef hi2c1;
UART_HandleTypeDef huart2;

/* USER CODE BEGIN PV */

/* Valores dos sensores */
volatile uint8_t LDR = 0;
volatile uint8_t Temp = 0;
volatile uint8_t Pot = 0;

/* Estados da maquina I2C */
typedef enum {
    I2C_IDLE,
    I2C_WAIT_TX,
    I2C_WAIT_RX,
    I2C_DONE,
    I2C_ERROR
} I2C_State;

typedef enum {
    I2C_OP_ADC,
    I2C_OP_DAC
} I2C_Operation;

volatile I2C_State i2c_state = I2C_IDLE;
volatile I2C_Operation i2c_operation = I2C_OP_ADC;

uint32_t i2c_error_code = 0;
uint32_t i2c_start_status = 0;

uint8_t i2c_control = 0;
uint8_t adc_buffer[2];
uint8_t dac_buffer[2];

uint8_t current_channel = 0;
uint8_t dac_value = 128;

/* Recepcao de comandos UART */
#define COMMAND_BUFFER_SIZE 50

uint8_t uart_rx_char;
char command_buffer[COMMAND_BUFFER_SIZE];

volatile uint8_t command_index = 0;
volatile uint8_t command_ready = 0;

/* USER CODE END PV */

/* Private function prototypes */
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_I2C1_Init(void);
static void MX_USART2_UART_Init(void);

/* USER CODE BEGIN PFP */
void UART_Start_Receive(void);
void UART_Send(char *message);
void PCF8591_StartRead(uint8_t channel);
void PCF8591_StartWriteDAC(uint8_t value);
void ProcessCommand(char *command);
/* USER CODE END PFP */

/* Private user code */

/* USER CODE BEGIN 0 */

/* Envia uma mensagem pelo terminal */
void UART_Send(char *message)
{
    HAL_UART_Transmit(
        &huart2,
        (uint8_t *)message,
        strlen(message),
        1000
    );
}

/* Inicia a recepcao UART por interrupcao */
void UART_Start_Receive(void)
{
    HAL_UART_Receive_IT(&huart2, &uart_rx_char, 1);
}

/* Inicia uma leitura ADC */
void PCF8591_StartRead(uint8_t channel)
{
    if ((channel != 0 && channel != 1 && channel != 3) ||
        i2c_state != I2C_IDLE)
    {
        return;
    }

    current_channel = channel;
    i2c_control = channel;
    i2c_operation = I2C_OP_ADC;

    i2c_error_code = 0;
    i2c_start_status = 0;
    i2c_state = I2C_WAIT_TX;

    HAL_StatusTypeDef status = HAL_I2C_Master_Transmit_IT(
        &hi2c1,
        (0x48 << 1),
        &i2c_control,
        1
    );

    if (status != HAL_OK)
    {
        i2c_start_status = status;
        i2c_state = I2C_ERROR;
    }
}

/* Inicia uma escrita no DAC */
void PCF8591_StartWriteDAC(uint8_t value)
{
    if (i2c_state != I2C_IDLE)
    {
        return;
    }

    dac_value = value;

    dac_buffer[0] = 0x40;
    dac_buffer[1] = value;

    i2c_operation = I2C_OP_DAC;

    i2c_error_code = 0;
    i2c_start_status = 0;
    i2c_state = I2C_WAIT_TX;

    HAL_StatusTypeDef status = HAL_I2C_Master_Transmit_IT(
        &hi2c1,
        (0x48 << 1),
        dac_buffer,
        2
    );

    if (status != HAL_OK)
    {
        i2c_start_status = status;
        i2c_state = I2C_ERROR;
    }
}

/* Interpreta os comandos recebidos */
void ProcessCommand(char *command)
{
    if (strcmp(command, "Read_AIN0") == 0)
    {
        PCF8591_StartRead(0);
    }
    else if (strcmp(command, "Read_AIN1") == 0)
    {
        PCF8591_StartRead(1);
    }
    else if (strcmp(command, "Read_AIN3") == 0)
    {
        PCF8591_StartRead(3);
    }
    else if (strncmp(command, "Set_DAC_", 8) == 0)
    {
        uint16_t value = 0;
        uint8_t valid = 1;

        if (command[8] == '\0')
        {
            valid = 0;
        }

        for (uint8_t i = 8; command[i] != '\0' && valid; i++)
        {
            if (command[i] < '0' || command[i] > '9')
            {
                valid = 0;
                break;
            }

            uint8_t digit = command[i] - '0';

            if (value > (255 - digit) / 10)
            {
                valid = 0;
                break;
            }

            value = value * 10 + digit;
        }

        if (valid)
        {
            PCF8591_StartWriteDAC((uint8_t)value);
        }
        else
        {
            UART_Send("Valor DAC invalido! Use 0 a 255.\r\n");
        }
    }
    else
    {
        UART_Send("Comando invalido!\r\n");
    }
}

/* Callback de recepcao UART */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART2)
    {
        if (command_ready == 0)
        {
            if (uart_rx_char == '\r' || uart_rx_char == '\n')
            {
                if (command_index > 0)
                {
                    command_buffer[command_index] = '\0';
                    command_ready = 1;
                }
            }
            else if (command_index < COMMAND_BUFFER_SIZE - 1)
            {
                command_buffer[command_index++] = uart_rx_char;
            }
        }

        HAL_UART_Receive_IT(&huart2, &uart_rx_char, 1);
    }
}

/* Callback de transmissao I2C */
void HAL_I2C_MasterTxCpltCallback(I2C_HandleTypeDef *hi2c)
{
    if (hi2c->Instance == I2C1 && i2c_state == I2C_WAIT_TX)
    {
        if (i2c_operation == I2C_OP_ADC)
        {
            i2c_state = I2C_WAIT_RX;

            HAL_StatusTypeDef status = HAL_I2C_Master_Receive_IT(
                &hi2c1,
                (0x48 << 1),
                adc_buffer,
                2
            );

            if (status != HAL_OK)
            {
                i2c_start_status = status;
                i2c_state = I2C_ERROR;
            }
        }
        else if (i2c_operation == I2C_OP_DAC)
        {
            i2c_state = I2C_DONE;
        }
    }
}

/* Callback de recepcao I2C */
void HAL_I2C_MasterRxCpltCallback(I2C_HandleTypeDef *hi2c)
{
    if (hi2c->Instance == I2C1 && i2c_state == I2C_WAIT_RX)
    {
        i2c_state = I2C_DONE;
    }
}

/* Callback de erro I2C */
void HAL_I2C_ErrorCallback(I2C_HandleTypeDef *hi2c)
{
    if (hi2c->Instance == I2C1)
    {
        i2c_error_code = HAL_I2C_GetError(hi2c);
        i2c_state = I2C_ERROR;
    }
}

/* USER CODE END 0 */

/**
  * @brief The application entry point.
  */
int main(void)
{
    HAL_Init();

    SystemClock_Config();
    MX_GPIO_Init();
    MX_I2C1_Init();
    MX_USART2_UART_Init();

    /* USER CODE BEGIN 2 */
    UART_Start_Receive();
    /* USER CODE END 2 */

    /* Infinite loop */
    /* USER CODE BEGIN WHILE */
    while (1)
    {
        /* Operacao I2C concluida */
        if (i2c_state == I2C_DONE)
        {
            char mensagem[50];

            if (i2c_operation == I2C_OP_ADC)
            {
                uint8_t value = adc_buffer[1];

                switch (current_channel)
                {
                    case 0:
                        LDR = value;
                        snprintf(mensagem, sizeof(mensagem),
                                 "AIN0 (LDR): %u\r\n", (unsigned int)LDR);
                        break;

                    case 1:
                        Temp = value;
                        snprintf(mensagem, sizeof(mensagem),
                                 "AIN1 (Temp): %u\r\n", (unsigned int)Temp);
                        break;

                    case 3:
                        Pot = value;
                        snprintf(mensagem, sizeof(mensagem),
                                 "AIN3 (Pot): %u\r\n", (unsigned int)Pot);
                        break;
                }
            }
            else
            {
                snprintf(mensagem, sizeof(mensagem),
                         "Valor do DAC: %u\r\n", (unsigned int)dac_value);
            }

            UART_Send(mensagem);
            i2c_state = I2C_IDLE;
        }

        /* Erro na comunicacao I2C */
        if (i2c_state == I2C_ERROR)
        {
            char mensagem[60];

            snprintf(mensagem, sizeof(mensagem),
                     "Erro I2C: status=%lu, erro=0x%lX\r\n",
                     (unsigned long)i2c_start_status,
                     (unsigned long)i2c_error_code);

            UART_Send(mensagem);
            i2c_state = I2C_IDLE;
        }

        /* Processa um novo comando quando o I2C estiver livre */
        if (command_ready && i2c_state == I2C_IDLE)
        {
            char command[COMMAND_BUFFER_SIZE];

            __disable_irq();

            strcpy(command, command_buffer);
            command_ready = 0;
            command_index = 0;

            __enable_irq();

            ProcessCommand(command);
        }
    }
    /* USER CODE END WHILE */
}

/**
  * @brief System Clock Configuration
  */
void SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1);

    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
    RCC_OscInitStruct.HSIState = RCC_HSI_ON;
    RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;

    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
    RCC_OscInitStruct.PLL.PLLM = 1;
    RCC_OscInitStruct.PLL.PLLN = 10;
    RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV7;
    RCC_OscInitStruct.PLL.PLLQ = RCC_PLLQ_DIV2;
    RCC_OscInitStruct.PLL.PLLR = RCC_PLLR_DIV2;

    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
    {
        Error_Handler();
    }

    RCC_ClkInitStruct.ClockType =
        RCC_CLOCKTYPE_HCLK |
        RCC_CLOCKTYPE_SYSCLK |
        RCC_CLOCKTYPE_PCLK1 |
        RCC_CLOCKTYPE_PCLK2;

    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_4) != HAL_OK)
    {
        Error_Handler();
    }
}

/**
  * @brief I2C1 Initialization Function
  */
static void MX_I2C1_Init(void)
{
    hi2c1.Instance = I2C1;
    hi2c1.Init.Timing = 0x10D19CE4;
    hi2c1.Init.OwnAddress1 = 0;
    hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
    hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
    hi2c1.Init.OwnAddress2 = 0;
    hi2c1.Init.OwnAddress2Masks = I2C_OA2_NOMASK;
    hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
    hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;

    if (HAL_I2C_Init(&hi2c1) != HAL_OK)
    {
        Error_Handler();
    }

    if (HAL_I2CEx_ConfigAnalogFilter(&hi2c1, I2C_ANALOGFILTER_ENABLE) != HAL_OK)
    {
        Error_Handler();
    }

    if (HAL_I2CEx_ConfigDigitalFilter(&hi2c1, 0) != HAL_OK)
    {
        Error_Handler();
    }
}

/**
  * @brief USART2 Initialization Function
  */
static void MX_USART2_UART_Init(void)
{
    huart2.Instance = USART2;
    huart2.Init.BaudRate = 115200;
    huart2.Init.WordLength = UART_WORDLENGTH_8B;
    huart2.Init.StopBits = UART_STOPBITS_1;
    huart2.Init.Parity = UART_PARITY_NONE;
    huart2.Init.Mode = UART_MODE_TX_RX;
    huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart2.Init.OverSampling = UART_OVERSAMPLING_16;
    huart2.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
    huart2.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;

    if (HAL_UART_Init(&huart2) != HAL_OK)
    {
        Error_Handler();
    }
}

/**
  * @brief GPIO Initialization Function
  */
static void MX_GPIO_Init(void)
{
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
}

/**
  * @brief Error Handler
  */
void Error_Handler(void)
{
    __disable_irq();

    while (1)
    {
    }
}

#ifdef USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line)
{
    /* Tratamento opcional de assert */
}
#endif
