# ESP32-S3 physical regression pre-flight — historical snapshot

Status: **BLOCKED — HARDWARE REQUIRED**

This report records a pre-flight against source HEAD
`bd20bd9c1aacf918c4742375246175867c2c32f9`, captured before the later
repository changes. It is not a report for the current branch tip. At that
pre-flight, no firmware was flashed and no physical connection was made.

## Board detection

The board was not detected during that pre-flight. Port names and host device
identifiers are omitted because they are local-machine details. Port numbers
in older hardware milestone records are historical observations, not defaults
for another system.

Therefore transport testing, flashing, and peripheral testing were stopped
before hardware contact.

## Firmware resource map recorded at that pre-flight

| Resource | Module | Pins / instance | Configuration |
|---:|---|---|---|
| 1 | GPIO | output GPIO4 | write-only |
| 2 | GPIO | input GPIO5 | read-only |
| 3 | ADC | ESP32-S3 ADC resource | native maximum 4095 |
| 4 | PWM | LEDC output GPIO6 | duty maximum 10000 |
| 9 | SPI | SPI2; MOSI11, MISO12, SCLK13, CS10 | mode 0, 1 MHz default, 100 kHz–10 MHz, max 246 bytes |
| 10 | I2C | I2C0; SDA8, SCL9 | fixed 7-bit address `0x50`, 100 kHz |
| 5 | UART | UART1; TX17, RX18 | 115200 baud, 64-byte TX/RX limits |
| 6 | CAN/TWAI | controller 0; TX21, RX22 | classic CAN, 500 kbit/s |
| 7 | Pulse | input GPIO7 | 1 MHz timing, bounded capture |
| 8 | Encoder | A25, B26 | quadrature X4 |

The module IDs advertised by the current image remain `1,2,3,4,5,6,7,8,10`.

## Required hardware before continuing

- ESP32-S3 board connected through USB Serial/JTAG and visibly enumerated.
- Board-specific pinout confirmed; the source map above is the firmware map,
  not proof that every board pin is externally accessible.
- Common 3.3 V logic and common ground for all external equipment.
- For CAN: a 3.3 V-compatible CAN transceiver, CANH/CANL wiring, termination,
  and a second CAN node or analyzer. Never connect CANH/CANL to GPIO21/22.
- For I2C: a target at fixed address `0x50`, 3.3 V pull-ups on SDA/SCL as
  required by the target, and common ground.
- For SPI: a compatible SPI target or safe loopback setup that respects the
  configured CS10 ownership.
- For UART: a 3.3 V UART loopback between GPIO17 and GPIO18.
- For GPIO/ADC/PWM/Pulse/Encoder: safe loopback or signal-generation wiring
  based on the confirmed board pinout.

No physical test is classified as verified until the board is detected and
the exact wiring is recorded.

## Classification recorded at that pre-flight

- USB Serial/JTAG: BLOCKED — HARDWARE REQUIRED
- GPIO: BLOCKED — HARDWARE REQUIRED
- ADC: BLOCKED — HARDWARE REQUIRED
- PWM: BLOCKED — HARDWARE REQUIRED
- SPI: BLOCKED — HARDWARE REQUIRED
- I2C: BLOCKED — HARDWARE REQUIRED
- UART: BLOCKED — HARDWARE REQUIRED
- CAN/TWAI: BLOCKED — HARDWARE REQUIRED
- Pulse: BLOCKED — HARDWARE REQUIRED
- Encoder: BLOCKED — HARDWARE REQUIRED

DAC, OneWire, BlockDevice, RTC, and Watchdog remain generic-only,
unregistered, and unadvertised for this ESP32 image.
