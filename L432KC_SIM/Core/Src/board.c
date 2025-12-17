/*
 * board.c
 *
 *  Created on: Jul 6, 2024
 *      Author: andrei
 */
#include "main.h"
#include <string.h>
#include <stdlib.h>// for atoi
#include <math.h>

#include "board.h"

#define HIGH 1
#define LOW 0
#define false 0
#define true 1
#define MIN32 -2147483648
#define MAX32 2147483647

/* Private variables ---------------------------------------------------------*/
/* -------------------------------------------------------- Enumerations  */
enum state_enum { INITIAL, START_ADC, COLLECT, ERR };    // Three different machine states

// --- States
//enum state_enum sysState = INITIAL; // State of machine
uint16_t recLimit = 800; 	// Maximum samples to transmit to PIM during reportInterval
//static uint8_t interlockState = LOW;   // Indicates when chassis interlock is bad.
char header = 'I';              // Indicates the SIM state to the PIM
//static uint8_t readingState = false;
//static uint8_t newDataFromSerial = false;
#define SPI_INBUF_SIZE 4
static uint8_t spi_buf[10] = {0xa,0x5,0xa,0x5,0xa,0x5,0xa,0x5,0xa,0x5};// Input SPI buffer

static uint8_t spi_sdo[SPI_INBUF_SIZE] = {0x78, 0x00, 0x00, 0x00};//set OSR to 32768, 6.9Hz sampling rate
static uint8_t samplingRate[10] = {0x78,0x48,0x40,0x38,0x30,0x28,0x20,0x18,0x10,0x08};
// 6.9Hz:0x78, 880Hz:0x18
static uint8_t srate = 0;

static SPI_HandleTypeDef* hspiPtr;
static HAL_StatusTypeDef halStatus;

extern uint32_t tickMS;
extern struct DATACHUNK datachunk;
extern uint8_t dbg;

static char legalCommands[] = "Legal commands: S N, STS?, R N, 5V N, TO N, RI N, DBG N";
//static char disableEnable[2][8] = {"Disable\0","Enable \0"};
static uint32_t sampleCount = 0;

int32_t decode_adc(){
  //return adcValue from spi_buf[0:3]
  uint32_t val = 0;
  if (((spi_buf[0]) & 0x80) != 0)//Conversion not finished
    return 0x80000001;
  else if (spi_buf[0] & 0x40)// Error. It should be 0
    return 0x80000002;
  else if ((spi_buf[0] & 0x30) == 0x30)// Overflow
    return 0x7fffffff;
  else if ((spi_buf[0] & 0x30) == 0)// Underflow
    return 0x80000000;
  else{
  if (dbg&4) uart_hexdump("DBG4:SPI buf:",spi_buf, 4);
  val = (spi_buf[0]&0x1f) << 8;//bits 19:23
  val = (val | (spi_buf[1])) << 8;//bits 11:18
  val = (val | (spi_buf[2])) << 3;//bits 03:10
  val = val | ((spi_buf[3]>>5)&0x7);  //bits 0:2
  val = val << 8;
  }
  return val;
}
void clear_statistics();

// ---------------------------------------------------- report_status()
void report_status(){
  /* Description: Provides current status of the input variables
   *              used to determine the state of the machine.
   */
  uart_printf("SR:%i,TO:%i,RL:%i,T:%i,V:%s",
      srate, receiveTimeout, recLimit, HAL_GetTick(), VERSION);
}
void board_init(SPI_HandleTypeDef* hptr){
  uint8_t tdbg = dbg;
  HAL_GPIO_WritePin(SIM_CS, GPIO_PIN_SET);
  dbg = 1;
  report_status();
  dbg = tdbg;
  hspiPtr = hptr;
  HAL_GPIO_WritePin(LED_GREEN, GPIO_PIN_RESET);
  HAL_Delay(1000);
  HAL_GPIO_WritePin(LED_GREEN, GPIO_PIN_SET);
}

/* Private user code ---------------------------------------------------------*/
void board_process_cmd(char *buf){
  char *strtokIndx;
  //char cmd[UART1_RXSIZE] = {0};
  char *cmd;
  int value = 0;
  //uart_printf("Command: %s\n", buf);
  // ---------------------------- Grab Command
  strtokIndx = strtok(buf," ");
  //strcpy( cmd, strtokIndx );
  cmd = strtokIndx;
  // ---------------------------- Grab value
  strtokIndx = strtok(NULL,",");
  value = atoi(strtokIndx);

  // ---------------------------- Evaluate Command
  //uart_printf("Parsed: %s, %i", cmd, value);
  // ----------------------------- Change the sample rate: NOT IMPLEMENTED
  if (strcmp(cmd,"S") == 0){
      if ((value>7) | (value<0)){
	printe("ADC sampling rate selector should be 0:7");
	return;
      }
      uart_printf("Changing ADC sample rate from %i to %i",srate, value);
      srate = value;
      spi_sdo[0] = samplingRate[srate];
  // ----------------------------- Transmit status on Serial line
  }else if (strcmp(cmd,"STS?") == 0){
      report_status();
  // ----------------------------- Turn on sending data to PIM
  }else if (strcmp(cmd,"R") == 0){
      if (value == 0)
	uart_printf("Disabled transmission of ADC samples to PIM");
      else
	uart_printf("Enabled transmission of max %i ADC samples to PIM",value);
      recLimit = value;
  }else if (strcmp(cmd,"5V") == 0){
      HAL_GPIO_WritePin(SIM_5V, ((value) != 0));
  // ----------------------------- Turn on debugging
  }else if (strcmp(cmd,"DBG") == 0){
      dbg = (uint8_t)value;
      uart_printf("DBG is set to %u", dbg);
  }else if (strcmp(cmd,"TO") == 0){
      uart_printf("Timeout changed from %i to %i",receiveTimeout, value);
      receiveTimeout = value;
  }else if (strcmp(cmd,"RI") == 0){
      if (value < 1000){
	  printe("Reporting interval should be 1000 or more");
	  return;
      }
      uart_printf("Reporting interval changed from %i to %i",reportingInterval, value);
      reportingInterval = value;
  }else if (strcmp(cmd,"G") == 0){
      if ((value<0) | (value>3)){
	  printe("Gain selection should be 0:3");
	  return;
      }
      int a0 = HAL_GPIO_ReadPin(SIM_A0);
      int a1 = HAL_GPIO_ReadPin(SIM_A1);
      HAL_GPIO_WritePin(SIM_A0, value&1);
      HAL_GPIO_WritePin(SIM_A1, value&2);
      uart_printf("Gain changed from %i to %i", a1*2+a0, value);
  }else{
      printe(legalCommands);
  }
}
struct BOARD_RESULT statistics;

#define STAT_MAXSAMPLES 64
struct {
  uint32_t count;
  int32_t min,max;
  int32_t sample[STAT_MAXSAMPLES];
}statStorage;

void accumulate_statistics(int32_t val){
  if (statStorage.count >= STAT_MAXSAMPLES)
    return;
  if (val == -1)//There is some mistique with this value
    return;
  statStorage.sample[statStorage.count++] = val;
}
void clear_statistics(){
  statStorage.count = 0;
  sampleCount = 0;
  statStorage.min = MAX32;
  statStorage.max = MIN32;
}

void print_statStorage(){
  uart_printf("DBG4: StatStorage=[");
  for (int i=0; i<statStorage.count;i++)
      uart_printf("%i,",statStorage.sample[i]);
  uart_printf("]\n");
}

void calculate_statistics()
{
  int32_t sum = 0, i, istart = 0, iv;
  float mean,fsum=0.,val;
  if (statStorage.count == STAT_MAXSAMPLES) istart = 4;// First 4 sample could be distorted due to EMI
  statistics.n = statStorage.count - istart;
  for (i=istart; i<statStorage.count; i++){
      iv = statStorage.sample[i];
      sum += iv;
      if (statStorage.min > iv)
        statStorage.min = iv;
      if (statStorage.max < iv)
        statStorage.max = iv;
  }
  mean = (float)sum/statistics.n;
  for (i=istart; i<statStorage.count; i++){
      val = (statStorage.sample[i] - mean);
      fsum += val*val;
  }
  statistics.sampleCount = sampleCount;
  statistics.mean = (int32_t)(mean*10);
  statistics.stdev = sqrtf(fsum/statistics.n)*10;
  statistics.peak2peak = statStorage.max - statStorage.min;
  if (dbg&4) print_statStorage();
  clear_statistics();
}
int conversion_finished(){
  //Check if conversion finished
  HAL_GPIO_WritePin(SIM_CS, GPIO_PIN_RESET);
  int busy = HAL_GPIO_ReadPin(SIM_MISO);
  //HAL_GPIO_WritePin(SIM_CS, GPIO_PIN_SET);
  return !busy;
}

int board_acquire_sample(){
  int32_t val = 0;
  //HAL_GPIO_WritePin(SIM_CS, GPIO_PIN_RESET);
  if (!conversion_finished()){
      HAL_GPIO_WritePin(SIM_CS, GPIO_PIN_SET);
      return 1;
  }
  //HAL_GPIO_WritePin(SIM_CS, GPIO_PIN_RESET);
  halStatus = HAL_SPI_TransmitReceive(hspiPtr, spi_sdo, spi_buf, SPI_INBUF_SIZE, 100);
  if (halStatus != HAL_OK){
      printe("TransmitReceive not OK");
      uart_printf("TransmitReceive: %h\n",halStatus);
      return 3;
  }
  // SDO should be high, indicating start of new conversion
  val = HAL_GPIO_ReadPin(SIM_MISO);
  if (val == 0){
      printe("Conversion delayed");
  }
  HAL_GPIO_WritePin(SIM_CS, GPIO_PIN_SET);
  val = decode_adc();
  if (val == 0x80000001){
    // This should not happen
    //if (dbg)
    printe("Conversion not finished\n");
    return 1;
  }
  else if (val == 0x80000002){
    //never happened
    uart_printf("ADC format error\n");
    return 2;
  }
  sampleCount++;
  val = val >> 8;

  accumulate_statistics(val);

  if (sampleCount < recLimit){
    //fill data chunk
    if (datachunk.len < datachunk.size){
	datachunk.buf[datachunk.len] = val;
	datachunk.len++;
    }else{
	// should never happen
	printe("Logic: datachunk was not dispatched\n");
    }
  }
  return 0;
}

struct BOARD_RESULT board_report(){
  statistics.clockCount = HAL_GetTick();
  calculate_statistics();
  return statistics;
}

