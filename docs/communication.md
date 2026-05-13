# Communication Contract

This document defines the communication contract between the boards and I2C devices in the Motor Tester Bed project.

---

### Main communication roles

| Device | Role |
|---|---|
| `arduino-main` | Main coordinator / I2C master |
| `arduino-measurement` | Worker board / I2C slave |
| `arduino-opto` | Worker board / I2C slave |
| INA226 - Motor Under Test | I2C sensor / slave |
| INA226 - Load Motor | I2C sensor / slave |
| Nextion display | USART HMI device |

`arduino-main` owns the high-level runtime logic and starts all I2C communication.

---

## I2C Bus

### I2C Pins

On Arduino Nano / ATmega328P boards:

| Arduino Pin | AVR Pin | Function |
|---|---|---|
| A4 | PC4 / SDA | I2C data |
| A5 | PC5 / SCL | I2C clock |

---

## I2C Addresses

| Device | Address | Notes |
|---|---:|---|
| `arduino-measurement` | `0x08` | Receives PWM/motor control commands |
| `arduino-opto` | `0x09` | Provides RPM data |
| INA226 - Motor Under Test | `0x40` | Measures voltage/current for Motor Under Test |
| INA226 - Load Motor | `0x41` | Measures voltage/current for Load Motor |

---

## I2C Command IDs

### Commands for `arduino-measurement`

| Command | ID | Direction | Purpose |
|---|---:|---|---|
| `CMD_SET_PWM` | `0x01` | main → measurement | Set PWM duty cycle |
| `CMD_STOP_MOTOR` | `0x02` | main → measurement | Stop motor output |

### Commands for `arduino-opto`

| Command | ID | Direction | Purpose |
|---|---:|---|---|
| `CMD_TAKE_RPM_MEASUREMENT` | `0x10` | main → opto | Start a new RPM measurement window |
| `CMD_GET_RPM` | `0x11` | main → opto | Request the latest completed RPM result |

---

## Message Layouts

### `CMD_SET_PWM`

Direction: `arduino-main` → `arduino-measurement`

Purpose: set the PWM duty cycle used for the motor driver.

Size: 2 bytes

| Byte | Field | Type | Description |
|---:|---|---|---|
| 0 | command ID | `uint8_t` | `CMD_SET_PWM` |
| 1 | duty cycle | `uint8_t` | PWM value from 0 to 255 |

Example:

| Byte | Value |
|---:|---:|
| 0 | `0x01` |
| 1 | `128` |

Meaning:

```text
Set PWM output to 128, approximately 50% duty cycle.
```