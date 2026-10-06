# ESP32-S3 build report

**Validation date:** 2026-09-21

**Result:** Build passed; this validation did not include physical hardware.

## Toolchain

- ESP-IDF 5.5.1, target `esp32s3`
- Python 3.11.2, CMake 3.30.2, Ninja 1.12.1
- Xtensa ESP32-S3 GCC 14.2.0

## Build and memory results

ESP-IDF configure, compile, link, image generation, and partition checks
passed. The application image was 263,003 bytes; the smallest application
partition had 75% free space. Bootloader usage was 64%.

| Memory region | Usage |
|---|---:|
| Flash code | 142,636 bytes |
| DIRAM | 68,955 / 341,760 bytes |
| IRAM | 16,384 / 16,384 bytes |
| RTC slow memory | 32 / 8,192 bytes |
| RTC fast memory | 24 / 8,192 bytes |

IRAM had no reported free space: 1,028 bytes were vectors and 15,356 bytes
were IRAM text. DIRAM had 272,805 bytes free. The IDF TWAI header emitted an
external conversion warning; project-owned warnings were not reported.

## Firmware composition

The image registered GPIO, ADC, PWM, SPI, I2C, UART, CAN/TWAI, Pulse, and
Encoder (module IDs 1–8 and 10). DAC, OneWire, BlockDevice, RTC, and Watchdog
were not registered in this image. Their generic modules remain available to
other platform compositions.

SPI2 used GPIO 11/12/13 and CS 10. I2C0 used SDA 8, SCL 9, and address `0x50`.
CAN used classic TWAI with one-entry transmit and receive queues. SPI and I2C
resource mappings were compile-verified only. The
[hardware test guide](docs/hardware/ESP32S3_HARDWARE_TEST_GUIDE.md) contains
per-module examples and procedures for physical validation.

## Validation limits

No firmware was flashed as part of this build validation. Peripheral
electrical behavior and USB Serial/JTAG framing were not tested. ESP-IDF heap
usage was outside the scope of this measurement. The Runa execution path uses
bounded buffers and does not allocate from the heap.
