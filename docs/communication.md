# Communication Contract

This document defines the **single shared command/status contract** for all three Arduino boards.

Source of truth in code: `shared/protocol/command_status.h`.

## Bus topology
- `arduino-main` is the I2C controller/master.
- `arduino-measurement` is an I2C target at `0x12`.
- `arduino-opto` is an I2C target at `0x13`.
- `0x11` is reserved as the logical ID for `arduino-main` in shared definitions.

## Command bytes (`arduino-main` -> target boards)
Single-byte command frame:

| Byte | Field | Values |
|---|---|---|
| 0 | `command` | `1=Start`, `2=Stop`, `3=Ping` |

Notes:
- Unknown command values should set target `fault=true` and `errorCode=UnknownCommand (13)`.
- `Ping` is liveness-only and should not change runtime state.

## Status payload (`target boards` -> `arduino-main`)
Fixed 6-byte frame (`kStatusPayloadSize=6`):

| Byte | Field | Type | Meaning |
|---|---|---|---|
| 0 | `flags` | `uint8_t` | Bit0=`Fault`, Bit1=`MeasurementDone` |
| 1 | `progressPercent` | `uint8_t` | 0..100 runtime progress |
| 2 | `rpmLow` | `uint8_t` | RPM LSB |
| 3 | `rpmHigh` | `uint8_t` | RPM MSB |
| 4 | `currentDeciAmp` | `uint8_t` | Current in 0.1 A units |
| 5 | `errorCode` | `uint8_t` | Shared `ErrorCode` enum value |

Derived values:
- `rpm = (rpmHigh << 8) | rpmLow`
- `milliAmps = currentDeciAmp * 10`

Board ownership:
- `arduino-measurement` owns current estimation and reports `currentDeciAmp` from motor current estimate.
- `arduino-opto` does not own current sensing and currently reports `currentDeciAmp=0` placeholder.

## Shared error codes

| Code | Name | Typical source/meaning |
|---:|---|---|
| 0 | `None` | No error |
| 1 | `SetupMissingInput` | Setup incomplete before start |
| 2 | `RemoteBoardTimeout` | Main timed out waiting for target |
| 3 | `AbnormalMotorCondition` | Generic abnormal condition |
| 10 | `EstimatedOverCurrent` | Measurement board overcurrent estimate |
| 11 | `OptoNoPulseTimeout` | Opto running but no pulse observed in timeout window |
| 12 | `InvalidRpmEstimate` | Opto RPM computation invalid |
| 13 | `UnknownCommand` | Target received unsupported command byte |
| 255 | `Unknown` | Fallback/unspecified |

## Alignment policy
- Every board includes and uses `shared/protocol/command_status.h`.
- Do not redefine command bytes, addresses, payload indexing, or error-code values locally.
- If protocol changes, update both the shared header and this document in the same change.

## Hardware-validation status
Runtime behavior is still provisional until validated on real hardware.
- TODO(hardware): confirm no-pulse timeout threshold and whether it should always be a fault condition.
- TODO(hardware): validate current scaling and overcurrent thresholds against real INA226-backed readings.
