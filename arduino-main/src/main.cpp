#include <Arduino.h>
#include <Wire.h>

// =======================
// Runtime States
// =======================

enum class MainState {
    WAIT_FOR_SETUP,
    PREPARE_TEST,
    RUN_MEASUREMENT_CYCLE,
    EVALUATE_NEXT_STEP,
    OUTPUT_RESULTS,
    ERROR
};

// =======================
// Measurement Modes
// =======================

enum class MeasurementMode {
    AUTOMATIC,
    MANUAL_TARGET
};

// =======================
// Measurement Data
// =======================

struct MeasurementDataPoint {
    uint8_t dutyCycleStep;
    uint16_t effectiveVoltageMilliVolt;
    uint16_t torqueMilliNewtonMeter;
    uint16_t powerMilliWatt;
    uint16_t rpm;
};

// =======================
// Pin and Protocol Constants
// =======================

const uint8_t START_BUTTON_PIN = 2;
const uint8_t STOP_BUTTON_PIN = 3;
const uint8_t START_LED_PIN = 6;
const uint8_t STOP_LED_PIN = 7;

const uint8_t MEASUREMENT_I2C_ADDRESS = 0x08;
const uint8_t OPTO_I2C_ADDRESS = 0x09;

const uint8_t CMD_SET_PWM = 0x01;
const uint8_t CMD_STOP_MOTOR = 0x02;
const uint8_t CMD_TAKE_RPM_MEASUREMENT = 0x10;
const uint8_t CMD_GET_RPM = 0x11;
const uint8_t CMD_IS_MEASUREMENT_RUNNING = 0x12;

const uint8_t MAX_DATA_POINTS = 51;

// =======================
// Global Runtime Data
// =======================

volatile MainState currentState = MainState::WAIT_FOR_SETUP;
MeasurementMode currentMeasurementMode = MeasurementMode::AUTOMATIC;
MeasurementDataPoint dataPoints[MAX_DATA_POINTS];
uint8_t dataPointCount = 0;
uint8_t currentDutyCycle = 0;
volatile bool startButtonInterruptFlag = false;
volatile bool stopButtonInterruptFlag = false;

// =======================
// Function Declarations
// =======================

void initializeSystem();
void runStateMachine();
void handleButtonInterruptFlags();
void handleStartButtonInterrupt();
void handleStopButtonInterrupt();
void enterState(MainState nextState);

void handleWaitForSetupState();
void handlePrepareTestState();
void handleRunMeasurementCycleState();
void handleEvaluateNextStepState();
void handleOutputResultsState();
void handleErrorState();

void resetPreviousMeasurement();
void getSetupInformationFromNextion();
void prepareMeasurementStep();

void sendDutyCycleToMeasurementBoard();
void stopMeasurementMotor();
void waitForMotorToStabilize();
void startOptoRpmMeasurement();
bool isOptoMeasurementRunning();
float readOptoRpm();
void readElectricalMeasurements();
void calculateTorque();
void calculatePower();
void calculateEffectiveVoltage();
void storeCompletedMeasurementDataPoint();

void evaluateAutomaticMeasurementProgress();
void evaluateManualTargetMeasurementProgress();
void selectNextMeasurementStep();

void outputResultsToNextion();
void exportResultsUSB();

// =======================
// Setup / Loop
// =======================

void setup() {
    initializeSystem();
}

void loop() {
    handleButtonInterruptFlags();
    runStateMachine();
}

// =======================
// State Machine
// =======================

void runStateMachine() {
    switch (currentState) {
        case MainState::WAIT_FOR_SETUP:
            handleWaitForSetupState();
            break;

        case MainState::PREPARE_TEST:
            handlePrepareTestState();
            break;

        case MainState::RUN_MEASUREMENT_CYCLE:
            handleRunMeasurementCycleState();
            break;

        case MainState::EVALUATE_NEXT_STEP:
            handleEvaluateNextStepState();
            break;

        case MainState::OUTPUT_RESULTS:
            handleOutputResultsState();
            break;

        case MainState::ERROR:
            handleErrorState();
            break;
    }
}

void enterState(MainState nextState) {
    currentState = nextState;
}

// =======================
// State Handlers
// =======================

void handleWaitForSetupState() {
}

void handlePrepareTestState() {
    resetPreviousMeasurement();
    getSetupInformationFromNextion();
    prepareMeasurementStep();

    enterState(MainState::RUN_MEASUREMENT_CYCLE);
}

void handleRunMeasurementCycleState() {
}

void handleEvaluateNextStepState() {
}

void handleOutputResultsState() {
}

void handleErrorState() {
}

// =======================
// Function Implementations
// =======================

void initializeSystem() {
    pinMode(START_BUTTON_PIN, INPUT);
    pinMode(STOP_BUTTON_PIN, INPUT);
    pinMode(START_LED_PIN, OUTPUT);
    pinMode(STOP_LED_PIN, OUTPUT);

    attachInterrupt(digitalPinToInterrupt(START_BUTTON_PIN), handleStartButtonInterrupt, RISING);
    attachInterrupt(digitalPinToInterrupt(STOP_BUTTON_PIN), handleStopButtonInterrupt, RISING);
}

void handleStartButtonInterrupt() {
    if (currentState == MainState::WAIT_FOR_SETUP) {
        startButtonInterruptFlag = true;
    }
}

void handleStopButtonInterrupt() {
    if (currentState != MainState::WAIT_FOR_SETUP) {
        stopButtonInterruptFlag = true;
    }
}

void handleButtonInterruptFlags() {
    bool shouldStart = false;
    bool shouldStop = false;

    noInterrupts();
    if (startButtonInterruptFlag) {
        startButtonInterruptFlag = false;
        shouldStart = true;
    }
    if (stopButtonInterruptFlag) {
        stopButtonInterruptFlag = false;
        shouldStop = true;
    }
    interrupts();

    if (shouldStop) {
        stopMeasurementMotor();
        enterState(MainState::WAIT_FOR_SETUP);
        return;
    }

    if (shouldStart && currentState == MainState::WAIT_FOR_SETUP) {
        enterState(MainState::PREPARE_TEST);
    }
}

// ============================
// Prepare Test State functions
// ============================

void resetPreviousMeasurement() {
}

void getSetupInformationFromNextion() {
    // This includes getting the information, validating it and storing it
}

void prepareMeasurementStep() {
}

// =====================================
// Run Measurement Cycle State functions
// =====================================

void sendDutyCycleToMeasurementBoard() {
}

void stopMeasurementMotor() {
}

void waitForMotorToStabilize() {
}

void startOptoRpmMeasurement() {
}

bool isOptoMeasurementRunning() {
}

float readOptoRpm() {
}

void readElectricalMeasurements() {
}

void calculateTorque() {
}

void calculatePower() {
}

void calculateEffectiveVoltage() {
}

void storeCompletedMeasurementDataPoint() {
}

// ==================================
// Evaluate Next Step State functions
// ==================================

void evaluateAutomaticMeasurementProgress() {
}

void evaluateManualTargetMeasurementProgress() {
}

void selectNextMeasurementStep() {
}

// ==============================
// Output Results State functions
// ==============================

void outputResultsToNextion() {
}

void exportResultsUSB() {
}

// ============================
// Error State functions
// ============================
