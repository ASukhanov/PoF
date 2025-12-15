# Change Log for module L432KC

## [0.0.0] - 2024-07-05
L432KC_blinky2, flashing at 10 Hz
- Power from USB: 52mA, 0.265 mW
- Power from +5V: 5mA, 32mW. Note SB9 need to be open!

## [0.0.2] - 2024-07-05

### added
L432KC_SIM_v1

 uart1 RX & TX are instrumented.
  Power from +5V: 12mA, 60mW. with LEDs, 56mW without LEDs

## [0.0.3] - 2024-07-06
Refactoring: Board specifics moved to board.h and board.c

SIM commands parsing

## [0.0.4] - 2024-07-08
Dummy report_status
