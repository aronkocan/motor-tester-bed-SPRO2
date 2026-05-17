# Pinouts

Current pin assignments used by the code for the three Arduino Nano (`nanoatmega328new`) boards.

## `arduino-main`

| Arduino pin | AVR pin | Direction | Function | Notes |
|---|---|---|---|---|
| D0 | PD0 / RX | input | Nextion serial RX | Arduino receives from Nextion TX at 9600 baud |
| D1 | PD1 / TX | output | Nextion serial TX | Arduino sends to Nextion RX at 9600 baud |
| D2 | PD2 / INT0 | input | START button | `INPUT`, active HIGH, rising-edge interrupt |
| D3 | PD3 / INT1 | input | STOP button | `INPUT`, active HIGH, rising-edge interrupt |
| D6 | PD6 | output | START LED | active HIGH |
| D7 | PD7 | output | STOP LED | active HIGH |
| D12 | PB4 | input | USB host serial RX | `SoftwareSerial` RX from CH376/USB host TX at 9600 baud |
| D13 | PB5 | output | USB host serial TX | `SoftwareSerial` TX to CH376/USB host RX at 9600 baud |
| A4 | PC4 / SDA | bidirectional | I2C SDA | master bus to worker boards and INA226 sensors |
| A5 | PC5 / SCL | output | I2C SCL | master bus to worker boards and INA226 sensors |

Unassigned in code: D4, D5, D8, D9, D10, D11, A0, A1, A2, A3, A6, A7.

## `arduino-measurement`

| Arduino pin | AVR pin | Direction | Function | Notes |
|---|---|---|---|---|
| D6 | PD6 | output | Motor PWM | `analogWrite()` PWM output |
| A0 | PC0 | input | Test-motor voltage ADC | 10 ADC samples, 5 V reference, multiplied by 6 for voltage divider |
| A4 | PC4 / SDA | bidirectional | I2C SDA | slave address `0x08` |
| A5 | PC5 / SCL | input | I2C SCL | slave address `0x08` |

Unassigned in code: D0, D1, D2, D3, D4, D5, D7, D8, D9, D10, D11, D12, D13, A1, A2, A3, A6, A7.

## `arduino-opto`

| Arduino pin | AVR pin | Direction | Function | Notes |
|---|---|---|---|---|
| D4 | PD4 | input | Encoder pulse input | `INPUT_PULLUP`; counts LOW-to-HIGH edges for RPM |
| A4 | PC4 / SDA | bidirectional | I2C SDA | slave address `0x09` |
| A5 | PC5 / SCL | input | I2C SCL | slave address `0x09` |

Unassigned in code: D0, D1, D2, D3, D5, D6, D7, D8, D9, D10, D11, D12, D13, A0, A1, A2, A3, A6, A7.

## Shared I2C bus

| Arduino pin | AVR pin | Function |
|---|---|---|
| A4 | PC4 / SDA | I2C data |
| A5 | PC5 / SCL | I2C clock |

I2C devices used by `arduino-main`: `arduino-measurement` (`0x08`), `arduino-opto` (`0x09`), Motor Under Test INA226 (`0x40`), and Load Motor INA226 (`0x45`).
