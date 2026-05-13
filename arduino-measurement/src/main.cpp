#include <Arduino.h>
#include <Wire.h>

// I2C address and command IDs from docs/communication.md.
const uint8_t MEASUREMENT_I2C_ADDRESS = 0x08;
const uint8_t CMD_SET_PWM = 0x01;
const uint8_t CMD_STOP_MOTOR = 0x02;

// PWM output pin from docs/pinouts.md: arduino-measurement D6 / PD6.
const uint8_t MOTOR_PWM_PIN = 6;

volatile bool hasPendingPwmValue = false;
volatile uint8_t pendingPwmValue = 0;

void receiveI2cCommand(int byteCount);
void setMotorPwm(uint8_t pwmValue);
void stopMotor();

void setup() {
    pinMode(MOTOR_PWM_PIN, OUTPUT);
    stopMotor();

    Wire.begin(MEASUREMENT_I2C_ADDRESS);
    Wire.onReceive(receiveI2cCommand);
}

void loop() {
    if (hasPendingPwmValue) {
        noInterrupts();
        const uint8_t pwmValue = pendingPwmValue;
        hasPendingPwmValue = false;
        interrupts();

        setMotorPwm(pwmValue);
    }
}

void receiveI2cCommand(int byteCount) {
    if (byteCount < 1) {
        return;
    }

    const uint8_t command = Wire.read();

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

        default:
            // Unknown commands are ignored so the last valid PWM value stays active.
            break;
    }

    while (Wire.available() > 0) {
        Wire.read();
    }
}

void setMotorPwm(uint8_t pwmValue) {
    analogWrite(MOTOR_PWM_PIN, pwmValue);
}

void stopMotor() {
    setMotorPwm(0);
}