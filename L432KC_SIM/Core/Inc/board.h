/*
 * board.h
 *
 *  Created on: Jul 6, 2024
 *      Author: andrei
 */
/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __BOARD_H
#define __BOARD_H
#define VERSION "0.1.4"//2024-08-16 //STS shows pollingInterval
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
#define SIM_SCK		GPIOB,GPIO_PIN_3
#define SIM_MISO	GPIOB,GPIO_PIN_4
#define SIM_MOSI	GPIOB,GPIO_PIN_5
#define SIM_WTX		GPIOA,GPIO_PIN_9
#define SIM_WRX		GPIOA,GPIO_PIN_10

//	Defines
#define SER_START_CHAR '<' //Start character for serial communication
#define SER_END_CHAR '>'   //End character for serial communication
//#define POLLING_INTERVAL 10 //ms
//#define REPORTING_INTERVAL 1000//ms

#define UART1_RXSIZE 16
#define DATACHUNKSIZE 1// Larger chunks cause excessive noise and transfer speed is effectively lower because for single dtransfer the transfer delay overlaps with with conversion.
#define LARGEST(x,y) ( (x) > (y) ? (x) : (y) )
#define UART1_TXSIZE LARGEST(80,10*DATACHUNKSIZE+4)//10 chars per word + <> and \n
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
  uint32_t clockCount;	//milliseconds
};
/* Private function prototypes -----------------------------------------------*/
void dbgUart_printf(const char* format, ...);
void uart_printf(const char* format, ...);
void uart_hexdump(const char* prefix, uint8_t*, uint8_t len);
void printe(char* msg);

void board_init(SPI_HandleTypeDef* hspiPtr);
void board_process_cmd(char *buf);
int board_acquire_sample();// called in the main loop
struct BOARD_RESULT board_report();//

extern uint16_t receiveTimeout;
extern uint16_t reportingInterval;
extern uint16_t recLimit;

#endif /* __BOARD_H */



