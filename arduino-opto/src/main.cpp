#include <Arduino.h>
#include <Wire.h>

// I2C address and command IDs from docs/communication.md.
const uint8_t OPTO_I2C_ADDRESS = 0x09;
const uint8_t CMD_TAKE_RPM_MEASUREMENT = 0x10;
const uint8_t CMD_GET_RPM = 0x11;
const uint8_t CMD_IS_MEASUREMENT_RUNNING = 0x12;

// Encoder input pin from docs/pinouts.md: arduino-opto D4 / PD4.
const uint8_t ENCODER_PULSE_PIN = 4;

// The encoder wheel has 40 holes, so 40 pulses are one full shaft rotation.
const uint8_t ENCODER_HOLES = 40;

// Keep the measurement window simple and predictable.
const unsigned long MEASUREMENT_WINDOW_MS = 1000;

volatile bool shouldStartMeasurement = false;
volatile uint8_t lastRequestedCommand = 0;
volatile uint8_t latestRpmBytes[sizeof(float)] = {0, 0, 0, 0};

volatile bool measurementIsRunning = false;
unsigned long measurementStartMs = 0;
unsigned long pulseCount = 0;
int lastEncoderState = LOW;

void receiveI2cCommand(int byteCount);
void sendI2cResponse();
void startRpmMeasurement();
void updateRpmMeasurement();
void storeLatestRpm(float rpm);

void setup() {
    pinMode(ENCODER_PULSE_PIN, INPUT_PULLUP);
    lastEncoderState = digitalRead(ENCODER_PULSE_PIN);

    Wire.begin(OPTO_I2C_ADDRESS);
    Wire.onReceive(receiveI2cCommand);
    Wire.onRequest(sendI2cResponse);
}

void loop() {
    if (shouldStartMeasurement) {
        noInterrupts();
        shouldStartMeasurement = false;
        interrupts();

        startRpmMeasurement();
    }

    updateRpmMeasurement();
}

void receiveI2cCommand(int byteCount) {
    if (byteCount < 1) {
        return;
    }

    const uint8_t command = Wire.read();
    lastRequestedCommand = command;

    switch (command) {
        case CMD_TAKE_RPM_MEASUREMENT:
            shouldStartMeasurement = true;
            break;

        case CMD_GET_RPM:
            // The next I2C read from arduino-main will receive latestRpmBytes.
            break;

        case CMD_IS_MEASUREMENT_RUNNING:
            // The next I2C read from arduino-main will receive 1 if running, 0 if not.
            break;

        default:
            // Unknown commands are ignored. The latest completed RPM value stays available.
            break;
    }

    while (Wire.available() > 0) {
        Wire.read();
    }
}

void sendI2cResponse() {
    if (lastRequestedCommand == CMD_GET_RPM) {
        uint8_t rpmBytes[sizeof(float)];

        for (uint8_t i = 0; i < sizeof(float); i++) {
            rpmBytes[i] = latestRpmBytes[i];
        }

        Wire.write(rpmBytes, sizeof(rpmBytes));
    } else if (lastRequestedCommand == CMD_IS_MEASUREMENT_RUNNING) {
        const uint8_t status = (measurementIsRunning || shouldStartMeasurement) ? 1 : 0;
        Wire.write(status);
    } else {
        Wire.write(static_cast<uint8_t>(0));
    }
}

void startRpmMeasurement() {
    measurementIsRunning = true;
    measurementStartMs = millis();
    pulseCount = 0;
    lastEncoderState = digitalRead(ENCODER_PULSE_PIN);
}

void updateRpmMeasurement() {
    if (!measurementIsRunning) {
        return;
    }

    const int currentEncoderState = digitalRead(ENCODER_PULSE_PIN);

    if (lastEncoderState == LOW && currentEncoderState == HIGH) {
        pulseCount++;
    }

    lastEncoderState = currentEncoderState;

    const unsigned long elapsedMs = millis() - measurementStartMs;
    if (elapsedMs >= MEASUREMENT_WINDOW_MS) {
        measurementIsRunning = false;

        const float elapsedMinutes = elapsedMs / 60000.0f;
        const float rotations = pulseCount / static_cast<float>(ENCODER_HOLES);
        const float rpm = rotations / elapsedMinutes;

        storeLatestRpm(rpm);
    }
}

void storeLatestRpm(float rpm) {
    union {
        float value;
        uint8_t bytes[sizeof(float)];
    } rpmData;

    rpmData.value = rpm;

    noInterrupts();
    for (uint8_t i = 0; i < sizeof(float); i++) {
        latestRpmBytes[i] = rpmData.bytes[i];
    }
    interrupts();
}
