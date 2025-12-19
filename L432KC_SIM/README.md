# PoF_SIM_L432
Firmware for Nucleo L432KC eval board for PoF SIM board.
Design environment: STM32CubeIde v1.14.0.

## Solder beads

SB9 Off to power from +5V<br>
Note: SB16 and SB18 are better be OFF, they connect PA6-PB6 and PA5-PB5.<br>

## Pinout
```
//-----------------+-------------------+----------------------------------------
//    Pins          STM32               Arduino
#define LED_GREEN   GPIOA,GPIO_PIN_12// D2
#define LED_RED     GPIOA,GPIO_PIN_7//  A6
#define LED_BLUE    GPIOA,GPIO_PIN_6//  A5
#define SIM_A1      GPIOA,GPIO_PIN_11// D10 OpAmp gain
#define SIM_A0      GPIOA,GPIO_PIN_8//  D9  OpAmp gain
#define SIM_5V      GPIOC,GPIO_PIN_14// D7  5V_SW. Occupied by RCC_OSC32_OUT
#define SIM_SW_STATE GPIOB,GPIO_PIN_1// D6
#define SIM_POW_GOOD GPIOB,GPIO_PIN_6// D5
#define SIM_BUSY    GPIOB,GPIO_PIN_7//  D4  Not needed. The Busy state can be determined by bit31 of data.
#define SIM_CS      GPIOB,GPIO_PIN_0//  D3
#define SIM_SCK     GPIOB,GPIO_PIN_3//  D13 On L432 it is connected to LD3 Green
#define SIM_MISO    GPIOB,GPIO_PIN_4//  D12
#define SIM_MOSI    GPIOB,GPIO_PIN_5//  D11
#define SIM_WTX     GPIOA,GPIO_PIN_9//  TX
#define SIM_WRX     GPIOA,GPIO_PIN_10// RX
//-----------------+-------------------+----------------------------------------
To control D7 the following switches required:
SB4-OFF, SB6-ON, SB5-OFF, SB7-OFF, SB8-ON/OFF
```
## Power Consumption
Firmware 0.1.4 2024-08-19
- Whole board, sampling rate 7Hz: **0.25W, 50mA**.
- Whole board sampling rate 800Hz:  **0.18W**.
- Standalone STM32L432, SB9 Off, Power from USB: 0.280W, 54mA
- Standalone STM32L432, SB9 Off, Power from external +5V: 0.060W, 11mA

## Commands
Communication interface: UART1.
All data are ASCII strings.
Format of input commands `<CMD VALUE>`:
List of legal commands:
- `<STS?>`: Request board status. The board will respond with following ASCII string:<br>
```
    Ver 0.1.4 2024-08-19: <TSR:0,TO:160,RL:0,T:1382570,V:0.1.4>
```
- `<S Value>`: Set sampling rate of the ADC, Value is in range [0:7].
- `<R Value>`: Set recLimit, number of samples to transmit to PIM during each reporting interval.
- `<RI Value>`: Set reporting interval in milliseconds, default 1000.
- `<TO Value>`: Timeout for receiving one character from PIM, it defines data rate.
The actual data delivery interval is the sum of timeout value and the ADC conversion time.
- `<+5V Value>`: Turn On/Off the +5V switch, legal values: 1/0'
- `<DBG Value>`: Debugging control. Bit0: enable output to debugging UART2. Bits1,2 extended debugging. 
- `<G Value>`: Gain selection of the PAmp, legal values: [0:3].

## Testing/Debugging
The RS232 with 3.3V levels should be connected: RXD - to PA9 (W_TX), TXD - to PA10 (W_RX).

To communicate with the board over UART1:

    python3 -m serial.tools.miniterm /dev/ttyUSB0 57600

The UART2(over USB) sends debugging information, which is copy of the output sream of UART1

    python3 -m serial.tools.miniterm /dev/ttyACM0 115200

## Data stream from SIM to PIM.
The baud rate is the 57600. With ADC sampling rate of 800 Hz, SIM can deliver ~600 samples/s. The standard delivery is statistics over requested period, default 1 s. The statistics data are ASCII like this:

    <M498,60,-1340092,993,427,1371015>
    <M498,60,-1339997,1134,576,1372158>

Data format of the \<M...> record:<br>
**<Mn1,n2,n3,n4,n5,n6>**. Where n1 is number of samples, received from ADC 
since last report, n2 is number of samples, accumulated for statistics, 
n3: Mean\*10, n4: StDev\*10, n5: peak-to-peak amplitude, n6 clockCounter (1ms)<br>

If enabled, SIM will also deliver samples. For example: 

    <M7,7,-1339327,389,126,1371015><R-133932><R-133936><R-133954><R-133943><R-133980><R-133956><R-133939>
    <M7,7,-1339485,152,48,1372158><R-134004><R-133935><R-133932><R-133982><R-133959><R-133941><R-133932>

Data format (ASCII) of the \<R...> record:<br>
**\<Rn1>**. Where n1 is ADC reading.<br>
Reading 0 corresponds to 0V difference,  Minimal reading is -8388608 for -VREF/2, maximum reading is +8388608 for +VREF/2.

## Firmware flowchart.

![Firmware flowchart](L432KC_SIM/Docs/SIM_flowchart.png)
