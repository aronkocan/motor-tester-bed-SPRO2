#ifndef SHARED_PROTOCOL_COMMAND_STATUS_H
#define SHARED_PROTOCOL_COMMAND_STATUS_H

#include <Arduino.h>

namespace SharedProtocol {

constexpr uint8_t kMainI2cAddress = 0x11;
constexpr uint8_t kMeasurementI2cAddress = 0x12;
constexpr uint8_t kOptoI2cAddress = 0x13;

enum class CommandCode : uint8_t {
  Start = 1,
  Stop = 2,
  Ping = 3,
};

enum class ErrorCode : uint8_t {
  None = 0,
  SetupMissingInput = 1,
  RemoteBoardTimeout = 2,
  AbnormalMotorCondition = 3,
  EstimatedOverCurrent = 10,
  OptoNoPulseTimeout = 11,
  InvalidRpmEstimate = 12,
  UnknownCommand = 13,
  Unknown = 255,
};

enum class StatusFlags : uint8_t {
  Fault = 1 << 0,
  MeasurementDone = 1 << 1,
};

constexpr uint8_t kStatusPayloadSize = 6;
constexpr uint8_t kStatusIdxFlags = 0;
constexpr uint8_t kStatusIdxProgressPercent = 1;
constexpr uint8_t kStatusIdxRpmLow = 2;
constexpr uint8_t kStatusIdxRpmHigh = 3;
constexpr uint8_t kStatusIdxCurrentDeciAmp = 4;
constexpr uint8_t kStatusIdxErrorCode = 5;

inline bool hasFlag(uint8_t flags, StatusFlags flag) {
  return (flags & static_cast<uint8_t>(flag)) != 0;
}

inline uint8_t makeFlags(bool fault, bool measurementDone) {
  uint8_t flags = 0;
  if (fault) {
    flags |= static_cast<uint8_t>(StatusFlags::Fault);
  }
  if (measurementDone) {
    flags |= static_cast<uint8_t>(StatusFlags::MeasurementDone);
  }
  return flags;
}

}  // namespace SharedProtocol

#endif  // SHARED_PROTOCOL_COMMAND_STATUS_H
