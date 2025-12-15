/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2024 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#include "board.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

#define false 0
#define true 1
#define CR 13
#define LF 10
#define LED(signal) ((signal) == 0)
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */
/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
SPI_HandleTypeDef hspi1;

UART_HandleTypeDef huart1;
UART_HandleTypeDef huart2;

/* USER CODE BEGIN PV */
uint32_t cycle = 0;
struct {
  uint8_t len;
  uint8_t size;
  char buf[UART1_TXSIZE];
}uart_out = {0,UART1_TXSIZE,""};

/*huart2 is for debugging output, it communicates through USB (STLink).
 *To connect from a host: python3 -m serial.tools.miniterm /dev/ttyACM0 115200
 */
#define UART2_TXSIZE 80
struct {
  uint8_t len;
  uint8_t size;
  char buf[UART2_TXSIZE];
}dbgUart_out = {0,UART2_TXSIZE,""};

// huart1 is device communication link
struct {
  uint8_t len;
  uint8_t size;
  char buf[UART1_RXSIZE];
}input_cmd = {0,UART1_RXSIZE,""};

int command_received = 0;
int RX_overrun = 0;
uint32_t lastTickMS = 0;

// global variables used in other programs
uint16_t reportingInterval = 1000;
uint16_t pollingInterval = 160;
uint32_t tickMS;// Tick count in milliseconds
struct DATACHUNK datachunk = {0,DATACHUNKSIZE};
uint8_t dbg = 1;// 0:no debugging, bit0:debug output to dbgUart, bits[1:7] debugging level
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_USART1_UART_Init(void);
static void MX_SPI1_Init(void);
/* USER CODE BEGIN PFP */
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
void uart_transmit(uint8_t len){
  uart_out.len = len;
  if (HAL_UART_Transmit(&huart1, (uint8_t*)&uart_out.buf, len,
			HAL_MAX_DELAY) != HAL_OK) {
    HAL_GPIO_TogglePin(LED_RED);
  }
}
void dbgUart_printf(const char* format, ...){
  //printf to debugging UART
  if ((dbg&1) == 0)
    return;
  va_list argptr;
  va_start(argptr, format);
  vsnprintf(dbgUart_out.buf, dbgUart_out.size, format, argptr);
  va_end(argptr);
  if (HAL_UART_Transmit(&huart2, (uint8_t*)&dbgUart_out.buf,
			strlen(dbgUart_out.buf), HAL_MAX_DELAY) != HAL_OK) {
      __NOP();// to place breakpoint
  }
}
void uart_printf(const char* format, ...){
  //printf to main UART
  if (dbg == 0)
    return;
  va_list argptr;
  va_start(argptr, format);
  vsnprintf(uart_out.buf, uart_out.size, format, argptr);
  va_end(argptr);
  if ((dbg&0xFE) != 0)
    uart_transmit(strlen(uart_out.buf));
  dbgUart_printf("%s",uart_out.buf);
}

void send_toPIM(const char* prefix, int32_t* data, uint8_t len){
  // Send PIM-formatted int32_t data to main UART
  // Data will be prepended with suffix and appended with '>\n'
  uint8_t l;
  strcpy(uart_out.buf,prefix);
  for (int i=0; i<len; i++){
      l = strlen(uart_out.buf);
      snprintf(uart_out.buf+l, uart_out.size-l, "%li,", data[i]);
  }
  strncat(uart_out.buf, ">\n", uart_out.size);
  uart_transmit(strlen(uart_out.buf));
}
void printe(char* msg){
  //send error message to main UART
  snprintf(uart_out.buf, uart_out.size, "ERR: %s\n", msg);
  uart_transmit(strlen(uart_out.buf));
}
int wait_input(uint16_t timeout){
  //Returns 0: command was processed, 1: timeout, -1: filling of command buffer is not finished, -2: error
  uint8_t byte_received = 0;
  if(HAL_UART_Receive(&huart1, &byte_received, 1, timeout)!=HAL_OK) //if timeout
    return 1;

  HAL_GPIO_TogglePin(LED_BLUE);
  if (dbg&0x80)
    uart_printf("char: %u\n", byte_received);

  if (input_cmd.len >= input_cmd.size){
      printe("uart1 RX overrun");
      input_cmd.len = 0;
      return -2;
  //}else if (byte_received == CR){
  //    input_cmd.buf[input_cmd.len] = 0;
  }else if (byte_received == SER_END_CHAR){
      input_cmd.buf[input_cmd.len] = 0;
      board_process_cmd(input_cmd.buf);
      input_cmd.len = 0;
      return 0;
  }else if (byte_received == SER_START_CHAR){
      input_cmd.len = 0;
      return -1;
  }else{
      input_cmd.buf[input_cmd.len] = byte_received;
  }
  input_cmd.len ++;
  return -1;
}
/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{
  /* USER CODE BEGIN 1 */
  struct BOARD_RESULT statistics;
  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */
  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */
  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_USART2_UART_Init();
  MX_USART1_UART_Init();
  MX_SPI1_Init();
  /* USER CODE BEGIN 2 */
  board_init(&hspi1);
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    // Wait for 1 char from huart1
    //HAL_Delay(1000);
    wait_input(pollingInterval);
    tickMS =  HAL_GetTick();
    if (board_acquire_sample() != 0)
      continue;
    cycle++;
    // One ADC sample acquired and processed, update LEDs.
    HAL_GPIO_WritePin(LED_GREEN, LED(cycle&1));
    HAL_GPIO_WritePin(LED_BLUE, LED(cycle&2));
    HAL_GPIO_WritePin(LED_RED, LED(cycle&4));
    if (datachunk.len >= datachunk.size){
        // The chunk is full.
        send_toPIM("<R ", datachunk.buf, datachunk.len);
        datachunk.len = 0;
    }
    if ((tickMS - lastTickMS) > reportingInterval){
      // Reporting interval has elapsed. Send statistics.
      lastTickMS = tickMS;
      uart_printf("cycle %li, @%i ms, recState:%i\n", cycle, tickMS, recState);
      statistics = board_report();
      uart_printf("mean*10(%i)=%i, stdev*10=%i, p2p=%i\n", statistics.n, statistics.mean, statistics.stdev, statistics.peak2peak);
      send_toPIM("<M ", (int32_t*)&statistics, 5);
    }
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  if (HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_MSI;
  RCC_OscInitStruct.MSIState = RCC_MSI_ON;
  RCC_OscInitStruct.MSICalibrationValue = 0;
  RCC_OscInitStruct.MSIClockRange = RCC_MSIRANGE_6;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_MSI;
  RCC_OscInitStruct.PLL.PLLM = 1;
  RCC_OscInitStruct.PLL.PLLN = 16;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV7;
  RCC_OscInitStruct.PLL.PLLQ = RCC_PLLQ_DIV2;
  RCC_OscInitStruct.PLL.PLLR = RCC_PLLR_DIV2;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_1) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief SPI1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_SPI1_Init(void)
{

  /* USER CODE BEGIN SPI1_Init 0 */
  /* USER CODE END SPI1_Init 0 */

  /* USER CODE BEGIN SPI1_Init 1 */
  /* USER CODE END SPI1_Init 1 */
  /* SPI1 parameter configuration*/
  hspi1.Instance = SPI1;
  hspi1.Init.Mode = SPI_MODE_MASTER;
  hspi1.Init.Direction = SPI_DIRECTION_2LINES;
  hspi1.Init.DataSize = SPI_DATASIZE_8BIT;
  hspi1.Init.CLKPolarity = SPI_POLARITY_LOW;
  hspi1.Init.CLKPhase = SPI_PHASE_1EDGE;
  hspi1.Init.NSS = SPI_NSS_SOFT;
  hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_64;
  hspi1.Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi1.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi1.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi1.Init.CRCPolynomial = 7;
  hspi1.Init.CRCLength = SPI_CRC_LENGTH_DATASIZE;
  hspi1.Init.NSSPMode = SPI_NSS_PULSE_ENABLE;
  if (HAL_SPI_Init(&hspi1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN SPI1_Init 2 */
  /* USER CODE END SPI1_Init 2 */

}

/**
  * @brief USART1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART1_UART_Init(void)
{

  /* USER CODE BEGIN USART1_Init 0 */

  /* USER CODE END USART1_Init 0 */

  /* USER CODE BEGIN USART1_Init 1 */

  /* USER CODE END USART1_Init 1 */
  huart1.Instance = USART1;
  huart1.Init.BaudRate = 57600;
  huart1.Init.WordLength = UART_WORDLENGTH_8B;
  huart1.Init.StopBits = UART_STOPBITS_1;
  huart1.Init.Parity = UART_PARITY_NONE;
  huart1.Init.Mode = UART_MODE_TX_RX;
  huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart1.Init.OverSampling = UART_OVERSAMPLING_16;
  huart1.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  huart1.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
  if (HAL_UART_Init(&huart1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART1_Init 2 */

  /* USER CODE END USART1_Init 2 */

}

/**
  * @brief USART2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART2_UART_Init(void)
{

  /* USER CODE BEGIN USART2_Init 0 */

  /* USER CODE END USART2_Init 0 */

  /* USER CODE BEGIN USART2_Init 1 */

  /* USER CODE END USART2_Init 1 */
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
  /* USER CODE BEGIN USART2_Init 2 */

  /* USER CODE END USART2_Init 2 */

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
/* USER CODE BEGIN MX_GPIO_Init_1 */
/* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOA, LED_BLUE_Pin|LED_RED_Pin|LED_GREEN_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, CS_Pin|SW_STATE_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pins : LED_BLUE_Pin LED_RED_Pin LED_GREEN_Pin */
  GPIO_InitStruct.Pin = LED_BLUE_Pin|LED_RED_Pin|LED_GREEN_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pins : CS_Pin SW_STATE_Pin */
  GPIO_InitStruct.Pin = CS_Pin|SW_STATE_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pins : A0_Pin A1_Pin */
  GPIO_InitStruct.Pin = A0_Pin|A1_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pins : POW_GOOD_Pin BUSY_Pin */
  GPIO_InitStruct.Pin = POW_GOOD_Pin|BUSY_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

/* USER CODE BEGIN MX_GPIO_Init_2 */
/* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */
/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
