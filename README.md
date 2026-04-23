# Motor Tester Bed (SPRO2)

## Overview
Motor Tester Bed (SPRO2) is an embedded multi-controller system for measuring and evaluating small electric motors (for example, DC motors). The intended outcome is to derive unknown motor parameters through controlled measurement runs and compare them against acceptable tolerance ranges.

> **Project maturity:** early-stage and partly theoretical. Not all hardware is available yet, so parts of the implementation remain provisional until real hardware validation is complete.

---

## System Composition
The system is split across three Arduino Nano-based controllers plus one Nextion display:

- **`arduino-main` (central coordinator)**
  - high-level control and state handling
  - runtime phase transitions (`setup → measurement → result`)
  - coordination of the other Arduino boards
  - user command intake from hardware controls
  - Nextion communication over USART

- **`arduino-measurement`**
  - sensor reading
  - measurement-side processing
  - motor drive/PWM output logic (where applicable)
  - sends measurement status/data to `arduino-main`

- **`arduino-opto`**
  - optocoupler input/output handling
  - encoder pulse/RPM signal handling
  - sends optocoupler-derived status/data to `arduino-main`

- **Nextion display**
  - setup input and operator interaction
  - progress and result presentation

---

## Responsibility Boundaries
Responsibilities are intentionally separated by board:

- Keep runtime logic within the board that owns it.
- Do not shift board responsibilities unless there is an explicit design reason.
- Shared protocol/message definitions are allowed when required for cross-board consistency.

---

## Communication Contracts
- **`arduino-main` ↔ `arduino-measurement`:** I2C
- **`arduino-main` ↔ `arduino-opto`:** I2C
- **`arduino-main` ↔ Nextion display:** USART

Design expectations:

- Message meaning should stay explicit and simple.
- Avoid unnecessary protocol complexity.
- Keep protocol definitions consistent across all participating boards.
- Where communication behavior is uncertain, document assumptions and leave TODOs.

---

## Runtime Model
The runtime model has three phases:

1. **Setup phase**
   - User enters/selects required setup inputs.
   - System prepares test conditions.
2. **Measurement phase**
   - Measurement runs and reports periodic progress.
   - Stop must remain immediately available.
3. **Result phase**
   - System transitions automatically when measurement completes.
   - Measured/calculated outputs are shown.

### Start/Stop Sources
Start/stop commands currently originate from:

- Hardware buttons connected to `arduino-main`

All start/stop requests must follow the same state transition and safety behavior.

---

## Fault and Error Handling Expectations
The system should visibly notify the user when:

- required setup input is missing
- motor behavior appears invalid/abnormal
- measurement fails or abnormal behavior is detected

Fault handling rules:

- On serious measurement faults, stop safely.
- Present a clear fault state to the user.
- Do not ignore faults silently.
- Keep fault handling explicit and extendable.

---

## Pin Assignments
Pin mappings are maintained in [`docs/pinouts.md`](docs/pinouts.md).

Use that file as the source of truth before making wiring or firmware pin changes.

---

## Repository Structure

```text
arduino-main/
arduino-measurement/
arduino-opto/
docs/
AGENTS.md
README.md
```

Each Arduino folder is an independent PlatformIO project with its own `platformio.ini`.

---

## Build & Verification
In cloud/managed environments, PlatformIO tooling may be unavailable.

Recommended board build commands:

- `pio run -d <board-folder>`
- `python -m platformio run -d <board-folder>`

If both fail because PlatformIO is missing, treat status as:

- **`PENDING (environment missing PlatformIO)`**

and continue with static consistency checks (interfaces, documentation alignment, TODO coverage for hardware-only assumptions).

---

## Engineering Principles
- Keep logic simple, readable, and embedded-friendly.
- Avoid unnecessary abstraction and dynamic memory usage.
- Prefer maintainability/debuggability over cleverness.
- Avoid premature optimization.
- Preserve structure unless a clear improvement is needed.

Language baseline: Arduino C++ with straightforward implementation style.

---

## Current Validation Status
- Runtime behavior is **provisional** without real hardware tests.
- Hardware-dependent behavior should be treated as unverified until physically tested.
- Keep assumptions explicit and conservative.
