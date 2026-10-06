# HW Phase 1 Findings — ESP32-S3 — archived validation snapshot

Historical hardware validation recorded on 2026-09-16.

Date: 2026-09-16

## Detected hardware

- ESP32-S3 QFN56, revision 0.2
- 16 MB quad flash
- Flash JEDEC manufacturer/device: `20:4018`
- 8 MB embedded octal PSRAM (`AP_3v3`)
- 40 MHz crystal
- USB Serial/JTAG transport
- USB VID/PID: `303A:1001`
- Secure Boot disabled
- Flash encryption disabled

The exact board model, display controller, display wiring, and general-purpose pin mapping were not reliably identifiable. No display or physical GPIO assignment was guessed.

## Implemented

- Isolated ESP-IDF 5.5.1 target under `platform/esp32`
- Existing portable C runtime compiled unchanged for ESP32-S3
- USB Serial/JTAG request transport with bounded framing and resynchronization
- Existing ACK, EMIT, and RESULT frames used without an ESP32-specific IR
- GET_INFO and GET_CAPABILITIES responses
- ESP-IDF-backed GPIO, ADC, PWM, monotonic time, and delay HAL
- Fixed 2048-byte job input buffer
- Windows C# serial transport for COM devices
- Generic real-hardware test runner

The compiled resource table is intentionally empty because no physical GPIO, ADC, or PWM pin could be identified safely from available evidence.

## Physical and protocol validation

- Firmware built and flashed successfully
- Device booted and established host protocol communication
- GET_INFO confirmed 16 MB flash and 8 MB PSRAM
- GET_CAPABILITIES reported the frozen v1 opcode set and zero physical resources
- RETURN, arithmetic, comparison/branch, DELAY_MS, and EMIT executed successfully
- Invalid resource, opcode, register, jump, and IR version returned explicit errors
- Step and runtime limits returned explicit RESULT errors
- Truncated jobs returned explicit validation errors
- Malformed transport framing recovered without device reset
- Every accepted test job produced one ACK and exactly one final RESULT
- Validation rejections produced one explicit RESULT without partial execution

No GPIO, ADC, PWM, permission-path, or display tests were run because a safe
board pin mapping was unavailable. The ESP timer and delay HAL path was tested.

## Stability

- 1,035 hardware checks passed
- A 500-job repeated sequence passed
- No observed ESP32 reset
- No framing desynchronization
- No missing or duplicate RESULT
- No job-to-job register or state leakage
- Device remained responsive after malformed and invalid inputs

## Desktop regression

- MSVC Debug build and CTest passed
- MSVC Release build and CTest passed
- Strict GCC build and CTest passed
- C# tests passed with zero warnings
- Native/C# cross-language conformance passed
- 25 repeated native torture runs passed

## Firmware image and flash

- Application image size: 189,952 bytes (`0x2e600`)
- Application partition size: 1 MiB
- Application partition free space: 82%
- Image checksum valid
- Image SHA-256 validation hash valid
- Flash write completed with post-write hash verification

## Source revisions

- Software freeze tag: `software-core-freeze-v0.1`
- ESP32 platform commit: `4f03582`
- USB serial hardware validation commit: `04963d0`

## Status

ESP32-S3 HARDWARE PHASE 1: PASS
