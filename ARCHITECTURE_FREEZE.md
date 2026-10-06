# Runa software architecture freeze — historical milestone snapshot

This document records the software audit freeze as it stood at the time of
its validation. Its deferred-work list is historical, not the current project
status. Subsequent ESP32 build evidence is recorded in
`ESP32_BUILD_VERIFICATION.md`; physical results and their limits are recorded
in the `HW_PHASE_*_MILESTONE.md` files.

Status at this snapshot: software audit freeze for that milestone. This
document records the contract verified by the native and host test matrix; it
is not a promise of permanent ABI compatibility.

## Frozen

- Core remains peripheral-agnostic.  It owns framing-independent IR decoding,
  scalar execution, validation, limits, resource lookup, generic registry
  dispatch, lifecycle, capabilities, events, monotonic time, and delay.
- V2 module execution uses `EXT` with a little-endian module ID, operation, and
  module-owned bounded payload.  Core does not switch on peripheral semantics.
- Modules are registered at compile/composition time through `runa_module_t`.
  There is no dynamic plugin mechanism.
- Module IDs are fixed and must not be reused or renumbered:

  | ID | Module |
  |---:|---|
  | 1 | GPIO |
  | 2 | ADC |
  | 3 | PWM |
  | 4 | SPI |
  | 5 | I2C |
  | 6 | UART |
  | 7 | CAN |
  | 8 | Pulse |
  | 9 | DAC |
  | 10 | Encoder |
  | 11 | OneWire |
  | 12 | BlockDevice |
  | 13 | RTC |
  | 14 | Watchdog |

- The module ABI is version 1 for this milestone.  A descriptor has explicit
  ABI/module versions, validation and execution callbacks, optional lifecycle
  callbacks, optional capability data, and explicit context ownership.
- Capability format version 2 and its 32-byte header are frozen.  Capabilities
  describe the registry actually composed into the image; unknown record types
  remain skippable.
- Execution is bounded by the advertised limits: 2048-byte jobs, 256
  instructions, 10,000 steps, 5 seconds, bounded delay, resources, result,
  event count/bytes, module-data frames, and module transfer sizes.
- Core execution does not allocate from the heap, recurse, feed a watchdog, or
  roll back physical side effects.  Structural validation precedes execution;
  already-completed physical actions remain completed after a later failure.
- Module lifecycle begins in registry order and ends in reverse order.  A
  failed begin is not ended; successfully begun modules are ended with the
  execution status.
- V1 framing/opcodes and compatibility behavior remain frozen separately from
  V2 modular dispatch.

## Stable but extensible

- New modules may be added with new IDs and host packages without adding
  peripheral semantics to Core.
- Module-owned capability payloads, resource configuration, HAL adapters, and
  module operations may grow under explicit module/version changes while the
  generic registry and dispatch contract remains intact.
- Platform adapters may expose only the modules physically implemented by that
  platform.  Capability output must match that compiled/registered set.

## Not frozen

- Permanent ABI compatibility beyond the current ABI version.
- The exact set of platform adapters and physical resource mappings.
- Hardware timing, electrical characteristics, and reset/reporting behavior.

## Deferred at that milestone

- ESP-IDF compilation and linker verification in the target environment.
- Physical verification of every module and platform mapping.  Software mocks,
  native tests, and host conformance are not hardware evidence.
- Watchdog reset-before-response behavior on real hardware.

## Side-effect and terminal semantics

Accepted jobs emit at most one ACK and one terminal RESULT when the event sink
remains available.  Validation failures emit a RESULT without ACK.  Runtime
failures preserve prior physical effects, end active modules deterministically,
and report the first execution/cleanup error.  A physical watchdog reset may
interrupt reporting and is an intentional hardware exception.
