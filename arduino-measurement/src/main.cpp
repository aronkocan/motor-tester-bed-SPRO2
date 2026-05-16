#include <Arduino.h>
#include <Wire.h>

// I2C address and command IDs from docs/communication.md.
const uint8_t MEASUREMENT_I2C_ADDRESS = 0x08;
const uint8_t CMD_SET_PWM = 0x01;
const uint8_t CMD_STOP_MOTOR = 0x02;
const uint8_t CMD_GET_TEST_MOTOR_VOLTAGE = 0x03;

// PWM output pin from docs/pinouts.md: arduino-measurement D6 / PD6.
const uint8_t MOTOR_PWM_PIN = 6;

// Analog voltage input from docs/pinouts.md: arduino-measurement A0 / PC0.
const uint8_t TEST_MOTOR_VOLTAGE_PIN = A0;
const uint16_t ADC_REFERENCE_MILLI_VOLT = 5000;
const uint16_t ADC_MAX_READING = 1023;
const uint8_t ADC_SAMPLE_COUNT = 10;
const uint8_t TEST_MOTOR_VOLTAGE_DIVIDER_MULTIPLIER = 6;

volatile bool hasPendingPwmValue = false;
volatile uint8_t pendingPwmValue = 0;
volatile uint8_t lastRequestedCommand = 0;
volatile bool shouldReadTestMotorVoltage = true;
volatile uint16_t latestTestMotorVoltageMilliVolt = 0;

void receiveI2cCommand(int byteCount);
void sendI2cResponse();
uint16_t readTestMotorVoltageMilliVolt();
void setMotorPwm(uint8_t pwmValue);
void stopMotor();

void setup() {
    pinMode(MOTOR_PWM_PIN, OUTPUT);
    pinMode(TEST_MOTOR_VOLTAGE_PIN, INPUT);
    stopMotor();

    Wire.begin(MEASUREMENT_I2C_ADDRESS);
    Wire.onReceive(receiveI2cCommand);
    Wire.onRequest(sendI2cResponse);
}

void loop() {
    if (hasPendingPwmValue) {
        noInterrupts();
        const uint8_t pwmValue = pendingPwmValue;
        hasPendingPwmValue = false;
        interrupts();

        setMotorPwm(pwmValue);
    }

    if (shouldReadTestMotorVoltage) {
        noInterrupts();
        shouldReadTestMotorVoltage = false;
        interrupts();

        const uint16_t voltageMilliVolt = readTestMotorVoltageMilliVolt();

        noInterrupts();
        latestTestMotorVoltageMilliVolt = voltageMilliVolt;
        interrupts();
    }
}

void receiveI2cCommand(int byteCount) {
    if (byteCount < 1) {
        return;
    }

    const uint8_t command = Wire.read();
    lastRequestedCommand = command;

    switch (command) {
        case CMD_STOP_MOTOR:
            pendingPwmValue = 0;
            hasPendingPwmValue = true;
            break;

        case CMD_SET_PWM:
            if (Wire.available() >= 1) {
                pendingPwmValue = Wire.read();
                hasPendingPwmValue = true;
            }
            break;

        case CMD_GET_TEST_MOTOR_VOLTAGE:
            // Keep the I2C callback short. The loop takes and caches the ADC sample.
            shouldReadTestMotorVoltage = true;
            break;

        default:
            // Unknown commands are ignored so the last valid PWM value stays active.
            break;
    }

    while (Wire.available() > 0) {
        Wire.read();
    }
}

void sendI2cResponse() {
    if (lastRequestedCommand == CMD_GET_TEST_MOTOR_VOLTAGE) {
        const uint16_t voltageMilliVolt = latestTestMotorVoltageMilliVolt;
        Wire.write(static_cast<uint8_t>(voltageMilliVolt & 0xFF));
        Wire.write(static_cast<uint8_t>((voltageMilliVolt >> 8) & 0xFF));
    } else {
        Wire.write(static_cast<uint8_t>(0));
    }
}

uint16_t readTestMotorVoltageMilliVolt() {
    uint32_t adcReadingSum = 0;

    for (uint8_t i = 0; i < ADC_SAMPLE_COUNT; i++) {
        adcReadingSum += analogRead(TEST_MOTOR_VOLTAGE_PIN);
    }

    const uint32_t adcAverageDivisor = static_cast<uint32_t>(ADC_SAMPLE_COUNT) * ADC_MAX_READING;
    const uint32_t adcPinVoltageMilliVolt = ((adcReadingSum * ADC_REFERENCE_MILLI_VOLT) + (adcAverageDivisor / 2)) / adcAverageDivisor;
    const uint32_t realMotorVoltageMilliVolt = adcPinVoltageMilliVolt * TEST_MOTOR_VOLTAGE_DIVIDER_MULTIPLIER;

    if (realMotorVoltageMilliVolt > UINT16_MAX) {
        return UINT16_MAX;
    }

    return static_cast<uint16_t>(realMotorVoltageMilliVolt);
}

void setMotorPwm(uint8_t pwmValue) {
    analogWrite(MOTOR_PWM_PIN, pwmValue);
}

void stopMotor() {
    setMotorPwm(0);
}