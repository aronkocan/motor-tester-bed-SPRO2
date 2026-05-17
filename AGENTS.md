# AGENTS.md

## Project Purpose

This repository contains a small embedded motor tester for measuring and evaluating small electric motors with three Arduino Nano boards. Keep changes simple, Arduino-style, and easy to explain for a second-semester embedded project.

The current implementation can:
- run automatic duty-cycle sweeps,
- run manual/target-based measurements,
- collect compact integer datapoints,
- show/control setup and result actions through a Nextion display,
- export collected results to USB as CSV through a CH376 module.

---

## Repository Layout

Each Arduino folder is an independent PlatformIO project:

- `arduino-main/` — central coordinator and state machine.
- `arduino-measurement/` — motor PWM worker and test-motor voltage ADC worker.
- `arduino-opto/` — encoder/opto RPM worker.
- `docs/communication.md` — current board-to-board, Nextion, I2C, and USB protocol details.
- `docs/pinouts.md` — current pin assignments used by the code.

Build commands:

```sh
pio run -d arduino-main
pio run -d arduino-measurement
pio run -d arduino-opto
```

All three PlatformIO projects target `atmelavr`, board `nanoatmega328new`, framework `arduino`. `arduino-main` additionally depends on `robtillaart/INA226 @ ^0.6.6` and `Ch376msc` from `https://github.com/djuseeq/Ch376msc.git#1.4.5`.

---

## Hardware and Responsibilities

Hardware represented by the current code:

- `arduino-main`: Arduino Nano (`nanoatmega328new`), central controller.
- `arduino-measurement`: Arduino Nano (`nanoatmega328new`), I2C slave at `0x08`.
- `arduino-opto`: Arduino Nano (`nanoatmega328new`), I2C slave at `0x09`.
- Nextion display (`NX8048P050`) connected to `arduino-main` hardware serial at 9600 baud.
- Two INA226 sensors on the main I2C bus:
  - Motor Under Test INA226 at `0x40`.
  - Load Motor INA226 at `0x45`.
- CH376 USB host module connected to `arduino-main` by `SoftwareSerial` at 9600 baud.

### `arduino-main`

Owns high-level coordination and must remain the only board that controls test phases. Its current duties are:

- runtime state machine and phase transitions,
- Start/Stop button interrupt handling,
- Start/Stop LED handling,
- requesting setup values from Nextion,
- sending PWM and voltage-measurement commands to `arduino-measurement`,
- requesting RPM measurements from `arduino-opto`,
- reading INA226 current/voltage values where implemented,
- calculating effective voltage, electrical power, and estimated torque,
- storing up to 51 result datapoints,
- handling result-screen touch events,
- exporting `RESULT.CSV` to USB.

Important current measurement detail: `arduino-main` reads Motor Under Test current from the INA226 at `0x40`, but the Motor Under Test voltage used for datapoints currently comes from `arduino-measurement` A0 over I2C. `arduino-main` also reads load-motor current and bus voltage from the INA226 at `0x45`, although those load values are not currently stored in the result datapoint.

### `arduino-measurement`

Acts as an I2C worker board. Its current duties are:

- receive PWM commands from `arduino-main`,
- output motor PWM on D6 with `analogWrite()`,
- stop PWM on command,
- sample A0 for test-motor voltage when requested,
- return latest test-motor voltage as millivolts.

The A0 voltage measurement averages 10 ADC samples using a 5 V reference, multiplies the ADC pin voltage by 6 for the voltage divider, clamps to `uint16_t`, and returns a little-endian millivolt value.

### `arduino-opto`

Acts as an I2C worker board. Its current duties are:

- read encoder/opto pulses on D4 with `INPUT_PULLUP`,
- count LOW-to-HIGH edges during a 1000 ms measurement window,
- calculate whole-number RPM assuming 40 pulses per rotation,
- return the latest RPM as a little-endian `uint16_t`.

### Nextion display

The Nextion display is used for setup input, local UI navigation, status/progress display, result display, export trigger, and return-to-setup trigger. It should not own the measurement sequence or decide runtime phase transitions. `arduino-main` requests values from named Nextion components when entering `PREPARE_TEST`.

### CH376 USB export

In `OUTPUT_RESULTS`, a Nextion press on component ID `1` triggers USB export. The code writes `RESULT.CSV` with this header:

```text
duty,effective_mV,torque_mNm,power_mW,rpm
```

A press on component ID `2` returns the main state machine to setup.

---

## Communication Contract

Current communication paths:

- `arduino-main` ↔ `arduino-measurement`: I2C, main is master, worker address `0x08`.
- `arduino-main` ↔ `arduino-opto`: I2C, main is master, worker address `0x09`.
- `arduino-main` ↔ Motor Under Test INA226: I2C address `0x40`.
- `arduino-main` ↔ Load Motor INA226: I2C address `0x45`.
- `arduino-main` ↔ Nextion display: hardware serial, 9600 baud.
- `arduino-main` ↔ CH376 USB host: software serial on D12/D13, 9600 baud.

Keep all I2C addresses unique. If protocol IDs, pin mappings, or serial component names change, update both code and the matching documentation in `docs/communication.md` or `docs/pinouts.md`.

---

## Runtime Model

The high-level runtime flow is owned by `arduino-main` and currently uses these states:

1. `WAIT_FOR_SETUP`
   - System is idle.
   - Nextion may handle local setup-page navigation.
   - `arduino-main` waits for the physical Start button interrupt.

2. `PREPARE_TEST`
   - Reset old measurement data.
   - Request setup values from the Nextion display.
   - Configure automatic or manual measurement variables.
   - Enter the first measurement cycle.

3. `RUN_MEASUREMENT_CYCLE`
   - Send current duty cycle to `arduino-measurement`.
   - Wait 1000 ms for stabilization while polling Stop.
   - Read electrical measurements.
   - Start and wait for opto RPM measurement while polling Stop.
   - Calculate effective voltage, power, and torque.
   - Store the completed datapoint.

4. `EVALUATE_NEXT_STEP`
   - Automatic mode selects the next duty cycle or finishes.
   - Manual mode updates the best target match and continues or finishes.

5. `OUTPUT_RESULTS`
   - Wait for Nextion result-screen touch events.
   - Component `1` exports CSV to USB.
   - Component `2` returns to `WAIT_FOR_SETUP`.

Stop handling should remain responsive during active measurement phases. A Stop button press stops the measurement motor, briefly turns on the Stop LED, and returns to `WAIT_FOR_SETUP`.

---

## Measurement Data Model

Completed measurements are stored as compact datapoints to fit Arduino Nano memory limits:

```cpp
struct MeasurementDataPoint {
    uint8_t dutyCycleStep;
    uint16_t effectiveVoltageMilliVolt;
    uint16_t torqueMilliNewtonMeter;
    uint16_t powerMilliWatt;
    uint16_t rpm;
};
```

Use fixed-unit integer fields for stored results where practical. Do not replace this with larger or more abstract structures unless there is a clear reason.

Current calculations:

- Effective voltage is based on the measured test-motor voltage in millivolts.
- Power is based on Motor Under Test voltage and current, converted to milliwatts.
- Torque is estimated from electrical power, a fixed efficiency factor (`0.75 * 0.9`), and measured RPM.
- Values are clamped to fit `uint16_t` where needed.

---

## Measurement Modes

Both modes reuse the same basic cycle: set duty cycle, wait for stabilization, measure voltage/current, measure RPM, calculate values, and store a datapoint.

### Automatic Mode

Automatic setup values come from Nextion components:

- `range.n0.val` — minimum duty cycle.
- `range.n1.val` — maximum duty cycle.
- `range.n2.val` — duty step size; `0` is converted to `1`.

The code swaps min/max if needed, calculates the required datapoint count, and limits storage to `MAX_DATA_POINTS` (`51`).

### Manual / Target-Based Mode

Manual setup uses `target.tarsel.val`:

- `0` power target from `target.power.val` in milliwatts,
- `1` torque target from `ttorque.x0.val`, scaled by 100,
- `2` RPM target from `trpm.n0.val`,
- `3` effective-voltage target from `teff.x0.val`, scaled by 100 and compared internally as millivolts,
- `4` duty-cycle target from `tduty.n0.val`.

Manual search currently checks the minimum duty (`1`), maximum duty (`255`), then uses a binary-search-style range narrowing until the target is within tolerance or the search range is exhausted. If the target is outside the reachable range, the best reached datapoint is kept.

Manual target tolerance is 5% with minimum margins by target type: duty `1`, RPM `10`, effective voltage `100 mV`, torque `2 mNm`, and power `100 mW`.

---

## Pin Assignments

Pin assignments are documented in `docs/pinouts.md` and should be treated as the source of truth when changing hardware-facing code. Do not invent new mappings without updating that document.

Important current pins:

- `arduino-main`: D2 Start button, D3 Stop button, D6 Start LED, D7 Stop LED, D12/D13 CH376 software serial, A4/A5 I2C, D0/D1 Nextion serial.
- `arduino-measurement`: D6 motor PWM, A0 test-motor voltage ADC, A4/A5 I2C.
- `arduino-opto`: D4 encoder pulse input, A4/A5 I2C.

---

## Implementation Guidelines

- Keep board responsibilities clearly separated.
- Do not move responsibilities between boards without explicit reason.
- Prefer direct, readable Arduino C++ over advanced abstractions.
- Keep I2C callbacks short; set flags in callbacks and perform longer work in `loop()`.
- Use compact integer units for stored datapoints where possible.
- Modify only files relevant to the requested task.
- If code changes affect protocols or pins, update the corresponding docs.
- Never add try/catch blocks around imports/includes.
