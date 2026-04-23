#include <Arduino.h>

namespace {
constexpr uint8_t kOptoPulsePin = 4;
constexpr uint8_t kEncoderHolesPerRevolution = 20;
constexpr unsigned long kRpmUpdateIntervalMs = 250;
constexpr unsigned long kNoPulseTimeoutMs = 1000;

volatile uint32_t g_pulseCount = 0;
volatile uint8_t g_lastPinState = LOW;
volatile unsigned long g_lastPulseMs = 0;

uint32_t g_lastPulseSnapshot = 0;
unsigned long g_lastRpmUpdateMs = 0;
float g_currentRpm = 0.0f;

void setupPulseCapture() {
  pinMode(kOptoPulsePin, INPUT);

  g_lastPinState = digitalRead(kOptoPulsePin);

  // Enable pin-change interrupts for port D (PCINT[23:16]).
  PCICR |= _BV(PCIE2);
  // D4 maps to PD4 / PCINT20.
  PCMSK2 |= _BV(PCINT20);
}

void updateRpmFromPulseCount(unsigned long nowMs) {
  const unsigned long elapsedMs = nowMs - g_lastRpmUpdateMs;
  if (elapsedMs < kRpmUpdateIntervalMs) {
    return;
  }

  noInterrupts();
  const uint32_t pulseSnapshot = g_pulseCount;
  const unsigned long lastPulseMsSnapshot = g_lastPulseMs;
  interrupts();

  const uint32_t pulsesInWindow = pulseSnapshot - g_lastPulseSnapshot;
  g_lastPulseSnapshot = pulseSnapshot;

  if (pulsesInWindow == 0 && (nowMs - lastPulseMsSnapshot) >= kNoPulseTimeoutMs) {
    g_currentRpm = 0.0f;
  } else if (pulsesInWindow > 0) {
    const float revolutions = static_cast<float>(pulsesInWindow) /
                              static_cast<float>(kEncoderHolesPerRevolution);
    g_currentRpm = (revolutions * 60000.0f) / static_cast<float>(elapsedMs);
  }

  g_lastRpmUpdateMs = nowMs;

  // TODO: Replace Serial output with I2C status message to arduino-main.
  Serial.print(F("Pulse count: "));
  Serial.print(pulseSnapshot);
  Serial.print(F(" | RPM: "));
  Serial.println(g_currentRpm, 2);
}
}  // namespace

ISR(PCINT2_vect) {
  const uint8_t currentState = (PIND & _BV(PD4)) ? HIGH : LOW;

  if (currentState != g_lastPinState && currentState == HIGH) {
    ++g_pulseCount;
    g_lastPulseMs = millis();
  }

  g_lastPinState = currentState;
}

void setup() {
  Serial.begin(115200);
  setupPulseCapture();

  const unsigned long nowMs = millis();
  g_lastRpmUpdateMs = nowMs;
  g_lastPulseMs = nowMs;

  Serial.println(F("arduino-opto encoder pulse capture started."));
  Serial.print(F("Encoder holes/rev assumption: "));
  Serial.println(kEncoderHolesPerRevolution);
  Serial.println(F("TODO: validate signal polarity and timeout on real hardware."));
}

void loop() {
  updateRpmFromPulseCount(millis());
}
