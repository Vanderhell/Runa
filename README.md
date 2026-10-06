# Runa

Runa is a minimal-footprint, deterministic MCU execution runtime. It lets a
host define bounded hardware jobs and send them to a microcontroller as a
compact binary intermediate representation (IR). The MCU executes those jobs
without requiring application-specific firmware for each host workflow.

Runa keeps hardware access in a small, reviewable firmware runtime while the
host defines and changes workflows.

## Architecture

- **Core** decodes and validates the IR, enforces execution and resource
  limits, dispatches registered modules, and reports bounded events/results.
  It does not allocate from the heap or recurse.
- **Modules** implement peripheral-specific operations behind a versioned
  module interface. The current generic module set includes GPIO, ADC, PWM,
  SPI, I2C, UART, CAN, Pulse capture, DAC, Encoder, OneWire, BlockDevice, RTC,
  and Watchdog.
- **Platform** supplies the hardware abstraction layer (HAL), transport, and
  the set of modules/resources composed into a firmware image. ESP32-S3 is the
  current platform implementation. Some generic modules do not have an
  ESP32-S3 hardware adapter.
- **Host** provides .NET job-building, module-extension, transport, and
  response-decoding packages. The host can inspect device capabilities and
  build jobs from the modules exposed by the target.

See [Architecture](ARCHITECTURE.md) for the module IDs, execution bounds, and
runtime semantics.

The current Core limits include 2,048 bytes per job, 256 decoded
instructions, 10,000 execution steps, a five-second execution budget, and
bounded delays, resources, events, and results. Platform drivers can impose
additional operation-specific limits. Physical side effects already completed
are not rolled back after a later runtime error.

## Modules and platform status

All fourteen modules listed above have generic C implementations and .NET
builder packages. The default native CMake composition enables GPIO, ADC, and
PWM; the CI configuration builds and tests all modules. The ESP32-S3 firmware
composition registers GPIO, ADC, PWM, SPI, I2C, UART, CAN/TWAI, Pulse, and
Encoder. Its SPI and I2C mappings are compile-verified; the historical Phase 3
hardware validation exercised GPIO, ADC, and PWM on its documented board
configuration. The [ESP32 build report](ESP32_BUILD_VERIFICATION.md) and
[archived hardware test reports](docs/archive/hardware/README.md) describe
their validation scope. Hardware validation applies to the configurations
listed in those reports.

## Build and test

Requirements:

- CMake 3.20 or newer and a C11 compiler (GCC, Clang, or MSVC).
- .NET SDK 9 for host builds and cross-language conformance tests.
- Ninja is optional; omit `-G Ninja` to use the platform's default generator.
- ESP-IDF 5.5.1 is needed only to build the ESP32-S3 firmware.

Configure, build, and run the default native test set:

```sh
cmake -S . -B build -DJOB_BUILD_TESTS=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

To build all generic modules and their available tests, enable the optional
modules. The same configuration is used by the repository's strict compiler
CI jobs:

```sh
cmake -S . -B build-all -DJOB_BUILD_TESTS=ON -DJOB_BUILD_FUZZ=ON \
  -DRUNA_MODULE_SPI=ON -DRUNA_MODULE_I2C=ON -DRUNA_MODULE_UART=ON \
  -DRUNA_MODULE_CAN=ON -DRUNA_MODULE_PULSE=ON -DRUNA_MODULE_DAC=ON \
  -DRUNA_MODULE_ENCODER=ON -DRUNA_MODULE_ONEWIRE=ON \
  -DRUNA_MODULE_BLOCK_DEVICE=ON -DRUNA_MODULE_RTC=ON \
  -DRUNA_MODULE_WATCHDOG=ON
cmake --build build-all --parallel
ctest --test-dir build-all --output-on-failure
dotnet run --project host/dotnet/McuRemote.Tests/McuRemote.Tests.csproj \
  --configuration Release
```

Build an individual .NET package with, for example:

```sh
dotnet build host/dotnet/Runa.Core/Runa.Core.csproj --configuration Release
```

## Example

The host builds a job with the typed builder and module extensions, then a
platform transport sends the resulting bytes to a device:

```csharp
using McuRemote;

var builder = new JobBuilder(jobId: 1);
Register value = builder.Load(42);
builder.Return(value);
byte[] job = builder.Build();
// Send `job` through the selected platform transport and inspect its result.
```

The target must have a compatible module and resource registered. Consult the
host package APIs and device capabilities for the operations supported by a
specific firmware composition.

## Maturity and limitations

Runa is an early, actively validated project at version 0.1.0. Native C tests,
cross-language conformance tests, compiler-specific CI, fuzz smoke targets,
and ESP32-S3 build and hardware validation records are available.
The current module ABI is version 1 and may evolve through explicit version
changes. Hardware results apply to the recorded board, firmware, mappings, and
tests; they do not establish electrical timing, calibrated analog accuracy,
or support for every generic module/platform combination.
See [CHANGELOG.md](CHANGELOG.md) for the initial project status.

## Compatibility naming

Runa is the current project name. Legacy C `Job`/`job_` identifiers and the
`.NET` `McuRemote` namespace and project names remain in the host API for
compatibility.

## License

No `LICENSE` file is present. The repository's distribution terms are
unspecified.
