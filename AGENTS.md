# AGENTS.md

## Project Name
motor-tester-bed-SPRO2

## Project Purpose
This project is an embedded motor testing system for measuring and evaluating small electric motors, such as DC motors. The goal is to determine unknown motor parameters by running controlled measurements and calculating relevant values within an acceptable tolerance range.

The system should be developed in a way that is modular, understandable, and easy to refine later when real hardware testing becomes possible.

---

## High-Level System Overview
The system consists of three Arduino-based controllers and one Nextion display:

- `arduino-main`
  - central controller of the system
  - coordinates the other Arduinos
  - communicates with the Nextion display
  - manages system flow and high-level logic

- `arduino-measurement`
  - responsible for motor-related measurements
  - expected to measure values such as current, voltage, and torque
  - may also generate or control PWM input to the motor
  - communicates measurement data to the main Arduino

- `arduino-opto`
  - responsible for the optocoupler-related functionality
  - communicates with the main Arduino

- `Nextion display`
  - user interface for starting, stopping, and monitoring tests
  - communicates with `arduino-main` via USART

There may also be additional hardware for sensing, signal conditioning, and measurement support.

---

## Communication Rules
- The three Arduinos communicate via **I2C**
- The main Arduino communicates with the Nextion display via **USART**
- Treat `arduino-main` as the central coordinator
- Communication between boards should be explicit, simple, and well-structured
- If communication-related code is added, keep message meaning clear and avoid unnecessary complexity
- If shared protocol/message definitions are introduced, keep them consistent across all relevant boards

---

## System Operation Model
The system should be structured around three main phases:

### 1. Setup Phase
Before a measurement starts:
- the user may input required motor information
- the user chooses what the system should do
- the system prepares the test conditions

### 2. Measurement Phase
- measurement starts when the user presses the start button on the Nextion display
- the system performs the measurement process
- progress should be shown on the display
- the user must be able to stop the measurement immediately at any time from the display

### 3. Result Phase
- once measurements are complete, the system automatically transitions to displaying results
- measured/calculated data should be shown to the user
- the user should eventually be able to extract data via USB or micro USB

---

## User Interface Expectations
The display/UI should support:
- starting a test
- stopping a running test
- entering or selecting required setup information before the test
- showing progress during the measurement
- showing results after the measurement

UI expectations:
- keep the interface responsive
- screen updates during measurement should be periodic
- do not block critical system behavior with unnecessary UI logic
- prioritize clarity and reliability over flashy behavior

---

## Error Handling Expectations
The system should notify the user when:
- required user input is missing
- something is wrong with the motor
- something goes wrong during measurement
- abnormal detection or measurement behavior occurs

Error handling rules:
- if a serious problem occurs during measurement, stop the measurement safely
- notify the user clearly
- do not ignore faults silently
- keep fault handling simple, explicit, and easy to extend later

Additional error handling will be refined during development as hardware behavior becomes clearer.

---

## Development Status / Important Context
This project is currently in an early and partly theoretical phase.

Important context:
- not all hardware is available yet
- not all implementation details are fully known yet
- some details will only become clear during later hardware testing
- current work should focus on creating a solid structure, reasonable placeholders, and clear interfaces rather than pretending everything is already fully known

When generating code, prefer:
- skeletons
- modular structure
- placeholders
- clearly marked assumptions
- code that is easy to test and revise later

Do not invent overly specific behavior unless it is clearly required.

---

## Testing Strategy
Testing will happen in stages:

1. test `arduino-opto` separately
2. test `arduino-measurement` separately
3. once subsystem behavior is acceptable, integrate the full system
4. iterate through:
   - test
   - observe
   - describe issues
   - adjust implementation
   - test again

When modifying code:
- prefer changes that support iterative debugging and refinement
- make behavior easy to observe
- make assumptions explicit
- do not over-optimize prematurely

---

## Coding Guidelines
When working on this repository:

- keep code simple, readable, and modular
- prefer straightforward embedded-friendly logic
- avoid unnecessary abstraction
- avoid unnecessary dynamic memory usage
- do not merge unrelated responsibilities into one file or one board
- keep each Arduino's role separate and clear
- do not introduce large architectural changes without strong reason
- prefer maintainability and clarity over cleverness
- write code in a way that can be debugged easily later on real hardware

---

## Board Responsibility Rules
### `arduino-main`
Should mainly contain:
- high-level control logic
- system state handling
- coordination of the other boards
- communication with the Nextion display
- handling start/stop commands and phase transitions

### `arduino-measurement`
Should mainly contain:
- sensor reading logic
- measurement-related processing
- motor-driving-related logic if applicable
- sending measurement data/status to the main Arduino

### `arduino-opto`
Should mainly contain:
- optocoupler-related input/output handling
- optocoupler-specific logic
- communication with the main Arduino

Do not move responsibilities between boards unless there is a strong and explicit reason.

---

## Architecture Guidance
- favor simple, understandable, and maintainable designs
- keep board responsibilities clear and separated
- use communication patterns that are explicit and easy to debug
- prefer solutions that are realistic for Arduino-based embedded development
- avoid unnecessary complexity and premature optimization

These are preferences, not strict rules. If a different approach is clearly better, use it and keep the reasoning explicit. When multiple approaches are possible, prefer the one that is easiest to understand, test, and modify later.

---

## File / Change Behavior for Codex
When making changes:
- only modify files that are relevant to the requested task
- avoid unrelated edits
- preserve existing structure unless improvement is clearly necessary
- if adding new modules, use clear names based on purpose
- if behavior is uncertain, leave TODO comments instead of fabricating false certainty
- if protocol structures or assumptions are created, keep them documented and easy to change later

---

## What to Prioritize Right Now
At the current stage, prioritize:
- project structure
- module boundaries
- communication structure
- state flow
- placeholders for measurement/control/UI logic
- code that can later be tested and refined on real hardware

Do not prioritize:
- premature optimization
- over-engineered abstractions
- pretending hardware behavior is already confirmed

---

## Expected Style of Help
When implementing or modifying code in this project:
- make reasonable assumptions, but keep them conservative
- clearly reflect the known system architecture
- support iterative refinement
- generate code that is realistic for Arduino-based embedded development
- prefer robust and understandable solutions over complex ones

---

## Build System
- Each Arduino folder is a separate PlatformIO project
- Each contains its own `platformio.ini`
- Code should be compatible with PlatformIO builds

---

## Verification Guidance
When making code changes, prefer validation that is possible without hardware.

Current verification priorities:
- Build `arduino-main`
- Build `arduino-measurement`
- Build `arduino-opto`

If logic is added or changed, also prefer:
- simple logic tests where possible
- mock/simulated inputs for protocol or state-flow behavior
- explicit TODOs where real hardware validation is still required

Do not claim hardware behavior is verified unless it has been tested on real hardware.