# AGENTS.md

## Project Purpose and Current Constraints
This project is an embedded motor testing system for measuring and evaluating small electric motors (such as DC motors). The goal is to determine unknown motor parameters by running controlled measurements and calculating values within acceptable tolerance ranges.

---

## System Boundaries and Responsibilities
The system consists of three Arduino-based controllers and one Nextion display.

### `arduino-main` (central coordinator)
Main responsibilities:
- high-level control logic and system state handling
- phase transitions
- coordination of other boards
- communication with Nextion display
- handling unified start/stop commands

### `arduino-measurement`
Main responsibilities:
- sensor reading logic
- measurement-related processing
- motor-driving/PWM logic when applicable
- sending measurement data/status to `arduino-main`

### `arduino-opto`
Main responsibilities:
- optocoupler-related input/output handling
- encoder pulse handling / RPM-related signal logic
- sending optocoupler-derived status/data to `arduino-main`

Boundary rules:
- Keep runtime responsibilities clearly separated by board
- Do not move responsibilities between boards without explicit reason
- Shared protocol/message definitions are allowed when needed for cross-board consistency

---

## Communication Contract
- `arduino-main` ↔ `arduino-measurement`: I2C
- `arduino-main` ↔ `arduino-opto`: I2C
- `arduino-main` ↔ Nextion display: USART

---

## Runtime Model and UI Behavior
### Runtime phases
1. Setup phase
   - user enters/selects required setup inputs
   - system prepares test conditions
2. Measurement phase
   - measurement runs and reports periodic progress
   - stop must be possible immediately
3. Result phase
   - system transitions automatically to showing results when measurement completes
   - measured/calculated output is shown

### Start/Stop command sources
Start/stop commands may come from:
- hardware button inputs on `arduino-main`


### UI expectations
- Keep UI responsive
- Do not block critical system behavior with unnecessary UI logic
- Prioritize clarity and reliability over flashy behavior

---

## Error Handling
The system should notify the user when:
- required setup input is missing
- motor condition appears invalid/abnormal
- measurement fails or abnormal behavior is detected

Fault-handling rules:
- On serious measurement faults, stop measurement safely
- Surface a clear fault indication to the user
- Do not ignore faults silently
- Keep fault handling explicit and easy to extend

---

## Engineering Principles (Single Source)
When implementing code:
- keep logic simple, readable, and embedded-friendly
- prefer maintainability and debuggability over cleverness
- avoid premature optimization and over-engineering
- preserve existing structure unless a clear improvement is needed
- modify only files relevant to the requested task

Language/style:
- C++ in Arduino environment
- Prefer straightforward implementations over advanced C++ patterns

---

## Priority Ladder for Conflicts
If guidance conflicts, use this priority order:
1. Safety and fault handling correctness
2. Board responsibility boundaries
3. Communication clarity and consistency
4. Simplicity/readability/maintainability
5. Optimization

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