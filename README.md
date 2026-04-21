# Motor Tester Bed (SPRO2)

## Overview
This project is an embedded system designed to measure and evaluate small electric motors (e.g. DC motors).

The goal is to determine unknown motor parameters by performing controlled measurements and analyzing the results within a defined tolerance range.

---

## System Architecture

The system consists of three Arduino boards and a display:

- **arduino-main**
  - central controller of the system
  - communicates with other boards and the display

- **arduino-measurement**
  - measures motor properties (current, voltage, torque)
  - may control the motor via PWM

- **arduino-opto**
  - handles optocoupler-related functionality

- **Nextion display**
  - provides the user interface
  - allows starting/stopping measurements
  - displays progress and results

---

## Communication

- Arduino ↔ Arduino: I2C  
- Main Arduino ↔ Display: USART  

---

## Project Structure

    arduino-main/
    arduino-measurement/
    arduino-opto/
    docs/
    AGENTS.md
    README.md

Each Arduino folder is an independent PlatformIO project.

---

## Current Status

- Early development phase  
- Project structure and basic setup in progress  
- System architecture still being refined  
- No hardware available yet → implementation is currently theoretical

---

## Notes

- Some system details are still being defined and may change during development  
- Hardware limitations may affect final performance and accuracy  