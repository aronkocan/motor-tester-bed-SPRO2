#include <Arduino.h>
#include <Wire.h>

namespace {
constexpr uint8_t kMeasurementI2cAddress = 0x12;

constexpr uint8_t kPinPwmEnB = 6;
constexpr uint8_t kPinMotorVoltageSense = A0;

constexpr unsigned long kSensorUpdateIntervalMs = 50;
constexpr unsigned long kProgressUpdateIntervalMs = 100;
constexpr unsigned long kMeasurementDurationMs = 12000;
constexpr unsigned long kStartSettleMs = 250;

constexpr uint16_t kPwmDutyMin = 40;
constexpr uint16_t kPwmDutyMax = 220;

// TODO(hardware): validate ADC-to-voltage conversion against real RC bridge scaling.
constexpr float kAdcReferenceVolts = 5.0f;
constexpr float kAdcMaxCount = 1023.0f;
constexpr float kRcBridgeScale = 4.0f;

// TODO(hardware): replace with INA226 readings once both sensors are connected/validated.
constexpr uint16_t kEstimatedCurrentFaultThresholdMilliAmps = 2000;
constexpr float kEstimatedCurrentScaleMilliAmpsPerVolt = 250.0f;

enum class CommandCode : uint8_t {
  Start = 1,
  Stop = 2,
  Ping = 3,
};

enum class MeasurementState : uint8_t {
  Idle = 0,
  Running = 1,
  Completed = 2,
  Fault = 3,
};

struct MeasurementStatus {
  bool fault = false;
  bool measurementDone = false;
  uint8_t progressPercent = 0;
  uint16_t rpm = 0;
  uint16_t milliAmps = 0;
};

MeasurementState g_state = MeasurementState::Idle;
MeasurementStatus g_status;

volatile CommandCode g_pendingCommand = CommandCode::Ping;
volatile bool g_hasPendingCommand = false;

unsigned long g_measurementStartedMs = 0;
unsigned long g_lastSensorUpdateMs = 0;
unsigned long g_lastProgressUpdateMs = 0;

float g_motorVoltage = 0.0f;
uint8_t g_pwmDuty = 0;

float adcToMotorVoltage(uint16_t rawAdc) {
  const float sensedVolts = (static_cast<float>(rawAdc) * kAdcReferenceVolts) / kAdcMaxCount;
  return sensedVolts * kRcBridgeScale;
}

uint16_t estimateCurrentMilliAmps(float motorVoltage) {
  const float estimate = motorVoltage * kEstimatedCurrentScaleMilliAmpsPerVolt;
  if (estimate <= 0.0f) {
    return 0;
  }

  if (estimate > 65535.0f) {
    return 65535;
  }

  return static_cast<uint16_t>(estimate);
}

void applyMotorPwm(uint8_t duty) {
  g_pwmDuty = duty;
  analogWrite(kPinPwmEnB, duty);
}

void stopMotorDrive() {
  applyMotorPwm(0);
}

void clearStatus() {
  g_status = MeasurementStatus{};
}

void startMeasurement(unsigned long nowMs) {
  clearStatus();
  g_state = MeasurementState::Running;
  g_measurementStartedMs = nowMs;
  g_lastSensorUpdateMs = nowMs;
  g_lastProgressUpdateMs = nowMs;

  applyMotorPwm(kPwmDutyMin);

  Serial.println(F("Measurement: started."));
}

void stopMeasurement(const __FlashStringHelper* reason) {
  stopMotorDrive();

  if (g_state == MeasurementState::Running) {
    g_state = MeasurementState::Idle;
  }

  g_status.measurementDone = false;
  g_status.progressPercent = 0;

  Serial.print(F("Measurement: stopped ("));
  Serial.print(reason);
  Serial.println(F(")."));
}

void setFault(const __FlashStringHelper* reason) {
  stopMotorDrive();
  g_state = MeasurementState::Fault;
  g_status.fault = true;
  g_status.measurementDone = false;

  Serial.print(F("Measurement fault: "));
  Serial.println(reason);
}

void completeMeasurement() {
  stopMotorDrive();
  g_state = MeasurementState::Completed;
  g_status.measurementDone = true;
  g_status.progressPercent = 100;

  Serial.println(F("Measurement: completed."));
}

void processMeasurement(unsigned long nowMs) {
  if (g_state != MeasurementState::Running) {
    return;
  }

  const unsigned long elapsedMs = nowMs - g_measurementStartedMs;

  if ((nowMs - g_lastSensorUpdateMs) >= kSensorUpdateIntervalMs) {
    g_lastSensorUpdateMs = nowMs;

    const uint16_t rawAdc = analogRead(kPinMotorVoltageSense);
    g_motorVoltage = adcToMotorVoltage(rawAdc);
    g_status.milliAmps = estimateCurrentMilliAmps(g_motorVoltage);

    if (g_status.milliAmps >= kEstimatedCurrentFaultThresholdMilliAmps) {
      setFault(F("estimated overcurrent"));
      return;
    }
  }

  if ((nowMs - g_lastProgressUpdateMs) >= kProgressUpdateIntervalMs) {
    g_lastProgressUpdateMs = nowMs;

    if (elapsedMs <= kStartSettleMs) {
      g_status.progressPercent = 1;
      applyMotorPwm(kPwmDutyMin);
    } else if (elapsedMs >= kMeasurementDurationMs) {
      completeMeasurement();
      return;
    } else {
      const unsigned long activeDurationMs = kMeasurementDurationMs - kStartSettleMs;
      const unsigned long activeElapsedMs = elapsedMs - kStartSettleMs;
      const float ratio = static_cast<float>(activeElapsedMs) /
                          static_cast<float>(activeDurationMs);

      const uint8_t progress = static_cast<uint8_t>(ratio * 99.0f);
      g_status.progressPercent = (progress < 99U) ? progress : 99U;

      const uint8_t duty = static_cast<uint8_t>(kPwmDutyMin +
                                                ratio * static_cast<float>(kPwmDutyMax -
                                                                           kPwmDutyMin));
      applyMotorPwm(duty);
    }
  }
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
    case static_cast<uint8_t>(CommandCode::Start):
      g_pendingCommand = CommandCode::Start;
      g_hasPendingCommand = true;
      break;
    case static_cast<uint8_t>(CommandCode::Stop):
      g_pendingCommand = CommandCode::Stop;
      g_hasPendingCommand = true;
      break;
    case static_cast<uint8_t>(CommandCode::Ping):
      g_pendingCommand = CommandCode::Ping;
      g_hasPendingCommand = true;
      break;
    default:
      break;
  }
}

void onRequestStatus() {
  const uint8_t faultByte = g_status.fault ? 1U : 0U;
  const uint8_t doneByte = g_status.measurementDone ? 1U : 0U;

  const uint8_t rpmLow = static_cast<uint8_t>(g_status.rpm & 0x00FFU);
  const uint8_t rpmHigh = static_cast<uint8_t>((g_status.rpm >> 8) & 0x00FFU);

  const uint16_t deciAmp = g_status.milliAmps / 10U;
  const uint8_t currentDeciAmp = (deciAmp > 255U) ? 255U : static_cast<uint8_t>(deciAmp);

  Wire.write(faultByte);
  Wire.write(doneByte);
  Wire.write(g_status.progressPercent);
  Wire.write(rpmLow);
  Wire.write(rpmHigh);
  Wire.write(currentDeciAmp);
}

void handlePendingCommand(unsigned long nowMs) {
  if (!g_hasPendingCommand) {
    return;
  }

  g_hasPendingCommand = false;

  if (g_pendingCommand == CommandCode::Start) {
    if (g_state == MeasurementState::Running) {
      return;
    }

    g_status.fault = false;
    startMeasurement(nowMs);
    return;
  }

  if (g_pendingCommand == CommandCode::Stop) {
    stopMeasurement(F("remote command"));
    g_state = MeasurementState::Idle;
    g_status.fault = false;
    return;
  }

  if (g_pendingCommand == CommandCode::Ping) {
    // Intentionally no state changes. Ping is used as liveness check.
  }
}
}  // namespace

void setup() {
  Serial.begin(115200);

  pinMode(kPinPwmEnB, OUTPUT);
  pinMode(kPinMotorVoltageSense, INPUT);
  stopMotorDrive();

  Wire.begin(kMeasurementI2cAddress);
  Wire.onReceive(onReceiveCommand);
  Wire.onRequest(onRequestStatus);

  clearStatus();

  Serial.println(F("arduino-measurement started."));
  Serial.println(F("I2C status payload v0 enabled (6 bytes)."));
  Serial.println(F("TODO: use INA226_1 (0x40) and INA226_2 (0x45) after hardware bring-up."));
}

void loop() {
  const unsigned long nowMs = millis();

  handlePendingCommand(nowMs);
  processMeasurement(nowMs);
}
