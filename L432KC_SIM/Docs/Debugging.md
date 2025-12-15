# Debugging the SIM board.

To control from USB-UART3.3V make following connections<br>
RXD(yellow): PA9, TXD(orange): PA10, GND: GND.

## RGB LED with common 3.3V<br>
Connection:
Red: PA7, Green PA12, Blue: PA6, Common: 3.3V.<br>
The green LED2 on board is replecting SPI_CLK.


When board starts the LED_GREEN goes on for 1 s, after that LEDS should start counting.<br>
THe flashing 2Hz LED_RED means that ADC is not convertsin.
 
## Suggested PCBModifications
- Pins (0.8mm through holes) for W_Ts, W_Rx, GND
- Pin for Ref+ (0.8mm through hole)
- U10.4 should have a capacitor 10nF to ground.
