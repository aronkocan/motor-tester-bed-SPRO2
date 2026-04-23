# AGENTS.md

## Project Purpose and Current Constraints
This project is an embedded motor testing system for measuring and evaluating small electric motors (such as DC motors). The goal is to determine unknown motor parameters by running controlled measurements and calculating values within acceptable tolerance ranges.

Current constraints:
- The project is in an early, partly theoretical phase
- Not all hardware is available yet
- Some implementation details will only be confirmed during real hardware testing

Implementation expectations under current constraints:
- Prefer modular skeletons and placeholders over speculative full implementations
- Keep assumptions conservative and explicit (use TODO comments where behavior is not yet confirmed)
- Do not claim hardware behavior is verified unless tested on real hardware

---

## System Boundaries and Responsibilities
The system consists of three Arduino-based controllers and one Nextion display.

### `arduino-main` (central coordinator)
Main responsibilities:
- high-level control logic and system state handling
- phase transitions (setup → measurement → result)
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

Communication rules:
- Keep message meaning explicit and simple
- Avoid unnecessary protocol complexity
- Keep protocol/message definitions consistent across all relevant boards
- If communication behavior is uncertain, document assumptions and leave TODOs

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
- Nextion display actions
- hardware button inputs on `arduino-main`

Both sources must map to the same state transition path and safety handling behavior.

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
- avoid unnecessary abstraction and dynamic memory usage
- prefer maintainability and debuggability over cleverness
- avoid premature optimization and over-engineering
- preserve existing structure unless a clear improvement is needed
- modify only files relevant to the requested task

Language/style:
- C++ in Arduino environment
- Prefer straightforward implementations over advanced C++ patterns unless justified

---

## Priority Ladder for Conflicts
If guidance conflicts, use this priority order:
1. Safety and fault handling correctness
2. Board responsibility boundaries
3. Communication clarity and consistency
4. Simplicity/readability/maintainability
5. Optimization

---

## Build and Verification Policy
Build system:
- Each Arduino folder is an independent PlatformIO project with its own `platformio.ini`

Verification policy by environment:

### 1) Cloud environment verification (tooling may be unavailable)
Use this path when running in managed/cloud agent environments where PlatformIO may not be preinstalled.

Required checks:
- Run and report the following board build commands:
  - `pio run -d <board-folder>`
  - `python -m platformio run -d <board-folder>`
- If both fail due to missing PlatformIO tooling, treat the result as an environment limitation (not immediate code failure).

Required reporting behavior:
1. Include exact failing command(s) and error text in the check output.
2. Mark build status as `PENDING (environment missing PlatformIO)`.
3. Continue with non-build checks that are still possible:
   - static code review for touched modules
   - interface/protocol consistency review where applicable
   - explicit TODO coverage for hardware-only assumptions
4. Do not claim compile success in cloud-only runs without successful build output.

### 2) Local/IDE verification (developer machine)
Use this path when PlatformIO is available locally (for example via VS Code + PlatformIO or CLI install).

Tooling options (either is acceptable):
- PlatformIO Core CLI on `PATH` (`pio ...`)
- Python module (`python -m platformio ...`)

Build scope rules:
- If a change is board-local, build only the affected board project(s).
- If a change touches shared protocol/interfaces/pin mapping assumptions, build all three:
  - `arduino-main`
  - `arduino-measurement`
  - `arduino-opto`

### 3) Hardware-aware validation status
- Without hardware access, runtime behavior is provisional.
- Leave explicit TODOs for hardware-only validation gaps.
- Do not claim hardware behavior as verified unless tested on real hardware.

Recommended reproducibility baseline:
- Document which PlatformIO path is used by the team (CLI vs Python module) in CI/devcontainer setup.
- Keep command examples in docs aligned with the chosen baseline.

---

## Pin Assignments
Pin assignments are defined in `docs/pinouts.md`.

Pin rules:
- follow documented pin mappings
- do not invent new mappings unless explicitly instructed
- keep board-specific pin usage separated by Arduino
