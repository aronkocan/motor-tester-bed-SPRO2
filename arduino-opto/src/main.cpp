#include <Arduino.h>
#include <Wire.h>
#include "../../shared/protocol/command_status.h"

namespace {
constexpr uint8_t kOptoPulsePin = 4;
constexpr uint8_t kEncoderHolesPerRevolution = 20;
constexpr unsigned long kRpmUpdateIntervalMs = 250;
constexpr unsigned long kNoPulseTimeoutMs = 1000;

enum class OptoState : uint8_t {
  Idle = 0,
  Running = 1,
  Fault = 2,
};

struct OptoStatus {
  bool fault = false;
  bool measurementDone = false;
  uint8_t progressPercent = 0;
  uint16_t rpm = 0;
  uint16_t milliAmps = 0;
  SharedProtocol::ErrorCode errorCode = SharedProtocol::ErrorCode::None;
};

volatile uint32_t g_pulseCount = 0;
volatile uint8_t g_lastPinState = LOW;
volatile unsigned long g_lastPulseMs = 0;

volatile SharedProtocol::CommandCode g_pendingCommand = SharedProtocol::CommandCode::Ping;
volatile bool g_hasPendingCommand = false;

OptoState g_state = OptoState::Idle;
OptoStatus g_status;
OptoStatus g_statusSnapshot;

uint32_t g_lastPulseSnapshot = 0;
unsigned long g_lastRpmUpdateMs = 0;
unsigned long g_measurementStartMs = 0;
float g_currentRpm = 0.0f;

void publishStatusSnapshot() {
  noInterrupts();
  g_statusSnapshot = g_status;
  interrupts();
}

void setupPulseCapture() {
  pinMode(kOptoPulsePin, INPUT);

  g_lastPinState = digitalRead(kOptoPulsePin);

  // Enable pin-change interrupts for port D (PCINT[23:16]).
  PCICR |= _BV(PCIE2);
  // D4 maps to PD4 / PCINT20.
  PCMSK2 |= _BV(PCINT20);
}

void clearRuntimeState(unsigned long nowMs) {
  noInterrupts();
  g_pulseCount = 0;
  g_lastPulseMs = nowMs;
  interrupts();

  g_lastPulseSnapshot = 0;
  g_lastRpmUpdateMs = nowMs;
  g_currentRpm = 0.0f;

  g_status.rpm = 0;
  g_status.progressPercent = 0;
  g_status.measurementDone = false;
  publishStatusSnapshot();
}

void startMeasurement(unsigned long nowMs) {
  g_state = OptoState::Running;
  g_status.fault = false;
  g_measurementStartMs = nowMs;
  clearRuntimeState(nowMs);

  Serial.println(F("Opto: measurement tracking started."));
}

void stopMeasurement(const __FlashStringHelper* reason) {
  g_state = OptoState::Idle;
  clearRuntimeState(millis());

  Serial.print(F("Opto: measurement tracking stopped ("));
  Serial.print(reason);
  Serial.println(F(")."));
}

void setFault(const __FlashStringHelper* reason) {
  g_state = OptoState::Fault;
  g_status.fault = true;

  Serial.print(F("Opto fault: "));
  Serial.println(reason);
}

void updateProgress(unsigned long nowMs) {
  if (g_state != OptoState::Running) {
    g_status.progressPercent = 0;
    return;
  }

  // TODO(hardware): use authoritative timing/progress from arduino-main or
  // arduino-measurement once cross-board synchronization is finalized.
  const unsigned long elapsedMs = nowMs - g_measurementStartMs;
  if (elapsedMs >= 12000UL) {
    g_status.progressPercent = 100;
    g_status.measurementDone = true;
  } else {
    g_status.progressPercent = static_cast<uint8_t>((elapsedMs * 100UL) / 12000UL);
    g_status.measurementDone = false;
  }
}

void updateRpmFromPulseCount(unsigned long nowMs) {
  if (g_state != OptoState::Running) {
    g_status.rpm = 0;
    return;
  }

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
    g_status.errorCode = SharedProtocol::ErrorCode::OptoNoPulseTimeout;
    g_currentRpm = 0.0f;
  } else if (pulsesInWindow > 0) {
    if (g_status.errorCode == SharedProtocol::ErrorCode::OptoNoPulseTimeout) {
      g_status.errorCode = SharedProtocol::ErrorCode::None;
    }

    const float revolutions = static_cast<float>(pulsesInWindow) /
                              static_cast<float>(kEncoderHolesPerRevolution);
    g_currentRpm = (revolutions * 60000.0f) / static_cast<float>(elapsedMs);
  }

  g_lastRpmUpdateMs = nowMs;

  if (g_currentRpm < 0.0f) {
    g_status.errorCode = SharedProtocol::ErrorCode::InvalidRpmEstimate;
    setFault(F("invalid negative RPM estimate"));
    g_status.rpm = 0;
    return;
  }

  if (g_currentRpm > 65535.0f) {
    g_status.rpm = 65535;
  } else {
    g_status.rpm = static_cast<uint16_t>(g_currentRpm);
  }

  Serial.print(F("Pulse count: "));
  Serial.print(pulseSnapshot);
  Serial.print(F(" | RPM: "));
  Serial.println(g_currentRpm, 2);
}

void onReceiveCommand(int byteCount) {
  if (byteCount <= 0) {
    return;
  }

  const uint8_t rawCommand = Wire.read();
  while (Wire.available() > 0) {
    Wire.read();
  }

  switch (rawCommand) {
    case static_cast<uint8_t>(SharedProtocol::CommandCode::Start):
      g_pendingCommand = SharedProtocol::CommandCode::Start;
      g_hasPendingCommand = true;
      break;
    case static_cast<uint8_t>(SharedProtocol::CommandCode::Stop):
      g_pendingCommand = SharedProtocol::CommandCode::Stop;
      g_hasPendingCommand = true;
      break;
    case static_cast<uint8_t>(SharedProtocol::CommandCode::Ping):
      g_pendingCommand = SharedProtocol::CommandCode::Ping;
      g_hasPendingCommand = true;
      break;
    default:
      g_status.fault = true;
      g_status.errorCode = SharedProtocol::ErrorCode::UnknownCommand;
      break;
  }
}

void onRequestStatus() {
  const OptoStatus status = g_statusSnapshot;
  const uint8_t flags = SharedProtocol::makeFlags(status.fault, status.measurementDone);
  const uint8_t rpmLow = static_cast<uint8_t>(status.rpm & 0x00FFU);
  const uint8_t rpmHigh = static_cast<uint8_t>((status.rpm >> 8) & 0x00FFU);

  // Opto board does not own motor-current sensing; keep byte 0 for compatibility.
  constexpr uint8_t kCurrentDeciAmpPlaceholder = 0;

  Wire.write(flags);
  Wire.write(status.progressPercent);
  Wire.write(rpmLow);
  Wire.write(rpmHigh);
  Wire.write(kCurrentDeciAmpPlaceholder);
  Wire.write(static_cast<uint8_t>(status.errorCode));
}

void handlePendingCommand(unsigned long nowMs) {
  if (!g_hasPendingCommand) {
    return;
  }

  g_hasPendingCommand = false;

  if (g_pendingCommand == SharedProtocol::CommandCode::Start) {
    if (g_state == OptoState::Running) {
      return;
    }

    startMeasurement(nowMs);
    g_status.errorCode = SharedProtocol::ErrorCode::None;
    return;
  }

  if (g_pendingCommand == SharedProtocol::CommandCode::Stop) {
    stopMeasurement(F("remote command"));
    g_status.fault = false;
    g_status.errorCode = SharedProtocol::ErrorCode::None;
    return;
  }

  if (g_pendingCommand == SharedProtocol::CommandCode::Ping) {
    // No state changes. Ping is used as liveness check.
  }
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
  clearRuntimeState(nowMs);

  Wire.begin(SharedProtocol::kOptoI2cAddress);
  Wire.onReceive(onReceiveCommand);
  Wire.onRequest(onRequestStatus);

  Serial.println(F("arduino-opto started."));
  Serial.println(F("Shared I2C status payload enabled (6 bytes)."));
  Serial.print(F("Encoder holes/rev assumption: "));
  Serial.println(kEncoderHolesPerRevolution);
  Serial.println(F("TODO: validate signal polarity and pulse conditioning on real hardware."));
}

void loop() {
  const unsigned long nowMs = millis();

  handlePendingCommand(nowMs);
  updateRpmFromPulseCount(nowMs);
  updateProgress(nowMs);
  publishStatusSnapshot();
}
