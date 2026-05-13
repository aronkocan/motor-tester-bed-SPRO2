# AGENTS.md

## Project Purpose and Current Constraints
This project is an embedded motor testing system for measuring and evaluating small electric motors (such as DC motors). The goal is to determine unknown motor parameters by running controlled measurements and calculating values within acceptable tolerance ranges.

---

## System Architecture

The system consists of three Arduino Nano controllers, one Nextion display, and two INA226 current/voltage sensors.

Hardware used:
- `arduino-main`: Arduino Nano (`nanoatmega328new`)
- `arduino-measurement`: Arduino Nano (`nanoatmega328new`)
- `arduino-opto`: Arduino Nano (`nanoatmega328new`)
- Nextion display: `NX8048P050`
- INA sensors: 2× `INA226`
  - one INA226 for the Motor Under Test
  - one INA226 for the Load Motor / testing motor

### `arduino-main` — central coordinator

Main responsibilities:
- high-level control logic and system state handling
- phase transitions
- coordination of the other boards
- communication with the Nextion display
- handling unified start/stop commands
- requesting setup/input values from the Nextion display when needed
- collecting measurement data from the other boards
- storing/calculating final datapoints

### `arduino-measurement`

Main responsibilities:
- motor-driving/PWM logic for the Motor Under Test
- communication with the two INA226 sensors
- reading voltage/current data from the INA226 sensors
- measurement-related preprocessing/averaging when needed
- sending measurement data/status to `arduino-main`

The two INA226 sensors are used to measure electrical values for:
- the Motor Under Test
- the Load Motor / testing motor

### `arduino-opto`

Main responsibilities:
- optocoupler-related input/output handling
- encoder pulse handling
- RPM calculation / RPM-related signal logic
- sending optocoupler-derived status/data to `arduino-main`

### Nextion display

The Nextion display (`NX8048P050`) is used as the user interface for setup values, status display, progress information, and results display.

The display may handle local page navigation internally. The main Arduino should request user-entered values from the display when needed, rather than relying on the display to control the test sequence directly.

### Boundary rules

- Keep runtime responsibilities clearly separated by board.
- Do not move responsibilities between boards without explicit reason.
- Shared protocol/message definitions are allowed when needed for cross-board consistency.

---

## Communication Contract

- `arduino-main` ↔ `arduino-measurement`: I2C
- `arduino-main` ↔ `arduino-opto`: I2C
- `arduino-main` ↔ Nextion display (`NX8048P050`): USART
- `arduino-measurement` ↔ INA226 sensor for Motor Under Test: I2C
- `arduino-measurement` ↔ INA226 sensor for Load Motor / testing motor: I2C

Notes:
- All I2C devices on the same bus must use unique addresses.
- `arduino-main` owns high-level coordination and requests data from the other boards.
- The Nextion display is used for user input/status display; it should not own the test sequence.
- The INA226 sensors provide voltage/current measurement data to `arduino-measurement`.

---

## Runtime Model and UI Behavior

### Main runtime states

The high-level runtime flow is owned by `arduino-main`.

1. `WAIT_FOR_SETUP`
   - system is idle
   - user enters/selects required setup values on the Nextion display
   - Nextion may handle local page navigation internally
   - `arduino-main` waits for the physical Start button

2. `PREPARE_TEST`
   - physical Start button has been pressed
   - `arduino-main` requests the required setup values from the Nextion display
   - setup values are validated
   - initial calculations are performed
   - old measurement data/counters are reset
   - first measurement step is prepared

3. `RUN_MEASUREMENT_CYCLE`
   - one complete datapoint is measured
   - `arduino-main` sends the PWM value to `arduino-measurement`
   - system waits for the motor to stabilize
   - `arduino-measurement` and `arduino-opto` complete their measurements
   - `arduino-main` requests voltage/current data from `arduino-measurement`
   - `arduino-main` requests RPM data from `arduino-opto`
   - `arduino-main` processes the received data
   - calculated values are produced, such as effective voltage, electrical power, and estimated torque
   - the completed datapoint is stored

4. `EVALUATE_NEXT_STEP`
   - `arduino-main` decides whether another datapoint is needed
   - if more datapoints are needed, the next PWM/test parameters are selected and the system returns to `RUN_MEASUREMENT_CYCLE`
   - if the measurement sequence is complete, the system transitions to `OUTPUT_RESULTS`

5. `OUTPUT_RESULTS`
   - collected data is displayed and/or exported
   - system waits until the user indicates they are done with the results
   - after completion/reset, the system returns to `WAIT_FOR_SETUP`

### Start/Stop command sources

Start/stop commands come from hardware button inputs on `arduino-main`.

The Nextion display is used for:
- setup input
- local page navigation
- status display
- progress display
- result display

The Nextion display should not own the measurement sequence or decide runtime phase transitions.

### Runtime control rules

- `arduino-main` owns the state machine and all high-level phase transitions.
- `arduino-measurement` acts as a worker board for PWM control and INA226 voltage/current measurement.
- `arduino-opto` acts as a worker board for optocoupler/encoder pulse handling and RPM measurement.
- Stop handling should remain available during active measurement phases.

---

## Engineering Principles

This is a small second-semester embedded project. The main priority is a simple, understandable, and easy-to-work-with implementation.

When implementing code:
- keep the logic simple and readable
- prefer clear, direct solutions over highly abstract designs
- make the code easy to debug and explain
- keep board responsibilities clear
- modify only files relevant to the requested task

Language/style:
- use C++ in the Arduino environment
- avoid advanced C++ patterns unless they clearly make the code simpler

---

## Build
Build system:
- Each Arduino folder is an independent PlatformIO project with its own `platformio.ini`

---

## Pin Assignments
Pin assignments are defined in `docs/pinouts.md`.

Pin rules:
- follow documented pin mappings
- do not invent new mappings unless explicitly instructed
- keep board-specific pin usage separated by Arduino