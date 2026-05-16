#include <Arduino.h>
#include <Wire.h>
#include <INA226.h>

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
const uint8_t MOTOR_UNDER_TEST_INA226_ADDRESS = 0x40;
const uint8_t LOAD_MOTOR_INA226_ADDRESS = 0x41;

const uint8_t CMD_SET_PWM = 0x01;
const uint8_t CMD_STOP_MOTOR = 0x02;
const uint8_t CMD_TAKE_RPM_MEASUREMENT = 0x10;
const uint8_t CMD_GET_RPM = 0x11;
const uint8_t CMD_IS_MEASUREMENT_RUNNING = 0x12;

const uint8_t MAX_DATA_POINTS = 51;

const float INA226_MAX_EXPECTED_CURRENT_AMPERE = 20.0;
const float INA226_SHUNT_RESISTOR_OHM = 0.002;

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
bool optoMeasurementIsRunning = false;
uint16_t latestOptoRpm = 0;

INA226 motorUnderTestIna226(MOTOR_UNDER_TEST_INA226_ADDRESS);
INA226 loadMotorIna226(LOAD_MOTOR_INA226_ADDRESS);

float motorUnderTestCurrentAmpere = 0.0;
float motorUnderTestVoltageVolt = 0.0;
float loadMotorCurrentAmpere = 0.0;
float loadMotorVoltageVolt = 0.0;

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
uint16_t readOptoRpm();
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

    Wire.begin();

    motorUnderTestIna226.begin();
    loadMotorIna226.begin();
    motorUnderTestIna226.setMaxCurrentShunt(INA226_MAX_EXPECTED_CURRENT_AMPERE, INA226_SHUNT_RESISTOR_OHM);
    loadMotorIna226.setMaxCurrentShunt(INA226_MAX_EXPECTED_CURRENT_AMPERE, INA226_SHUNT_RESISTOR_OHM);

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
    Wire.beginTransmission(MEASUREMENT_I2C_ADDRESS);
    Wire.write(CMD_SET_PWM);
    Wire.write(currentDutyCycle);
    Wire.endTransmission();
}

void stopMeasurementMotor() {
    currentDutyCycle = 0;

    Wire.beginTransmission(MEASUREMENT_I2C_ADDRESS);
    Wire.write(CMD_STOP_MOTOR);
    Wire.endTransmission();
}

void waitForMotorToStabilize() {
    delay(1000);
}

void startOptoRpmMeasurement() {
    Wire.beginTransmission(OPTO_I2C_ADDRESS);
    Wire.write(CMD_TAKE_RPM_MEASUREMENT);
    Wire.endTransmission();

    optoMeasurementIsRunning = true;
}

bool isOptoMeasurementRunning() {
    Wire.beginTransmission(OPTO_I2C_ADDRESS);
    Wire.write(CMD_IS_MEASUREMENT_RUNNING);
    Wire.endTransmission();

    Wire.requestFrom(OPTO_I2C_ADDRESS, static_cast<uint8_t>(1));
    if (Wire.available() >= 1) {
        optoMeasurementIsRunning = (Wire.read() == 1);
    }

    return optoMeasurementIsRunning;
}

uint16_t readOptoRpm() {
    Wire.beginTransmission(OPTO_I2C_ADDRESS);
    Wire.write(CMD_GET_RPM);
    Wire.endTransmission();

    Wire.requestFrom(OPTO_I2C_ADDRESS, static_cast<uint8_t>(2));
    if (Wire.available() >= 2) {
        const uint8_t lowRpmByte = Wire.read();
        const uint8_t highRpmByte = Wire.read();
        latestOptoRpm = static_cast<uint16_t>(lowRpmByte) | (static_cast<uint16_t>(highRpmByte) << 8);
    }

    return latestOptoRpm;
}

void readElectricalMeasurements() {
    motorUnderTestCurrentAmpere = motorUnderTestIna226.getCurrent();
    motorUnderTestVoltageVolt = motorUnderTestIna226.getBusVoltage();

    loadMotorCurrentAmpere = loadMotorIna226.getCurrent();
    loadMotorVoltageVolt = loadMotorIna226.getBusVoltage();
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
