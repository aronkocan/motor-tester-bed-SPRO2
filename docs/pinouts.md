# Pinouts

This document defines the current pin assignments for the three Arduino Nano boards used in the motor tester bed project.

---

## arduino-measurement

### Assigned pins

| Arduino Pin | AVR Pin | Function | Notes |
|---|---|---|---|
| D6 | PD6 | PWM EnB | PWM output |
| A0 | PC0 | Analog voltage input from motor | via RC bridge |
| A4 | PC4 / SDA | I2C | communication with other Nanos |
| A5 | PC5 / SCL | I2C | communication with other Nanos |

### Unassigned
- D0, D1, D2, D3, D4, D5, D7, D8, D9, D10, D11, D12, D13
- A1, A2, A3, A5, A6, A7

---

## arduino-main

### Assigned pins

| Arduino Pin | AVR Pin | Function | Notes |
|---|---|---|---|
| D0 | PD0 / RX | Hardware serial to Nextion RX | USART |
| D1 | PD1 / TX | Hardware serial to Nextion TX | USART |
| D3 | PD3 | START button | input, active HIGH |
| D4 | PD4 | STOP button | input, active HIGH |
| D6 | PD6 | START button LED | input, active HIGH |
| D7 | PD7 | STOP button LED | input, active HIGH |
| D12 | PB4 | Software serial to USB host TX | software serial |
| D13 | PB5 | Software serial to USB host RX | software serial |
| A4 | PC4 / SDA | I2C | communication with other Nanos |
| A5 | PC5 / SCL | I2C | communication with other Nanos |

### Additional notes
- Main controller / head Arduino
- Communicates with:
  - `arduino-measurement` via I2C
  - `arduino-opto` via I2C
  - Nextion display via USART
  - INA226_1: Sensor for Motor Under Test via I2C
  - INA226_2: Sensor for Load Motor / testing motor via I2C

### Unassigned
- D2, D5, D8, D9, D10, D11
- A0, A1, A2, A3, A5, A6, A7

---

## arduino-opto

### Assigned pins

| Arduino Pin | AVR Pin | Function | Notes |
|---|---|---|---|
| D4 | PD4 | Encoder pulse input (optocoupler) | digital input, pulses from encoder wheel for RPM measurement |
| A4 | PC4 / SDA | I2C | communication with other Nanos |
| A5 | PC5 / SCL | I2C | communication with other Nanos |

### Additional notes
- Handles optocoupler-related functionality
- Communicates with main Arduino via I2C

### Signal description
- The optocoupler is paired with an encoder wheel attached to the motor shaft
- It produces digital pulses corresponding to shaft rotation
- These pulses are used to calculate rotational speed (RPM)

### Unassigned
- D0, D1, D2, D3, D5, D6, D7, D8, D9, D10, D11, D12, D13
- A0, A1, A2, A3, A5, A6, A7

---

## Shared communication pins

### I2C
All three Nano boards use I2C communication.
The two INA sensors also communicate over I2C and share the same SDA/SCL bus as the connected Arduino.

| Arduino Pin | AVR Pin | Function |
|---|---|---|
| A4 | PC4 / SDA | I2C SDA |
| A5 | PC5 / SCL | I2C SCL |