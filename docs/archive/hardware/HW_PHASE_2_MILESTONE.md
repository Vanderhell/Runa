# HW Phase 2 Milestone — Pocket-Dongle-S3-0.96 — archived snapshot

Historical validation milestone recorded on 2026-09-16. Its PASS status
applies to the specific board, firmware, and tests described here; it is not a
claim that later platform mappings have been physically validated.

Date: 2026-09-16

## Software and firmware baseline

The portable software core and ESP32-S3 v1 runtime at this milestone used the
following baseline:

- Binary IR and execution semantics remain unchanged.
- Validator, executor, protocol, C# SDK, and generic hardware runner remain the shared implementation.
- ESP32 support remains an isolated platform adapter around the portable core.
- No I2C, SPI, UART, display, networking, or other feature expansion is included.

## Verified hardware execution

- Pocket-Dongle-S3-0.96 with ESP32-S3 QFN56 revision 0.2
- 16 MB flash and 8 MB octal PSRAM
- USB Serial/JTAG transport
- Real ESP32 HAL paths active for GPIO, ADC, PWM, monotonic time, and delay
- Logical GPIO output resource backed by GPIO1
- Logical ADC resource backed by GPIO2 / ADC1_CH1
- Logical PWM resource backed by GPIO4 / LEDC channel 0
- Generic cross-peripheral job executed through GPIO, PWM, delay, ADC, EMIT, and RETURN primitives

## Protocol and stability evidence

- 1,000 mixed hardware jobs completed without MCU reset or transport desynchronization.
- 2,115 hardware checks passed.
- Every accepted job produced one ACK and exactly one terminal RESULT.
- No missing or duplicate RESULT was observed.
- No register leakage or job-to-job state leakage was observed.
- Invalid resource, permission, type, range, opcode, register, jump, version, malformed-job, step-limit, and runtime-limit paths returned explicit errors on real hardware.
- Malformed framing recovered without reflashing or resetting the device.

## Footprint and regression

- Firmware image size: 197,520 bytes.
- Previous baseline: 189,952 bytes.
- Increase: 7,568 bytes, approximately 7.6 kB.
- The execution core does not depend on PSRAM.
- MSVC Debug and Release builds and CTest passed.
- Strict GCC build and CTest passed.
- Native/C# cross-language conformance passed.
- Native torture suite passed 20,125 checks with zero failures per tested configuration.
- C# host suite passed all checks.

## Measurements not included at this milestone

The following electrical measurements were not part of Phase 2. Phase 3
records subsequent GPIO, ADC, and PWM electrical checks.

- GPIO1 HIGH/LOW voltage was not measured or looped back.
- GPIO4 PWM waveform, frequency, and duty cycle were not measured.
- GPIO2 ADC was tested only as a floating input; accuracy at known voltages was not established.
- The integrated ST7735S display and its output were not visually confirmed by this firmware.

## Source revisions

- ESP32 physical-resource implementation: `cfc161f`
- Physical execution validation: `d9395d8`

## Status

POCKET-DONGLE-S3 HW PHASE 2: PASS

PORTABLE CORE AND ESP32-S3 V1 BASELINE: FROZEN
