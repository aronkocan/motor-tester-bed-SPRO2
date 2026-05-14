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

MainState currentState = MainState::WAIT_FOR_SETUP;
MeasurementMode currentMeasurementMode = MeasurementMode::AUTOMATIC;
MeasurementDataPoint dataPoints[MAX_DATA_POINTS];
uint8_t dataPointCount = 0;
uint8_t currentDutyCycle = 0;

// =======================
// Function Declarations
// =======================

void initializeSystem();
void runStateMachine();
void handleStopButton();
void enterState(MainState nextState);

void handleWaitForSetupState();
void handlePrepareTestState();
void handleRunMeasurementCycleState();
void handleEvaluateNextStepState();
void handleOutputResultsState();
void handleErrorState();

void sendMeasurementPwm(uint8_t pwmValue);
void stopMeasurementMotor();
void startOptoRpmMeasurement();
bool isOptoMeasurementRunning();
float readOptoRpm();

void resetMeasurementData();
void storeCurrentDataPoint(float rpm);
uint16_t clampFloatToUInt16(float value);

void updateNextionStatus();
bool readSetupFromNextion();
uint16_t readMotorVoltageMilliVolt();
uint16_t readMotorCurrentMilliAmp();

// =======================
// Setup / Loop
// =======================

void setup() {
    initializeSystem();
}

void loop() {
    handleStopButton();
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
// Function Stubs
// =======================

void initializeSystem() {
}

void handleStopButton() {
}

void sendMeasurementPwm(uint8_t pwmValue) {
}

void stopMeasurementMotor() {
}

void startOptoRpmMeasurement() {
}

bool isOptoMeasurementRunning() {
}

float readOptoRpm() {
}

void resetMeasurementData() {
}

void storeCurrentDataPoint(float rpm) {
}

uint16_t clampFloatToUInt16(float value) {
}

void updateNextionStatus() {
}

bool readSetupFromNextion() {
}

uint16_t readMotorVoltageMilliVolt() {
}

uint16_t readMotorCurrentMilliAmp() {
}
