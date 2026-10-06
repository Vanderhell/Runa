# ESP32 build verification — historical validation snapshot

Recorded in commit `53a3f074cff01e8434322394a28b32182751f1d3` (2026-09-21).
This records a successful ESP-IDF build and compile-only platform checks; it
does not claim that physical hardware was tested by this verification.

Status: ESP-IDF build verified; physical hardware and USB transport testing not performed.

## Environment

- Target: `esp32s3`
- ESP-IDF: v5.5.1
- Python: 3.11.2
- CMake: 3.30.2
- Ninja: 1.12.1
- Xtensa ESP32-S3 GCC: 14.2.0 (`esp-14.2.0_20241119`)

The build was configured from a fresh isolated ESP-IDF build directory with an
explicit `esp32s3` target. No ESP-IDF installation or system-wide environment
change was performed.

## Result

`idf.py build` passed configure, compile, link, image generation, and partition
size checks. The application image is `0x403d0` bytes, with `0xbfc30` bytes
(75%) free in the smallest application partition. The bootloader is `0x5180`
bytes, with 36% free.

The final `idf.py size` report showed:

- total image size: 263003 bytes
- Flash Code: 142636 bytes
- DIRAM: 68955 / 341760 bytes (20.18%)
- IRAM: 16384 / 16384 bytes (100%)
- RTC SLOW: 32 / 8192 bytes
- RTC FAST: 24 / 8192 bytes

The full compiler output had no remaining project-owned warnings. The strict
project warning policy remains enabled; the IDF TWAI header's conversion
warning is isolated at the include boundary because it is external code.

The official JSON size report decomposes IRAM as 1028 bytes of vectors and
15356 bytes of IRAM text in a fixed 16384-byte IRAM region, leaving zero bytes
reported free in that region. DIRAM remains distinct and has 272805 bytes
free. This is a real tight IRAM condition, not a percentage-formatting issue;
no test-only firmware code was added to consume it.

ASan/UBSan: NOT PERFORMED for the final documented software matrix. Existing
sanitizer build directories contain configuration/object artifacts, but no
complete sanitizer test execution was established by this verification.

## Registered ESP32 composition

The firmware registers fixed, resource-owned instances for GPIO, ADC, PWM, SPI,
I2C, UART, CAN/TWAI, Pulse, and Encoder. The advertised V2 module IDs are:

`1, 2, 3, 4, 5, 6, 7, 8, 10`

These correspond to GPIO, ADC, PWM, SPI, I2C, UART, CAN, Pulse, and Encoder.
DAC (9), OneWire (11), BlockDevice (12), RTC (13), and Watchdog (14) are not
registered or advertised. Their generic modules remain available to other
platform compositions.

The application uses fixed mappings: SPI2 with GPIO 11/12/13 and CS 10, and
I2C0 with SDA 8, SCL 9, and a fixed 7-bit device resource address `0x50`.
These mappings are compile-verified only and are not hardware-verified.

CAN uses classic TWAI with a configured transmit queue of 1 and receive queue
of 1. UART and USB Serial/JTAG use fixed-size driver buffers. No Runa heap
allocation, background streaming task, or unbounded Runa queue was added.

## Fixes made during this verification

- Added the ESP-IDF `driver` component dependency required by the supported
  TWAI v2 header.
- Replaced the removed `rmt_rx_wait_all_done` call with a bounded completion
  wait using the existing Pulse timeout and FreeRTOS delay.
- Added explicit CAN predicate grouping and localized the external IDF header
  conversion diagnostic; global warning strictness was not weakened.
- Registered the already-implemented SPI and I2C adapters with fixed platform
  resources so capability reporting matches usable firmware composition.

No Core, generic module, EXT dispatch, module ID, V1 wire, or architecture
freeze contract changed. Architecture freeze impact: none.

## Verification limits

- No firmware was flashed.
- No GPIO, ADC, PWM, SPI, I2C, UART, CAN/TWAI, Pulse, or Encoder electrical
  behavior was tested.
- USB Serial/JTAG framing is compile-verified only.
- ESP-IDF internal allocation is not audited; the Runa execution path has no
  explicit heap dependency.
