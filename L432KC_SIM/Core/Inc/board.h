/*
 * board.h
 *
 *  Created on: Jul 6, 2024
 *      Author: andrei
 */
/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __BOARD_H
#define __BOARD_H
#define VERSION "0.1.1 2024-08-07"// 
/* Private define ------------------------------------------------------------*/
//	Pins
#define LED_GREEN 	GPIOA,GPIO_PIN_12
#define LED_RED 	GPIOA,GPIO_PIN_7
#define LED_BLUE 	GPIOA,GPIO_PIN_6
#define SIM_A1		GPIOA,GPIO_PIN_11
#define SIM_A0		GPIOA,GPIO_PIN_8
#define SIM_5V		GPIOC,GPIO_PIN_14// Arduino D7
#define SIM_SW_STATE 	GPIOB,GPIO_PIN_1
#define SIM_POW_GOOD	GPIOB,GPIO_PIN_6
#define SIM_BUSY	GPIOB,GPIO_PIN_7
#define SIM_CS		GPIOB,GPIO_PIN_0
#define SIM_WTX		GPIOA,GPIO_PIN_9
#define SIM_WRX		GPIOA,GPIO_PIN_10

//	Defines
#define SER_START_CHAR '<' //Start character for serial communication
#define SER_END_CHAR '>'   //End character for serial communication
//#define POLLING_INTERVAL 10 //ms
//#define REPORTING_INTERVAL 1000//ms

#define UART1_RXSIZE 16
#define DATACHUNKSIZE 8
#define UART1_TXSIZE 10*DATACHUNKSIZE+4//10 chars per word + <> and \n
struct DATACHUNK{
  uint8_t len;
  uint8_t size;
  int32_t buf[DATACHUNKSIZE];
};
struct BOARD_RESULT{
  uint32_t sampleCount;	//Number of ADC samples, taken since last report
  uint32_t n;		//Number of samples, accumulated for statistics calculation
  int32_t mean;		//10 * Mean
  int32_t stdev;	//10 * Standard deviation
  int32_t peak2peak;	//Peak-to-peak amplitude
};
/* Private function prototypes -----------------------------------------------*/
void dbgUart_printf(const char* format, ...);
void uart_printf(const char* format, ...);
void printe(char* msg);

void board_init(SPI_HandleTypeDef* hspiPtr);
void board_process_cmd(char *buf);
int board_acquire_sample();// called in the main loop
struct BOARD_RESULT board_report();//

extern uint16_t pollingInterval;
extern uint16_t reportingInterval;
extern uint8_t recState;

#endif /* __BOARD_H */



