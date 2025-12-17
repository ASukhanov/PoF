# Debugging the SIM board.
## Test of the bare Nucleo-32 STM32L432 module through USB port (UART2).
- Connect connect module to USB.
- Run: ```python -m serial.tools.miniterm /dev/ttyACM0 115200```<br>
You should be getting stream like this and power consumption ~50mA:
```
<M6,6,-83886080,0,0,28614><?ERR: Conversion delayed><?ERR: Conversion delayed><?ERR: Conversion delayed><?ERR: Conversion delayed><?ERR: Conversion delayed><?ERR: Conversion delayed><?ERR: Conversion delayed>
...
```
If reset button of the Nucleo board is pressed, then it injects the following string in the stream:<br>
```<TSR:0,TO:160,RL:800,T:0,V:0.1.4>```
The V:0.1.4 stays for firmware version, defined in the source file board.h.

## Test of the bare Nucleo-32 STM32L432 module through UART1.
- To control from USB-UART3.3V make following connections<br>
TXD(yellow): PA9, RXD(orange): PA10, GND: GND.
- Run: ```python -m serial.tools.miniterm /dev/ttyUSB0 57600```<br>
The output stream at UART1 is copy of the stream at UART2.


## RGB LED with common 3.3V<br>
Connection:
Red: PA7, Green PA12, Blue: PA6, Common: 3.3V.<br>
The green LED2 on board is reflecting the SPI_CLK.


When board starts the LED_GREEN goes on for 1 s, after that LEDS should start counting.<br>
THe flashing 2Hz LED_RED means that ADC is not convertsin.
 
## Suggested PCBModifications
- Pins (0.8mm through holes) for W_Ts, W_Rx, GND
- Pin for Ref+ (0.8mm through hole)
- U10.4 should have a capacitor 10nF to ground.
