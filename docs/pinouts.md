# Pinouts

This document defines the current pin assignments for the three Arduino Nano boards used in the motor tester bed project.

These pin assignments are based on the current provided table and may be refined later if hardware testing shows changes are needed.

---

## arduino-measurement

### Assigned pins

| Arduino Pin | AVR Pin | Function | Notes |
|---|---|---|---|
| D6 | PD6 | PWM EnB | PWM output |
| A0 | PC0 | Analog voltage input from motor | via RC bridge |
| A4 | PC4 / SDA | I2C | communication with other Nanos |
| A5 | PC5 / SCL | I2C | communication with other Nanos |

### Additional notes
- INA226_1: test motor current (**address: 0x40**)
- INA226_2: measure motor voltage + current (**address: 0x45**)
- Communicates with main Arduino via I2C

### Unassigned / not yet defined
- D0, D1, D2, D3, D4, D5, D7, D8, D9, D10, D11, D12, D13
- A1, A2, A3, A5, A6, A7

---

## arduino-main

### Assigned pins

| Arduino Pin | AVR Pin | Function | Notes |
|---|---|---|---|
| D0 | PD0 / RX | Hardware serial to Nextion RX | USART |
| D1 | PD1 / TX | Hardware serial to Nextion TX | USART |
| D2 | PD2 | POWER button | input, active LOW |
| D3 | PD3 | START button | input, active LOW |
| D4 | PD4 | STOP button | input, active LOW |
| D5 | PD5 | QUICKEY 1 button | input, active LOW |
| D6 | PD6 | QUICKEY 2 button | input, active LOW |
| D7 | PD7 | QUICKEY 3 button | input, active LOW |
| D8 | PB0 | ENC button | encoder button, active LOW |
| D9 | PB1 | ENC B | encoder signal |
| D10 | PB2 | ENC A | encoder signal |
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

### Unassigned / not yet defined
- D11
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

### Unassigned / not yet defined
- D0, D1, D2, D3, D5, D6, D7, D8, D9, D10, D11, D12, D13
- A0, A1, A2, A3, A5, A6, A7

---

## Shared communication pins

### I2C
All three Nano boards use I2C communication.

| Arduino Pin | AVR Pin | Function |
|---|---|---|
| A4 | PC4 / SDA | I2C SDA |
| A5 | PC5 / SCL | I2C SCL |

### Notes
- The provided table explicitly marks I2C usage on A4 for all three boards
- Standard Nano I2C also uses A5 as SCL

---

## Important notes

- This file reflects the currently available pinout information
- Some assignments are still incomplete or not fully confirmed
- Unassigned pins should not be assumed to be free for use without checking updated hardware plans
- If pin mappings change, update this document before changing code