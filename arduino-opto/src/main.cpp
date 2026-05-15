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
volatile uint16_t latestRpm = 0;

volatile bool measurementIsRunning = false;
unsigned long measurementStartMs = 0;
unsigned long pulseCount = 0;
int lastEncoderState = LOW;

void receiveI2cCommand(int byteCount);
void sendI2cResponse();
void startRpmMeasurement();
void updateRpmMeasurement();
void storeLatestRpm(uint16_t rpm);

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
            // The next I2C read from arduino-main will receive latestRpm as 2 bytes.
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
        const uint16_t rpm = latestRpm;

        Wire.write(static_cast<uint8_t>(rpm & 0xFF));
        Wire.write(static_cast<uint8_t>((rpm >> 8) & 0xFF));
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
        const float elapsedMinutes = elapsedMs / 60000.0f;
        const float rotations = pulseCount / static_cast<float>(ENCODER_HOLES);
        const float rpm = rotations / elapsedMinutes;
        uint16_t roundedRpm = 0;

        if (rpm >= UINT16_MAX) {
            roundedRpm = UINT16_MAX;
        } else {
            roundedRpm = static_cast<uint16_t>(rpm + 0.5f);
        }

        storeLatestRpm(roundedRpm);
        measurementIsRunning = false;
    }
}

void storeLatestRpm(uint16_t rpm) {
    noInterrupts();
    latestRpm = rpm;
    interrupts();
}
