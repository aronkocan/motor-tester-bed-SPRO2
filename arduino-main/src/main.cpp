#include <Arduino.h>
#include <SoftwareSerial.h>
#include <Wire.h>
#include "../../shared/protocol/command_status.h"

namespace {
constexpr uint8_t kPinPowerButton = 2;
constexpr uint8_t kPinStartButton = 3;
constexpr uint8_t kPinStopButton = 4;
constexpr uint8_t kPinQuickKey1Button = 5;
constexpr uint8_t kPinQuickKey2Button = 6;
constexpr uint8_t kPinQuickKey3Button = 7;
constexpr uint8_t kPinEncoderButton = 8;
constexpr uint8_t kPinEncoderB = 9;
constexpr uint8_t kPinEncoderA = 10;

constexpr uint8_t kPinUsbHostTx = 12;
constexpr uint8_t kPinUsbHostRx = 13;

constexpr unsigned long kButtonDebounceMs = 40;
constexpr unsigned long kProgressUpdateIntervalMs = 250;
constexpr unsigned long kI2cPollIntervalMs = 100;
constexpr unsigned long kMeasurementTimeoutMs = 30000;
constexpr size_t kNextionBufferSize = 48;

enum class RuntimePhase : uint8_t {
  Setup = 0,
  Measurement = 1,
  Result = 2,
  Fault = 3,
};

struct RemoteStatus {
  bool online = false;
  bool fault = false;
  bool measurementDone = false;
  uint8_t progressPercent = 0;
  uint16_t rpm = 0;
  uint16_t milliAmps = 0;
  SharedProtocol::ErrorCode errorCode = SharedProtocol::ErrorCode::None;
};

struct SetupInputs {
  bool profileSelected = false;
  bool motorConnectedConfirmed = false;
  bool limitSet = false;
};

struct ButtonState {
  uint8_t pin = 0;
  bool stableState = LOW;
  bool lastSample = LOW;
  unsigned long lastTransitionMs = 0;
  bool pressedEvent = false;
};

RuntimePhase g_phase = RuntimePhase::Setup;
SharedProtocol::ErrorCode g_fault = SharedProtocol::ErrorCode::None;

SetupInputs g_setup;

RemoteStatus g_measurementStatus;
RemoteStatus g_optoStatus;

ButtonState g_powerButton{ kPinPowerButton };
ButtonState g_startButton{ kPinStartButton };
ButtonState g_stopButton{ kPinStopButton };
ButtonState g_quickKey1Button{ kPinQuickKey1Button };
ButtonState g_quickKey2Button{ kPinQuickKey2Button };
ButtonState g_quickKey3Button{ kPinQuickKey3Button };
ButtonState g_encoderButton{ kPinEncoderButton };

unsigned long g_lastProgressUpdateMs = 0;
unsigned long g_lastI2cPollMs = 0;
unsigned long g_measurementStartMs = 0;

char g_nextionBuffer[kNextionBufferSize];
size_t g_nextionLength = 0;
uint8_t g_nextionTerminatorCount = 0;

SoftwareSerial g_usbHostSerial(kPinUsbHostRx, kPinUsbHostTx);

Print& logStream() {
  return g_usbHostSerial;
}

void changePhase(RuntimePhase nextPhase);

const __FlashStringHelper* phaseToText(RuntimePhase phase) {
  switch (phase) {
    case RuntimePhase::Setup:
      return F("setup");
    case RuntimePhase::Measurement:
      return F("measurement");
    case RuntimePhase::Result:
      return F("result");
    case RuntimePhase::Fault:
      return F("fault");
  }

  return F("unknown");
}

void setFault(SharedProtocol::ErrorCode code, const __FlashStringHelper* message) {
  g_fault = code;
  changePhase(RuntimePhase::Fault);

  // Safety rule: stop measurement flow as soon as serious fault is identified.
  Wire.beginTransmission(SharedProtocol::kMeasurementI2cAddress);
  Wire.write(static_cast<uint8_t>(SharedProtocol::CommandCode::Stop));
  Wire.endTransmission();

  Wire.beginTransmission(SharedProtocol::kOptoI2cAddress);
  Wire.write(static_cast<uint8_t>(SharedProtocol::CommandCode::Stop));
  Wire.endTransmission();

  logStream().print(F("FAULT: "));
  logStream().println(message);

  // TODO: send explicit fault screen event to Nextion once command IDs are finalized.
}

void changePhase(RuntimePhase nextPhase) {
  if (g_phase == nextPhase) {
    return;
  }

  g_phase = nextPhase;
  logStream().print(F("Phase -> "));
  logStream().println(phaseToText(g_phase));

  // TODO: send phase-change events to Nextion with finalized command strings.
}

void initButton(ButtonState& button) {
  pinMode(button.pin, INPUT);
  button.stableState = digitalRead(button.pin);
  button.lastSample = button.stableState;
  button.lastTransitionMs = millis();
}

void updateButton(ButtonState& button, unsigned long nowMs) {
  const bool sample = digitalRead(button.pin);

  if (sample != button.lastSample) {
    button.lastSample = sample;
    button.lastTransitionMs = nowMs;
  }

  if ((nowMs - button.lastTransitionMs) >= kButtonDebounceMs &&
      button.stableState != button.lastSample) {
    button.stableState = button.lastSample;
    if (button.stableState == HIGH) {
      button.pressedEvent = true;
    }
  }
}

bool consumePressed(ButtonState& button) {
  if (!button.pressedEvent) {
    return false;
  }

  button.pressedEvent = false;
  return true;
}

void clearRemoteStatus(RemoteStatus& status) {
  status = RemoteStatus{};
}

void prepareFreshRemoteStatusesForRun(unsigned long nowMs) {
  clearRemoteStatus(g_measurementStatus);
  clearRemoteStatus(g_optoStatus);

  // Wait one normal poll interval before consuming remote status.
  // Slaves apply START in their loop(), while onRequest serves the last snapshot;
  // polling in the same iteration as START can read stale done/fault bits.
  g_lastI2cPollMs = nowMs;
}

bool setupInputsComplete() {
  return g_setup.profileSelected && g_setup.motorConnectedConfirmed && g_setup.limitSet;
}

void publishSetupValidationIfMissing() {
  if (setupInputsComplete()) {
    return;
  }

  logStream().println(F("Setup input missing: profile/motor confirmation/limit not complete."));
  // TODO: map to specific missing-field UI notification in Nextion.
}

bool sendCommandToBoard(uint8_t address, SharedProtocol::CommandCode command) {
  Wire.beginTransmission(address);
  Wire.write(static_cast<uint8_t>(command));
  return Wire.endTransmission() == 0;
}

bool pollBoardStatus(uint8_t address, RemoteStatus& status) {
  const uint8_t bytesRead = Wire.requestFrom(address, SharedProtocol::kStatusPayloadSize);
  if (bytesRead != SharedProtocol::kStatusPayloadSize) {
    status.online = false;
    return false;
  }

  uint8_t payload[SharedProtocol::kStatusPayloadSize];
  for (uint8_t i = 0; i < SharedProtocol::kStatusPayloadSize; ++i) {
    payload[i] = Wire.read();
  }

  status.online = true;
  const uint8_t flags = payload[SharedProtocol::kStatusIdxFlags];
  status.fault = SharedProtocol::hasFlag(flags, SharedProtocol::StatusFlags::Fault);
  status.measurementDone = SharedProtocol::hasFlag(flags, SharedProtocol::StatusFlags::MeasurementDone);
  status.progressPercent = payload[SharedProtocol::kStatusIdxProgressPercent];

  const uint8_t rpmLow = payload[SharedProtocol::kStatusIdxRpmLow];
  const uint8_t rpmHigh = payload[SharedProtocol::kStatusIdxRpmHigh];
  status.rpm = static_cast<uint16_t>(rpmHigh << 8) | rpmLow;

  status.milliAmps = static_cast<uint16_t>(payload[SharedProtocol::kStatusIdxCurrentDeciAmp]) * 10U;
  status.errorCode = static_cast<SharedProtocol::ErrorCode>(payload[SharedProtocol::kStatusIdxErrorCode]);

  return true;
}

void readNextionCommand() {
  auto dispatchBufferedNextionMessage = []() {
    if (g_nextionLength == 0) {
      return;
    }

    g_nextionBuffer[g_nextionLength] = '\0';

    if (strcmp(g_nextionBuffer, "START") == 0) {
      g_startButton.pressedEvent = true;
    } else if (strcmp(g_nextionBuffer, "STOP") == 0) {
      g_stopButton.pressedEvent = true;
    } else if (strcmp(g_nextionBuffer, "SETUP_PROFILE") == 0) {
      g_setup.profileSelected = true;
    } else if (strcmp(g_nextionBuffer, "SETUP_MOTOR_OK") == 0) {
      g_setup.motorConnectedConfirmed = true;
    } else if (strcmp(g_nextionBuffer, "SETUP_LIMIT_OK") == 0) {
      g_setup.limitSet = true;
    } else if (strcmp(g_nextionBuffer, "RESET_SETUP") == 0) {
      g_setup = SetupInputs{};
    } else if (strcmp(g_nextionBuffer, "FAULT_RESET") == 0) {
      g_fault = SharedProtocol::ErrorCode::None;
      changePhase(RuntimePhase::Setup);
    }

    g_nextionLength = 0;
  };

  while (Serial.available() > 0) {
    const uint8_t incoming = static_cast<uint8_t>(Serial.read());

    // Nextion command responses/events are terminated by 0xFF 0xFF 0xFF.
    // We primarily consume text commands here and dispatch when the terminator arrives.
    if (incoming == 0xFF) {
      ++g_nextionTerminatorCount;
      if (g_nextionTerminatorCount >= 3) {
        dispatchBufferedNextionMessage();
        g_nextionTerminatorCount = 0;
      }
      continue;
    }

    g_nextionTerminatorCount = 0;

    if (incoming == '\n' || incoming == '\r') {
      dispatchBufferedNextionMessage();
      continue;
    }

    if (g_nextionLength + 1 < kNextionBufferSize) {
      g_nextionBuffer[g_nextionLength++] = static_cast<char>(incoming);
    } else {
      g_nextionLength = 0;
      g_nextionTerminatorCount = 0;
    }
  }
}

void handleUnifiedStartStopLogic(unsigned long nowMs) {
  if (g_phase == RuntimePhase::Fault &&
      (consumePressed(g_stopButton) || consumePressed(g_powerButton))) {
    g_fault = SharedProtocol::ErrorCode::None;
    changePhase(RuntimePhase::Setup);
    return;
  }

  if (consumePressed(g_stopButton) || consumePressed(g_powerButton)) {
    sendCommandToBoard(SharedProtocol::kMeasurementI2cAddress, SharedProtocol::CommandCode::Stop);
    sendCommandToBoard(SharedProtocol::kOptoI2cAddress, SharedProtocol::CommandCode::Stop);

    if (g_phase != RuntimePhase::Setup) {
      changePhase(RuntimePhase::Setup);
    }
    return;
  }

  if (!consumePressed(g_startButton)) {
    return;
  }

  if (g_phase != RuntimePhase::Setup) {
    logStream().println(F("Start ignored: only valid in setup phase."));
    return;
  }

  if (!setupInputsComplete()) {
    publishSetupValidationIfMissing();
    setFault(SharedProtocol::ErrorCode::SetupMissingInput, F("Cannot start: setup data incomplete."));
    return;
  }

  const bool measurementAccepted = sendCommandToBoard(SharedProtocol::kMeasurementI2cAddress,
                                                       SharedProtocol::CommandCode::Start);
  const bool optoAccepted = sendCommandToBoard(SharedProtocol::kOptoI2cAddress,
                                                SharedProtocol::CommandCode::Start);
  if (!measurementAccepted || !optoAccepted) {
    setFault(SharedProtocol::ErrorCode::RemoteBoardTimeout,
             F("Could not start all remote boards."));
    return;
  }

  prepareFreshRemoteStatusesForRun(nowMs);
  g_measurementStartMs = nowMs;
  changePhase(RuntimePhase::Measurement);
}

void handleSetupShortcuts() {
  if (consumePressed(g_quickKey1Button)) {
    g_setup.profileSelected = !g_setup.profileSelected;
  }

  if (consumePressed(g_quickKey2Button)) {
    g_setup.motorConnectedConfirmed = !g_setup.motorConnectedConfirmed;
  }

  if (consumePressed(g_quickKey3Button)) {
    g_setup.limitSet = !g_setup.limitSet;
  }

  if (consumePressed(g_encoderButton)) {
    // TODO: map encoder button to Nextion field focus/selection once UI pages are fixed.
    logStream().println(F("Encoder button pressed (reserved for future UI navigation)."));
  }
}

void pollRemoteBoardsIfDue(unsigned long nowMs) {
  if ((nowMs - g_lastI2cPollMs) < kI2cPollIntervalMs) {
    return;
  }

  pollBoardStatus(SharedProtocol::kMeasurementI2cAddress, g_measurementStatus);
  pollBoardStatus(SharedProtocol::kOptoI2cAddress, g_optoStatus);

  g_lastI2cPollMs = nowMs;
}

void processMeasurementPhase(unsigned long nowMs) {
  if (g_phase != RuntimePhase::Measurement) {
    return;
  }

  if (!g_measurementStatus.online || !g_optoStatus.online) {
    if ((nowMs - g_measurementStartMs) > kMeasurementTimeoutMs) {
      setFault(SharedProtocol::ErrorCode::RemoteBoardTimeout,
               F("Remote board timeout during measurement."));
    }
    return;
  }

  if (g_measurementStatus.errorCode != SharedProtocol::ErrorCode::None) {
    setFault(g_measurementStatus.errorCode, F("Measurement board error code reported."));
    return;
  }

  if (g_optoStatus.errorCode != SharedProtocol::ErrorCode::None) {
    setFault(g_optoStatus.errorCode, F("Opto board error code reported."));
    return;
  }

  if (g_measurementStatus.fault || g_optoStatus.fault) {
    setFault(SharedProtocol::ErrorCode::AbnormalMotorCondition,
             F("Remote board reported abnormal motor condition."));
    return;
  }

  if (g_measurementStatus.measurementDone) {
    changePhase(RuntimePhase::Result);
  }
}

void publishProgressIfDue(unsigned long nowMs) {
  if ((nowMs - g_lastProgressUpdateMs) < kProgressUpdateIntervalMs) {
    return;
  }

  g_lastProgressUpdateMs = nowMs;

  if (g_phase == RuntimePhase::Measurement) {
    logStream().print(F("Progress: "));
    logStream().print(g_measurementStatus.progressPercent);
    logStream().print(F("% | RPM: "));
    logStream().print(g_optoStatus.rpm);
    logStream().print(F(" | mA: "));
    logStream().println(g_measurementStatus.milliAmps);
  }

  if (g_phase == RuntimePhase::Result) {
    logStream().print(F("Result ready. RPM="));
    logStream().print(g_optoStatus.rpm);
    logStream().print(F(", current[mA]="));
    logStream().println(g_measurementStatus.milliAmps);
  }
}

void updateButtons(unsigned long nowMs) {
  updateButton(g_powerButton, nowMs);
  updateButton(g_startButton, nowMs);
  updateButton(g_stopButton, nowMs);
  updateButton(g_quickKey1Button, nowMs);
  updateButton(g_quickKey2Button, nowMs);
  updateButton(g_quickKey3Button, nowMs);
  updateButton(g_encoderButton, nowMs);
}
}  // namespace

void setup() {
  Serial.begin(115200);  // Reserved for Nextion USART transport.
  g_usbHostSerial.begin(115200);

  Wire.begin();

  initButton(g_powerButton);
  initButton(g_startButton);
  initButton(g_stopButton);
  initButton(g_quickKey1Button);
  initButton(g_quickKey2Button);
  initButton(g_quickKey3Button);
  initButton(g_encoderButton);

  pinMode(kPinEncoderA, INPUT);
  pinMode(kPinEncoderB, INPUT);

  logStream().println(F("arduino-main coordinator started."));
  logStream().println(F("Runtime is provisional until hardware validation."));
  logStream().println(F("Shared command/status protocol enabled (v1)."));
}

void loop() {
  const unsigned long nowMs = millis();

  readNextionCommand();
  updateButtons(nowMs);
  handleSetupShortcuts();
  handleUnifiedStartStopLogic(nowMs);

  pollRemoteBoardsIfDue(nowMs);
  processMeasurementPhase(nowMs);
  publishProgressIfDue(nowMs);
}
