# Architecture

Runa separates deterministic job execution from peripheral-specific operations
and platform hardware access.

## Core and modules

The C11 Core decodes and validates the binary IR, executes scalar operations,
enforces resource and execution limits, dispatches registered modules, and
reports bounded events and results. Core does not allocate from the heap or
recurse. It contains no peripheral-specific execution logic.

Modules are registered at build time through `runa_module_t`; Runa has no
dynamic plugin mechanism. The current module ABI is version 1. Module
descriptors define validation and execution callbacks, optional lifecycle and
capability callbacks, and context ownership.

| ID | Module | ID | Module |
|---:|---|---:|---|
| 1 | GPIO | 8 | Pulse |
| 2 | ADC | 9 | DAC |
| 3 | PWM | 10 | Encoder |
| 4 | SPI | 11 | OneWire |
| 5 | I2C | 12 | BlockDevice |
| 6 | UART | 13 | RTC |
| 7 | CAN | 14 | Watchdog |

Module IDs are part of the binary interface and must not be reused or
renumbered. V2 module operations use `EXT` with a little-endian module ID,
operation, and bounded module payload. V1 framing, opcodes, and compatibility
behavior remain separate from V2 dispatch.

## Capabilities and execution bounds

Capability format version 2 has a 32-byte header and describes the modules and
resources registered in a firmware composition. Unknown capability record
types may be skipped.

Default Core limits include 2,048 bytes per job, 256 instructions, 10,000
execution steps, a five-second execution budget, bounded delays, resources,
events, result data, and module transfers. Platform modules may define
additional operation-specific limits.

Structural validation precedes execution. Successfully started modules are
stopped in reverse start order. Physical side effects already completed are
not rolled back after a later runtime error. Accepted jobs produce at most one
ACK and one terminal RESULT while the event sink remains available; validation
failures produce a RESULT without an ACK.

## Platforms and compatibility

Platform adapters provide hardware access, transport, and the modules and
resources available in a firmware image. Capability reporting reflects that
compiled composition. Each platform adapter may support a subset of the generic
module set.

The module ABI and capability format are versioned. Future versions may
introduce breaking changes. Hardware timing, electrical characteristics,
resource mappings, and watchdog reset behavior depend on the selected platform
and target hardware.
