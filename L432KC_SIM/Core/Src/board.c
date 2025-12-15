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
//uint8_t adcState = LOW;     	// Indicates when to retrieve the data from ADC
uint8_t recState = HIGH; 	// Indicates when the receiver is ready for data
static uint8_t interlockState = LOW;   // Indicates when chassis interlock is bad.
char header = 'I';              // Indicates the SIM state to the PIM
//static uint8_t readingState = false;
//static uint8_t newDataFromSerial = false;
static uint8_t spi_buf[10] = {0xa,0x5,0xa,0x5,0xa,0x5,0xa,0x5,0xa,0x5};

static uint8_t spi_sdo[4] = {0x78, 0x00, 0x00, 0x00};//set OSR to 32768, 6.9Hz sampling rate
static uint8_t samplingRate[10] = {0x78,0x48,0x40,0x38,0x30,0x28,0x20,0x18,0x10,0x08};
// 6.9Hz:0x78, 880Hz:0x18
static uint8_t srate = 0;

static SPI_HandleTypeDef* hspiPtr;
static HAL_StatusTypeDef halStatus;

extern uint32_t tickMS;
extern struct DATACHUNK datachunk;
extern uint8_t dbg;

static char legalCommands[] = "Legal commands: <S N><STS?><R><5V N><D?>";
static char disableEnable[2][7] = {"disable","enable "};
static uint32_t sampleCount = 0;

int32_t decode_adc(){
  //return adcValue from spi_buf[0:3]
  uint32_t val = 0;
  if (((spi_buf[0]) & 0x80) != 0)//Conversion not finished
    return 0x80000001;
  else if (spi_buf[0] & 0x40)// Error. It should be 0
    return 0x80000002;
  else if ((spi_buf[0] & 0x30) == 0x30)
    return 0x7fffffff;
  else if ((spi_buf[0] & 0x30) == 0)
    return 0x80000000;
  else{
  val = (spi_buf[0]&0x1f) << 8;//bits 19:23
  val = (val | (spi_buf[1])) << 8;//bits 11:18
  val = (val | (spi_buf[2])) << 3;//bits 03:10
  val = val | ((spi_buf[3]>>5)&0x7);  //bits 0:2
  val = val << 8;
  }
  return val;
}
void clear_statistics();
void board_init(SPI_HandleTypeDef* hptr){
  HAL_GPIO_WritePin(SIM_CS, GPIO_PIN_SET);
  hspiPtr = hptr;
}

// ---------------------------------------------------- report_status()
void report_status(){
  /* Description: Provides current status of the input variables
   *              used to determine the state of the machine.
   */
  int32_t savedbg = dbg;
  dbg = 3;
  uart_printf("\nVer: %s,",VERSION);
  uart_printf("Time: %i,", tickMS);
  //uart_printf("System State: %i,", sysState);
  //uart_printf("ADC_State: %i,", adcState);
  uart_printf("Receiver: %i,", recState);
  uart_printf("Interlock: %i,", interlockState);
  uart_printf("Srate: %i",srate);
  uart_printf("END\n");
  dbg = savedbg;
}
/* Private user code ---------------------------------------------------------*/
void board_process_cmd(char *buf){
  char *strtokIndx;
  //char cmd[UART1_RXSIZE] = {0};
  char *cmd;
  int value = 0;
  uart_printf("Command: %s\n", buf);
  // ---------------------------- Grab Command
  strtokIndx = strtok(buf," ");
  //strcpy( cmd, strtokIndx );
  cmd = strtokIndx;
  // ---------------------------- Grab value
  strtokIndx = strtok(NULL,",");
  value = atoi(strtokIndx);

  // ---------------------------- Evaluate Command
  uart_printf("Parsed: %s, %i\n", cmd, value);
  // ----------------------------- Change the sample rate: NOT IMPLEMENTED
  if (strcmp(cmd,"S") == 0){
      uart_printf("Set ADC sample rate to %i\n",value);
      if ((value>10-1) | (value<0)){
	printe("ADC sampling rate selector should be 0:9");
	return;
      }
      srate = value;
      spi_sdo[0] = samplingRate[srate];
  // ----------------------------- Transmit status on Serial line
  }else if (strcmp(cmd,"STS?") == 0){
      uart_printf("Report status\n");
      report_status();
  // ----------------------------- Turn on sending data to PIM
  }else if (strcmp(cmd,"R") == 0){
      uart_printf("%s sending ADC samples to PIM\n",disableEnable[value]);
      recState = value;
  }else if (strcmp(cmd,"5V") == 0){
      HAL_GPIO_WritePin(SIM_5V, ((value) != 0));
  // ----------------------------- Turn on debugging
  }else if (strcmp(cmd,"DBG") == 0){
      dbg = (uint8_t)value;
      uart_printf("DBG is set to %u\n", dbg);
  }else if (strcmp(cmd,"PI") == 0){
      if (value < 1){
	  printe("Polling interval should be > 0\n");
	  return;
      }
      uart_printf("Polling interval changed from %i to %i\n",pollingInterval, value);
      pollingInterval = value;
  }else if (strcmp(cmd,"RI") == 0){
      uart_printf("Reporting interval changed from %i to %i\n",reportingInterval, value);
      reportingInterval = value;
  }else if (strcmp(cmd,"G") == 0){
      if ((value<0) | (value>3)){
	  printe("Gain selection should be 0:3");
	  return;
      }
      HAL_GPIO_WritePin(SIM_A0, value&1);
      HAL_GPIO_WritePin(SIM_A0, value&2);
  }else{
      printe(legalCommands);
  }
}
struct BOARD_RESULT statistics;

#define STAT_MAXSAMPLES 100
struct {
  uint32_t count;
  int32_t min,max;
  int32_t sample[STAT_MAXSAMPLES];
}stat_storage;

void accumulate_statistics(int32_t val){
  if (stat_storage.count >= STAT_MAXSAMPLES)
    return;
  stat_storage.sample[stat_storage.count++] = val;
  if (stat_storage.min > val)
    stat_storage.min = val;
  if (stat_storage.max < val)
    stat_storage.max = val;
}
void clear_statistics(){
  stat_storage.count = 0;
  sampleCount = 0;
  stat_storage.min = MAX32;
  stat_storage.max = MIN32;
}
void calculate_statistics()
{
  int32_t sum = 0, i;
  float mean,fsum=0.,val;
  for (i=0; i<stat_storage.count; i++){
      sum += stat_storage.sample[i];
  }
  mean = (float)sum/stat_storage.count;
  for (i=0; i<stat_storage.count; i++){
      val = (stat_storage.sample[i] - mean);
      fsum += val*val;
  }
  statistics.sampleCount = sampleCount;
  statistics.n = stat_storage.count;
  statistics.mean = (int32_t)(mean*10);
  statistics.stdev = sqrtf(fsum/stat_storage.count)*10;
  statistics.peak2peak = stat_storage.max - stat_storage.min;
  clear_statistics();
}

int board_acquire_sample(){
  int32_t val = 0;
  HAL_GPIO_WritePin(SIM_CS, GPIO_PIN_RESET);
  halStatus = HAL_SPI_TransmitReceive(hspiPtr, spi_sdo, spi_buf, 4, 100);
  HAL_GPIO_WritePin(SIM_CS, GPIO_PIN_SET);
  val = decode_adc();
  if (val == 0x80000001){
    uart_printf("Conversion not finished\n");
    return 1;
  }
  else if (val == 0x80000002){
    uart_printf("ADC format error\n");
    return 2;
  }
  sampleCount++;
  val = val >> 8;

  accumulate_statistics(val);

  if (recState != 0){
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
  calculate_statistics();
  return statistics;
}

