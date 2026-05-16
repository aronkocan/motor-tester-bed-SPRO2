#include <Arduino.h>
#include <Wire.h>
#include <INA226.h>
#include <SoftwareSerial.h>
#include <Ch376msc.h>
#include <stdio.h>
#include <string.h>

// =======================
// Runtime States
// =======================

enum class MainState {
    WAIT_FOR_SETUP,
    PREPARE_TEST,
    RUN_MEASUREMENT_CYCLE,
    EVALUATE_NEXT_STEP,
    OUTPUT_RESULTS
};

// =======================
// Measurement Modes
// =======================

enum class MeasurementMode {
    AUTOMATIC,
    MANUAL_TARGET
};

enum class ManualTargetType {
    POWER,
    TORQUE,
    RPM,
    EFFECTIVE_VOLTAGE,
    DUTY_CYCLE
};

enum class ManualSearchPhase {
    CHECK_MINIMUM_DUTY,
    CHECK_MAXIMUM_DUTY,
    SEARCH_TARGET_RANGE
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
const uint8_t USB_HOST_RX_PIN = 12;
const uint8_t USB_HOST_TX_PIN = 13;

const uint8_t MEASUREMENT_I2C_ADDRESS = 0x08;
const uint8_t OPTO_I2C_ADDRESS = 0x09;
const uint8_t MOTOR_UNDER_TEST_INA226_ADDRESS = 0x45;
const uint8_t LOAD_MOTOR_INA226_ADDRESS = 0x40;

const uint8_t CMD_SET_PWM = 0x01;
const uint8_t CMD_STOP_MOTOR = 0x02;
const uint8_t CMD_TAKE_RPM_MEASUREMENT = 0x10;
const uint8_t CMD_GET_RPM = 0x11;
const uint8_t CMD_IS_MEASUREMENT_RUNNING = 0x12;

const uint8_t NEXTION_MODE_AUTOMATIC = 0;
const uint8_t NEXTION_MODE_MANUAL = 1;
const uint8_t NEXTION_TARGET_POWER = 0;
const uint8_t NEXTION_TARGET_TORQUE = 1;
const uint8_t NEXTION_TARGET_RPM = 2;
const uint8_t NEXTION_TARGET_EFFECTIVE_VOLTAGE = 3;
const uint8_t NEXTION_TARGET_DUTY_CYCLE = 4;
const uint8_t NEXTION_NUMBER_RESPONSE = 0x71;
const uint8_t NEXTION_TOUCH_EVENT_RESPONSE = 0x65;
const uint8_t NEXTION_TOUCH_EVENT_PRESS = 0x01;
const uint8_t NEXTION_RESULTS_EXPORT_BUTTON_COMPONENT_ID = 1;
const uint8_t NEXTION_RESULTS_BACK_BUTTON_COMPONENT_ID = 2;
const uint8_t NEXTION_END_BYTE = 0xFF;
const uint8_t NEXTION_MAX_PWM_VALUE = 255;
const uint16_t NEXTION_READ_TIMEOUT_MS = 500;
const unsigned long NEXTION_BAUD_RATE = 9600;
const unsigned long USB_HOST_BAUD_RATE = 9600;
const float NEXTION_SCALED_VALUE_DIVISOR = 100.0f;

const uint8_t MAX_DATA_POINTS = 51;
const uint8_t MANUAL_MINIMUM_DUTY_CYCLE = 1;
const uint8_t MANUAL_MAXIMUM_DUTY_CYCLE = 255;
const uint8_t MANUAL_TARGET_MARGIN_PERCENT = 5;
const uint8_t MANUAL_TARGET_DUTY_MARGIN = 1;
const uint8_t MANUAL_TARGET_RPM_MINIMUM_MARGIN = 10;
const uint8_t MANUAL_TARGET_VOLTAGE_MINIMUM_MARGIN_MV = 100;
const uint8_t MANUAL_TARGET_TORQUE_MINIMUM_MARGIN_MNM = 2;
const uint16_t MANUAL_TARGET_POWER_MINIMUM_MARGIN_MW = 100;
const unsigned long MOTOR_STABILIZATION_TIME_MS = 1000;
const unsigned long MOTOR_STABILIZATION_STOP_POLL_INTERVAL_MS = 10;
const char USB_EXPORT_FILE_NAME[] = "RESULT.CSV";

const float INA226_MAX_EXPECTED_CURRENT_AMPERE = 20.0;
const float INA226_SHUNT_RESISTOR_OHM = 0.1;
const float MOTOR_EFFICIENCY = 0.75f * 0.9f;
const float PI_VALUE = 3.14159265f;

// =======================
// Global Runtime Data
// =======================

volatile MainState currentState = MainState::WAIT_FOR_SETUP;
MeasurementMode currentMeasurementMode = MeasurementMode::AUTOMATIC;
ManualTargetType selectedManualTargetType = ManualTargetType::RPM;
uint8_t automaticIntervalMinimum = 0;
uint8_t automaticIntervalMaximum = 0;
uint8_t automaticStepSize = 1;
float manualTargetValue = 0.0f;
MeasurementDataPoint dataPoints[MAX_DATA_POINTS];
uint8_t dataPointCount = 0;
uint8_t requiredDataPointCount = 0;
bool requiredDataPointsStored = false;
uint8_t currentDutyCycle = 0;
ManualSearchPhase manualSearchPhase = ManualSearchPhase::CHECK_MINIMUM_DUTY;
uint8_t manualSearchLowestDutyCycle = MANUAL_MINIMUM_DUTY_CYCLE;
uint8_t manualSearchHighestDutyCycle = MANUAL_MAXIMUM_DUTY_CYCLE;
MeasurementDataPoint bestManualTargetDataPoint = {0, 0, 0, 0, 0};
float bestManualTargetDifference = 0.0f;
bool bestManualTargetDataPointIsSet = false;
volatile bool startButtonInterruptFlag = false;
volatile bool stopButtonInterruptFlag = false;
bool optoMeasurementIsRunning = false;
uint16_t latestOptoRpm = 0;
bool usbFlashDriveIsInitialized = false;

SoftwareSerial usbHostSerial(USB_HOST_RX_PIN, USB_HOST_TX_PIN);
Ch376msc usbFlashDrive(usbHostSerial);

INA226 motorUnderTestIna226(MOTOR_UNDER_TEST_INA226_ADDRESS);
INA226 loadMotorIna226(LOAD_MOTOR_INA226_ADDRESS);

float motorUnderTestCurrentAmpere = 0.0;
float motorUnderTestVoltageVolt = 0.0;
float loadMotorCurrentAmpere = 0.0;
float loadMotorVoltageVolt = 0.0;

uint16_t calculatedPowerMilliWatt = 0;
uint16_t calculatedEffectiveVoltageMilliVolt = 0;
uint16_t calculatedTorqueMilliNewtonMeter = 0;

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

void resetPreviousMeasurement();
void getSetupInformationFromNextion();
uint32_t requestNextionNumber(const char *componentPath);
void sendNextionCommand(const char *command);
uint8_t limitNextionValueToByte(uint32_t value);
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
float getManualTargetValueInDataUnits();
float getManualDataPointValue(const MeasurementDataPoint &dataPoint);
float getManualTargetMargin(float targetValue);
bool isManualTargetWithinMargin(float measuredValue, float targetValue);
float getAbsoluteDifference(float firstValue, float secondValue);
void updateBestManualTargetDataPoint();
void finishManualTargetMeasurement();

bool readNextionTouchEvent(uint8_t &componentId);
bool readNextionByteWithTimeout(uint8_t &value);
void initializeUsbFlashDrive();
bool writeTextToUsbFile(const char *text);
bool writeDataPointCsvLineToUsbFile(const MeasurementDataPoint &dataPoint);
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
    sendDutyCycleToMeasurementBoard();
    waitForMotorToStabilize();

    handleButtonInterruptFlags();
    if (currentState != MainState::RUN_MEASUREMENT_CYCLE) {
        return;
    }

    readElectricalMeasurements();
    startOptoRpmMeasurement();

    while (isOptoMeasurementRunning()) {
        handleButtonInterruptFlags();
        if (currentState != MainState::RUN_MEASUREMENT_CYCLE) {
            return;
        }

        delay(10);
    }

    readOptoRpm();
    calculateEffectiveVoltage();
    calculatePower();
    calculateTorque();
    storeCompletedMeasurementDataPoint();

    enterState(MainState::EVALUATE_NEXT_STEP);
}

void handleEvaluateNextStepState() {
    if (currentMeasurementMode == MeasurementMode::AUTOMATIC) {
        evaluateAutomaticMeasurementProgress();
    } else {
        evaluateManualTargetMeasurementProgress();
    }
}

void handleOutputResultsState() {
    uint8_t pressedComponentId = 0;

    if (!readNextionTouchEvent(pressedComponentId)) {
        return;
    }

    if (pressedComponentId == NEXTION_RESULTS_EXPORT_BUTTON_COMPONENT_ID) {
        exportResultsUSB();
    } else if (pressedComponentId == NEXTION_RESULTS_BACK_BUTTON_COMPONENT_ID) {
        enterState(MainState::WAIT_FOR_SETUP);
    }
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
    Serial.begin(NEXTION_BAUD_RATE);
    initializeUsbFlashDrive();

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
    memset(dataPoints, 0, sizeof(dataPoints));
    dataPointCount = 0;
    requiredDataPointsStored = false;
}

void getSetupInformationFromNextion() {
    const uint32_t selectedMode = requestNextionNumber("setup.mode.val");

    if (selectedMode == NEXTION_MODE_MANUAL) {
        currentMeasurementMode = MeasurementMode::MANUAL_TARGET;
    } else {
        currentMeasurementMode = MeasurementMode::AUTOMATIC;
    }

    if (currentMeasurementMode == MeasurementMode::AUTOMATIC) {
        automaticIntervalMinimum = limitNextionValueToByte(requestNextionNumber("range.n0.val"));
        automaticIntervalMaximum = limitNextionValueToByte(requestNextionNumber("range.n1.val"));
        automaticStepSize = limitNextionValueToByte(requestNextionNumber("range.n2.val"));

        if (automaticIntervalMinimum > automaticIntervalMaximum) {
            const uint8_t oldMinimum = automaticIntervalMinimum;
            automaticIntervalMinimum = automaticIntervalMaximum;
            automaticIntervalMaximum = oldMinimum;
        }

        if (automaticStepSize == 0) {
            automaticStepSize = 1;
        }

        return;
    }

    const uint32_t selectedTargetType = requestNextionNumber("target.tarsel.val");

    if (selectedTargetType == NEXTION_TARGET_POWER) {
        selectedManualTargetType = ManualTargetType::POWER;
        manualTargetValue = static_cast<float>(requestNextionNumber("target.power.val"));
    } else if (selectedTargetType == NEXTION_TARGET_TORQUE) {
        selectedManualTargetType = ManualTargetType::TORQUE;
        manualTargetValue = static_cast<float>(requestNextionNumber("ttorque.x0.val")) / NEXTION_SCALED_VALUE_DIVISOR;
    } else if (selectedTargetType == NEXTION_TARGET_EFFECTIVE_VOLTAGE) {
        selectedManualTargetType = ManualTargetType::EFFECTIVE_VOLTAGE;
        manualTargetValue = static_cast<float>(requestNextionNumber("teff.x0.val")) / NEXTION_SCALED_VALUE_DIVISOR;
    } else if (selectedTargetType == NEXTION_TARGET_DUTY_CYCLE) {
        selectedManualTargetType = ManualTargetType::DUTY_CYCLE;
        manualTargetValue = static_cast<float>(requestNextionNumber("tduty.n0.val"));
    } else {
        selectedManualTargetType = ManualTargetType::RPM;
        manualTargetValue = static_cast<float>(requestNextionNumber("trpm.n0.val"));
    }
}

uint32_t requestNextionNumber(const char *componentPath) {
    Serial.print("get ");
    sendNextionCommand(componentPath);

    const unsigned long startTimeMs = millis();
    while ((millis() - startTimeMs) < NEXTION_READ_TIMEOUT_MS) {
        if (Serial.available() <= 0) {
            continue;
        }

        if (Serial.read() != NEXTION_NUMBER_RESPONSE) {
            continue;
        }

        uint8_t valueBytes[4] = {0, 0, 0, 0};
        for (uint8_t i = 0; i < 4; i++) {
            while (Serial.available() <= 0) {
                if ((millis() - startTimeMs) >= NEXTION_READ_TIMEOUT_MS) {
                    return 0;
                }
            }

            valueBytes[i] = Serial.read();
        }

        for (uint8_t i = 0; i < 3; i++) {
            while (Serial.available() <= 0) {
                if ((millis() - startTimeMs) >= NEXTION_READ_TIMEOUT_MS) {
                    return 0;
                }
            }

            Serial.read();
        }

        return static_cast<uint32_t>(valueBytes[0]) |
               (static_cast<uint32_t>(valueBytes[1]) << 8) |
               (static_cast<uint32_t>(valueBytes[2]) << 16) |
               (static_cast<uint32_t>(valueBytes[3]) << 24);
    }

    return 0;
}

void sendNextionCommand(const char *command) {
    Serial.print(command);
    Serial.write(NEXTION_END_BYTE);
    Serial.write(NEXTION_END_BYTE);
    Serial.write(NEXTION_END_BYTE);
}

uint8_t limitNextionValueToByte(uint32_t value) {
    if (value > NEXTION_MAX_PWM_VALUE) {
        return NEXTION_MAX_PWM_VALUE;
    }

    return static_cast<uint8_t>(value);
}

void prepareMeasurementStep() {
    if (currentMeasurementMode == MeasurementMode::MANUAL_TARGET) {
        currentDutyCycle = MANUAL_MINIMUM_DUTY_CYCLE;
        requiredDataPointCount = 1;
        manualSearchPhase = ManualSearchPhase::CHECK_MINIMUM_DUTY;
        manualSearchLowestDutyCycle = MANUAL_MINIMUM_DUTY_CYCLE;
        manualSearchHighestDutyCycle = MANUAL_MAXIMUM_DUTY_CYCLE;
        bestManualTargetDataPoint = {0, 0, 0, 0, 0};
        bestManualTargetDifference = 0.0f;
        bestManualTargetDataPointIsSet = false;
        return;
    }

    currentDutyCycle = automaticIntervalMinimum;

    const uint16_t dutyCycleRange = automaticIntervalMaximum - automaticIntervalMinimum;
    uint16_t calculatedDataPointCount = (dutyCycleRange / automaticStepSize) + 1;

    if ((dutyCycleRange % automaticStepSize) != 0) {
        calculatedDataPointCount++;
    }

    if (calculatedDataPointCount > MAX_DATA_POINTS) {
        calculatedDataPointCount = MAX_DATA_POINTS;
    }

    requiredDataPointCount = static_cast<uint8_t>(calculatedDataPointCount);
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
    const unsigned long stabilizationStartMs = millis();

    while ((millis() - stabilizationStartMs) < MOTOR_STABILIZATION_TIME_MS) {
        handleButtonInterruptFlags();
        if (currentState != MainState::RUN_MEASUREMENT_CYCLE) {
            return;
        }

        delay(MOTOR_STABILIZATION_STOP_POLL_INTERVAL_MS);
    }
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
    if (latestOptoRpm == 0) {
        calculatedTorqueMilliNewtonMeter = 0;
        return;
    }

    float electricalPowerWatt = motorUnderTestVoltageVolt * motorUnderTestCurrentAmpere;
    if (electricalPowerWatt < 0.0) {
        electricalPowerWatt = -electricalPowerWatt;
    }

    const float mechanicalPowerWatt = MOTOR_EFFICIENCY * electricalPowerWatt;
    const float angularVelocityRadiansPerSecond = (2.0f * PI_VALUE * static_cast<float>(latestOptoRpm)) / 60.0f;
    const float torqueMilliNewtonMeter = (mechanicalPowerWatt / angularVelocityRadiansPerSecond) * 1000.0f;

    if (torqueMilliNewtonMeter > 65535.0f) {
        calculatedTorqueMilliNewtonMeter = 65535;
    } else {
        calculatedTorqueMilliNewtonMeter = static_cast<uint16_t>(torqueMilliNewtonMeter + 0.5f);
    }
}

void calculatePower() {
    float powerMilliWatt = motorUnderTestVoltageVolt * motorUnderTestCurrentAmpere * 1000.0;

    if (powerMilliWatt < 0.0) {
        powerMilliWatt = -powerMilliWatt;
    }

    if (powerMilliWatt > 65535.0) {
        calculatedPowerMilliWatt = 65535;
    } else {
        calculatedPowerMilliWatt = static_cast<uint16_t>(powerMilliWatt + 0.5);
    }
}

void calculateEffectiveVoltage() {
    float effectiveVoltageMilliVolt = motorUnderTestVoltageVolt * 1000.0;
    effectiveVoltageMilliVolt *= static_cast<float>(currentDutyCycle) / 255.0;

    if (effectiveVoltageMilliVolt < 0.0) {
        effectiveVoltageMilliVolt = -effectiveVoltageMilliVolt;
    }

    if (effectiveVoltageMilliVolt > 65535.0) {
        calculatedEffectiveVoltageMilliVolt = 65535;
    } else {
        calculatedEffectiveVoltageMilliVolt = static_cast<uint16_t>(effectiveVoltageMilliVolt + 0.5);
    }
}

void storeCompletedMeasurementDataPoint() {
    MeasurementDataPoint completedDataPoint;
    completedDataPoint.dutyCycleStep = currentDutyCycle;
    completedDataPoint.effectiveVoltageMilliVolt = calculatedEffectiveVoltageMilliVolt;
    completedDataPoint.torqueMilliNewtonMeter = calculatedTorqueMilliNewtonMeter;
    completedDataPoint.powerMilliWatt = calculatedPowerMilliWatt;
    completedDataPoint.rpm = latestOptoRpm;

    if (currentMeasurementMode == MeasurementMode::MANUAL_TARGET) {
        dataPoints[0] = completedDataPoint;
        dataPointCount = 1;
        return;
    }

    if (dataPointCount >= requiredDataPointCount || dataPointCount >= MAX_DATA_POINTS) {
        requiredDataPointsStored = true;
        return;
    }

    dataPoints[dataPointCount] = completedDataPoint;
    dataPointCount++;

    if (dataPointCount >= requiredDataPointCount || dataPointCount >= MAX_DATA_POINTS) {
        requiredDataPointsStored = true;
    }
}

// ==================================
// Evaluate Next Step State functions
// ==================================

void evaluateAutomaticMeasurementProgress() {
    if (dataPointCount < requiredDataPointCount && dataPointCount < MAX_DATA_POINTS) {
        uint16_t nextDutyCycle = static_cast<uint16_t>(currentDutyCycle) + automaticStepSize;

        if (nextDutyCycle > automaticIntervalMaximum) {
            nextDutyCycle = automaticIntervalMaximum;
        }

        currentDutyCycle = static_cast<uint8_t>(nextDutyCycle);
        enterState(MainState::RUN_MEASUREMENT_CYCLE);
        return;
    }

    stopMeasurementMotor();
    enterState(MainState::OUTPUT_RESULTS);
}

void evaluateManualTargetMeasurementProgress() {
    updateBestManualTargetDataPoint();

    const float targetValue = getManualTargetValueInDataUnits();
    const float measuredValue = getManualDataPointValue(dataPoints[0]);

    if (isManualTargetWithinMargin(measuredValue, targetValue)) {
        finishManualTargetMeasurement();
        return;
    }

    if (manualSearchPhase == ManualSearchPhase::CHECK_MINIMUM_DUTY) {
        if (targetValue < measuredValue) {
            finishManualTargetMeasurement();
            return;
        }

        manualSearchPhase = ManualSearchPhase::CHECK_MAXIMUM_DUTY;
        manualSearchLowestDutyCycle = MANUAL_MINIMUM_DUTY_CYCLE;
        currentDutyCycle = MANUAL_MAXIMUM_DUTY_CYCLE;
        enterState(MainState::RUN_MEASUREMENT_CYCLE);
        return;
    }

    if (manualSearchPhase == ManualSearchPhase::CHECK_MAXIMUM_DUTY) {
        if (targetValue > measuredValue) {
            finishManualTargetMeasurement();
            return;
        }

        manualSearchPhase = ManualSearchPhase::SEARCH_TARGET_RANGE;
        manualSearchHighestDutyCycle = MANUAL_MAXIMUM_DUTY_CYCLE;
    } else if (measuredValue < targetValue) {
        manualSearchLowestDutyCycle = currentDutyCycle;
    } else {
        manualSearchHighestDutyCycle = currentDutyCycle;
    }

    if ((manualSearchHighestDutyCycle - manualSearchLowestDutyCycle) <= 1) {
        finishManualTargetMeasurement();
        return;
    }

    currentDutyCycle = manualSearchLowestDutyCycle + ((manualSearchHighestDutyCycle - manualSearchLowestDutyCycle) / 2);
    enterState(MainState::RUN_MEASUREMENT_CYCLE);
}

float getManualTargetValueInDataUnits() {
    if (selectedManualTargetType == ManualTargetType::EFFECTIVE_VOLTAGE) {
        return manualTargetValue * 1000.0f;
    }

    return manualTargetValue;
}

float getManualDataPointValue(const MeasurementDataPoint &dataPoint) {
    if (selectedManualTargetType == ManualTargetType::POWER) {
        return static_cast<float>(dataPoint.powerMilliWatt);
    }

    if (selectedManualTargetType == ManualTargetType::TORQUE) {
        return static_cast<float>(dataPoint.torqueMilliNewtonMeter);
    }

    if (selectedManualTargetType == ManualTargetType::EFFECTIVE_VOLTAGE) {
        return static_cast<float>(dataPoint.effectiveVoltageMilliVolt);
    }

    if (selectedManualTargetType == ManualTargetType::DUTY_CYCLE) {
        return static_cast<float>(dataPoint.dutyCycleStep);
    }

    return static_cast<float>(dataPoint.rpm);
}

float getManualTargetMargin(float targetValue) {
    float percentMargin = targetValue * static_cast<float>(MANUAL_TARGET_MARGIN_PERCENT) / 100.0f;

    if (selectedManualTargetType == ManualTargetType::DUTY_CYCLE) {
        if (percentMargin < static_cast<float>(MANUAL_TARGET_DUTY_MARGIN)) {
            return static_cast<float>(MANUAL_TARGET_DUTY_MARGIN);
        }
        return percentMargin;
    }

    if (selectedManualTargetType == ManualTargetType::RPM) {
        if (percentMargin < static_cast<float>(MANUAL_TARGET_RPM_MINIMUM_MARGIN)) {
            return static_cast<float>(MANUAL_TARGET_RPM_MINIMUM_MARGIN);
        }
        return percentMargin;
    }

    if (selectedManualTargetType == ManualTargetType::EFFECTIVE_VOLTAGE) {
        if (percentMargin < static_cast<float>(MANUAL_TARGET_VOLTAGE_MINIMUM_MARGIN_MV)) {
            return static_cast<float>(MANUAL_TARGET_VOLTAGE_MINIMUM_MARGIN_MV);
        }
        return percentMargin;
    }

    if (selectedManualTargetType == ManualTargetType::TORQUE) {
        if (percentMargin < static_cast<float>(MANUAL_TARGET_TORQUE_MINIMUM_MARGIN_MNM)) {
            return static_cast<float>(MANUAL_TARGET_TORQUE_MINIMUM_MARGIN_MNM);
        }
        return percentMargin;
    }

    if (percentMargin < static_cast<float>(MANUAL_TARGET_POWER_MINIMUM_MARGIN_MW)) {
        return static_cast<float>(MANUAL_TARGET_POWER_MINIMUM_MARGIN_MW);
    }
    return percentMargin;
}

bool isManualTargetWithinMargin(float measuredValue, float targetValue) {
    return getAbsoluteDifference(measuredValue, targetValue) <= getManualTargetMargin(targetValue);
}

float getAbsoluteDifference(float firstValue, float secondValue) {
    if (firstValue > secondValue) {
        return firstValue - secondValue;
    }

    return secondValue - firstValue;
}

void updateBestManualTargetDataPoint() {
    const float targetValue = getManualTargetValueInDataUnits();
    const float measuredValue = getManualDataPointValue(dataPoints[0]);
    const float currentDifference = getAbsoluteDifference(measuredValue, targetValue);

    if (!bestManualTargetDataPointIsSet || currentDifference < bestManualTargetDifference) {
        bestManualTargetDataPoint = dataPoints[0];
        bestManualTargetDifference = currentDifference;
        bestManualTargetDataPointIsSet = true;
    }
}

void finishManualTargetMeasurement() {
    if (bestManualTargetDataPointIsSet) {
        dataPoints[0] = bestManualTargetDataPoint;
        dataPointCount = 1;
    }

    stopMeasurementMotor();
    enterState(MainState::OUTPUT_RESULTS);
}

// ==============================
// Output Results State functions
// ==============================

bool readNextionTouchEvent(uint8_t &componentId) {
    while (Serial.available() > 0) {
        if (Serial.read() != NEXTION_TOUCH_EVENT_RESPONSE) {
            continue;
        }

        uint8_t pageId = 0;
        uint8_t eventType = 0;
        uint8_t endByte = 0;

        if (!readNextionByteWithTimeout(pageId) ||
            !readNextionByteWithTimeout(componentId) ||
            !readNextionByteWithTimeout(eventType)) {
            return false;
        }
        (void)pageId;

        for (uint8_t i = 0; i < 3; i++) {
            if (!readNextionByteWithTimeout(endByte) || endByte != NEXTION_END_BYTE) {
                return false;
            }
        }

        return eventType == NEXTION_TOUCH_EVENT_PRESS;
    }

    return false;
}

bool readNextionByteWithTimeout(uint8_t &value) {
    const unsigned long startTimeMs = millis();

    while ((millis() - startTimeMs) < NEXTION_READ_TIMEOUT_MS) {
        if (Serial.available() <= 0) {
            continue;
        }

        value = Serial.read();
        return true;
    }

    return false;
}

void initializeUsbFlashDrive() {
    if (usbFlashDriveIsInitialized) {
        return;
    }

    usbHostSerial.begin(USB_HOST_BAUD_RATE);
    usbFlashDrive.init();
    usbFlashDrive.setSource(0);
    usbFlashDriveIsInitialized = true;
}

bool writeTextToUsbFile(const char *text) {
    return usbFlashDrive.writeFile(const_cast<char *>(text), strlen(text));
}

bool writeDataPointCsvLineToUsbFile(const MeasurementDataPoint &dataPoint) {
    char lineBuffer[42];
    snprintf(lineBuffer,
             sizeof(lineBuffer),
             "%u,%u,%u,%u,%u\r\n",
             dataPoint.dutyCycleStep,
             dataPoint.effectiveVoltageMilliVolt,
             dataPoint.torqueMilliNewtonMeter,
             dataPoint.powerMilliWatt,
             dataPoint.rpm);

    return writeTextToUsbFile(lineBuffer);
}

void exportResultsUSB() {
    initializeUsbFlashDrive();
    usbFlashDrive.checkIntMessage();

    if (!usbFlashDrive.driveReady()) {
        return;
    }

    usbFlashDrive.setFileName(USB_EXPORT_FILE_NAME);
    usbFlashDrive.deleteFile();
    usbFlashDrive.setFileName(USB_EXPORT_FILE_NAME);
    usbFlashDrive.openFile();

    bool exportSucceeded = writeTextToUsbFile("duty,effective_mV,torque_mNm,power_mW,rpm\r\n");

    for (uint8_t i = 0; i < dataPointCount && exportSucceeded; i++) {
        exportSucceeded = writeDataPointCsvLineToUsbFile(dataPoints[i]);
    }

    usbFlashDrive.closeFile();
}