# HW Phase 3 Milestone — Instrumented Electrical Validation — archived snapshot

Historical validation milestone recorded on 2026-09-16. Its PASS status
applies to the specific board, firmware, and GPIO/ADC/PWM tests described
here; it is not a claim that later platform mappings have been physically
validated.

Date: 2026-09-16

## Target hardware

- ESP32-S3 QFN56 revision 0.2
- 16 MB flash
- 8 MB octal PSRAM
- Native USB Serial/JTAG
- ESP-IDF 5.5.1

## Wiring used

- GPIO4 to GPIO5 for digital GPIO loopback
- GPIO1 to GND for ADC LOW validation
- GPIO1 to 3V3 for ADC HIGH validation
- GPIO6 to GPIO5 for PWM boundary observation

All wiring changes were performed with USB power disconnected.

## Physical resource mapping

- Resource 1: GPIO4, GPIO WRITE
- Resource 2: GPIO5, GPIO READ
- Resource 3: GPIO1 / ADC1_CH0, ADC READ
- Resource 4: GPIO6 / LEDC channel 0, PWM WRITE

Logical resource IDs remain independent from physical GPIO numbers. The binary IR, opcode set, validator, executor, register model, protocol framing, and ACK/EMIT/RESULT semantics remain unchanged.

## GPIO electrical validation

- LOW written on GPIO4 was physically read as LOW on GPIO5.
- HIGH written on GPIO4 was physically read as HIGH on GPIO5.
- 5,000 alternating electrical transitions completed.
- 15,035 GPIO electrical checks passed.
- Zero loopback mismatches.
- Zero missing or duplicate RESULT frames.
- No unexpected timeout, MCU reset, framing failure, or transport desynchronization.

## ADC electrical validation

ADC configuration:

- ADC1_CH0 on GPIO1
- 12-bit raw resolution
- 12 dB attenuation
- Raw range 0 through 4095
- Calibration supported by the chip but not enabled in the runtime

Known LOW, GPIO1 connected to GND:

- 100 samples
- Minimum: 0
- Maximum: 0
- Mean: 0.00

Known HIGH, GPIO1 connected to 3V3:

- 100 samples
- Minimum: 4095
- Maximum: 4095
- Mean: 4095.00

The HIGH measurement proves electrical separation from GND and expected upper-range saturation. It is not a calibrated measurement of 3.3 V.

## PWM electrical validation

PWM configuration:

- GPIO6
- LEDC low-speed channel 0
- 5 kHz configured frequency
- 13-bit duty resolution
- Runtime normalized range 0 through 10000

GPIO6 was connected to GPIO5 for digital observation:

- 0% duty was consistently read as LOW.
- 100% duty was consistently read as HIGH.
- 1,000 alternating boundary transitions completed.
- 3,008 PWM electrical checks passed.
- Zero boundary mismatches.
- Normalized values 2500, 5000, and 7500 were accepted but their physical duty cycle was not instrumented.

## Combined generic job

A completely host-defined job executed through existing generic primitives:

- GPIO WRITE
- DELAY
- GPIO READ
- ADC READ
- PWM WRITE
- two EMIT operations
- PWM shutdown
- GPIO shutdown
- multi-value RETURN

The GPIO loopback value was correct, the ADC result remained within the raw range, both EMIT frames were received, exactly one RESULT was received, and GPIO/PWM outputs returned to safe states.

## Failure and recovery

- Validation failure reported the correct failing instruction and performed no partial execution.
- Runtime PWM OUT_OF_RANGE reported the correct failing instruction.
- Existing partial-execution semantics were physically confirmed: GPIO remained HIGH after the later runtime failure.
- A separate valid recovery job restored GPIO to LOW.
- Invalid resource, permission, and resource-type operations returned explicit errors.
- The MCU remained responsive after every failure.

## Electrical stress

- 5,000 GPIO electrical loopback transitions passed with zero mismatches.
- An additional 1,000 mixed jobs passed.
- Mixed jobs covered GPIO write/read, ADC, PWM boundary values, DELAY, EMIT, RETURN, arithmetic, branches, combined peripheral execution, and intentional errors.
- 2,123 combined and recovery checks passed.
- No missing or duplicate RESULT.
- No unexpected timeout.
- No MCU reset.
- No transport desynchronization.
- No observed register or job-state leakage.

## Firmware size

- Phase 3 firmware: 197,584 bytes
- Phase 2 baseline: 197,520 bytes
- Increase: 64 bytes
- Application partition free space: 81%

## Desktop regression

- MSVC Debug build and CTest passed.
- MSVC Release build and CTest passed.
- Strict GCC build and CTest passed.
- Native/C# cross-language conformance passed.
- C# host suite passed 9 checks.
- Native torture suite passed 20,125 checks with zero failures in each tested configuration.

## Remaining unverified electrical properties

- Exact PWM duty cycle at 25%, 50%, and 75%
- PWM frequency accuracy and waveform quality under external instrumentation
- ADC calibrated voltage accuracy and linearity

These remaining measurements do not block the existing v1 architecture or the Phase 3 electrical-validation objective.

## Git baseline

- `0265fab Freeze ESP32-S3 v1 hardware baseline`
- `79f9566 Add GPIO input resource for hardware loopback`
- `cfd4c0a Validate instrumented ESP32-S3 hardware paths`

## Status

ESP32-S3 INSTRUMENTED HW PHASE 3: PASS
