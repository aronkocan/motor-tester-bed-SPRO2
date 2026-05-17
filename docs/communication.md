# Communication Contract

Current communication interfaces and message formats used by the code.

## Interfaces

| Link | Interface | Role / baud | Notes |
|---|---|---|---|
| `arduino-main` ↔ `arduino-measurement` | I2C | main is master, measurement is slave | PWM commands and test-motor voltage ADC result |
| `arduino-main` ↔ `arduino-opto` | I2C | main is master, opto is slave | RPM measurement commands and result |
| `arduino-main` ↔ Motor Under Test INA226 | I2C | main is master, INA226 is slave | current read by code; test-motor voltage comes from measurement board A0 |
| `arduino-main` ↔ Load Motor INA226 | I2C | main is master, INA226 is slave | load-motor current and bus voltage read by code |
| `arduino-main` ↔ Nextion | hardware serial | 9600 baud | setup value requests and result-screen touch events |
| `arduino-main` ↔ CH376 USB host | software serial | 9600 baud | exports `RESULT.CSV` |

I2C pins on each Nano: A4/PC4 = SDA, A5/PC5 = SCL.

## I2C addresses

| Device | Address |
|---|---:|
| `arduino-measurement` | `0x08` |
| `arduino-opto` | `0x09` |
| Motor Under Test INA226 | `0x40` |
| Load Motor INA226 | `0x45` |

## `arduino-measurement` I2C commands

| Command | ID | Request bytes | Response bytes | Purpose |
|---|---:|---|---|---|
| `CMD_SET_PWM` | `0x01` | `[cmd, duty:uint8]` | none | Set D6 PWM, 0-255 |
| `CMD_STOP_MOTOR` | `0x02` | `[cmd]` | none | Set D6 PWM to 0 |
| `CMD_TAKE_TEST_MOTOR_VOLTAGE_MEASUREMENT` | `0x03` | `[cmd]` | none | Schedule A0 voltage sampling |
| `CMD_GET_TEST_MOTOR_VOLTAGE` | `0x04` | `[cmd]` then read | 2 bytes | Latest real test-motor voltage in millivolts, little-endian `uint16_t` |
| `CMD_IS_TEST_MOTOR_VOLTAGE_MEASUREMENT_RUNNING` | `0x05` | `[cmd]` then read | 1 byte | `1` = running or queued, `0` = complete |

The measurement board samples A0 10 times using a 5 V ADC reference, averages the result, multiplies by 6 for the voltage divider, clamps to `uint16_t`, and returns millivolts.

## `arduino-opto` I2C commands

| Command | ID | Request bytes | Response bytes | Purpose |
|---|---:|---|---|---|
| `CMD_TAKE_RPM_MEASUREMENT` | `0x10` | `[cmd]` | none | Start a 1000 ms RPM window |
| `CMD_GET_RPM` | `0x11` | `[cmd]` then read | 2 bytes | Latest whole-number RPM, little-endian `uint16_t` |
| `CMD_IS_MEASUREMENT_RUNNING` | `0x12` | `[cmd]` then read | 1 byte | `1` = running or queued, `0` = complete |

The opto board uses D4 with `INPUT_PULLUP`, counts LOW-to-HIGH pulse edges, assumes 40 pulses per rotation, and rounds the calculated RPM.

## Nextion serial protocol

`arduino-main` requests setup values by sending `get <component>.val` followed by three `0xFF` bytes. A valid numeric response starts with `0x71`, contains a 4-byte little-endian value, and ends with three `0xFF` bytes. The code retries the same request until a complete valid response is received.

| Setup value | Requested component | Code meaning |
|---|---|---|
| Measurement mode | `setup.mode.val` | `0` automatic, `1` manual |
| Automatic minimum | `range.n0.val` | duty minimum, clamped to 0-255 |
| Automatic maximum | `range.n1.val` | duty maximum, clamped to 0-255 |
| Automatic step size | `range.n2.val` | duty step, clamped to 0-255, `0` becomes `1` |
| Manual target selector | `target.tarsel.val` | `0` power, `1` torque, `2` RPM, `3` effective voltage, `4` duty cycle |
| Power target | `target.power.val` | milliwatts |
| Torque target | `ttorque.x0.val` | scaled by 100, converted to mNm value |
| RPM target | `trpm.n0.val` | RPM |
| Effective voltage target | `teff.x0.val` | scaled by 100, converted to volts before internal millivolt comparison |
| Duty cycle target | `tduty.n0.val` | duty value |

In `OUTPUT_RESULTS`, Nextion touch events are read as `0x65, page, component, event, 0xFF, 0xFF, 0xFF`. Only press events (`event = 0x01`) are used.

| Component ID | Touch byte | Meaning |
|---:|---:|---|
| `2` | `0x02` | export results to USB |
| `3` | `0x03` | quit results screen and return to setup state |

## USB export

When export is triggered and the CH376 drive is ready, `arduino-main` writes `RESULT.CSV` with this header:

```text
duty,effective_mV,torque_mNm,power_mW,rpm
```
