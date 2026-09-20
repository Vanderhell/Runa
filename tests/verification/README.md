# Deterministic verification tiers

The verification campaign uses an explicit xorshift64* generator and reports
its seed, case count, assertion count, and failures. It does not use the C
library random-number implementation.

## FAST

Build and run the normal developer tests:

```text
cmake -S . -B build-fast -G Ninja -DJOB_BUILD_TESTS=ON
cmake --build build-fast --parallel
ctest --test-dir build-fast --output-on-failure
```

## STANDARD

Build all modules, native tests, fuzz smoke binaries, and host conformance as
used by the normal CI workflow:

```text
cmake -S . -B build-standard -G Ninja -DJOB_BUILD_TESTS=ON -DJOB_BUILD_FUZZ=ON \
  -DRUNA_MODULE_SPI=ON -DRUNA_MODULE_I2C=ON -DRUNA_MODULE_UART=ON \
  -DRUNA_MODULE_CAN=ON -DRUNA_MODULE_PULSE=ON -DRUNA_MODULE_DAC=ON \
  -DRUNA_MODULE_ENCODER=ON -DRUNA_MODULE_ONEWIRE=ON \
  -DRUNA_MODULE_BLOCK_DEVICE=ON -DRUNA_MODULE_RTC=ON -DRUNA_MODULE_WATCHDOG=ON
cmake --build build-standard --parallel
ctest --test-dir build-standard --output-on-failure
```

## EXHAUSTIVE

Enable the first-principles generated campaign. It requires all fourteen
modules and includes scalar/reference-model cases, malformed IR matrices,
registry/resource matrices, lifecycle failure injection, capability mutation
prefixes, and stateful BlockDevice model operations:

```text
cmake -S . -B build-exhaustive -G Ninja \
  -DRUNA_BUILD_EXHAUSTIVE_TESTS=ON -DJOB_BUILD_TESTS=ON \
  -DRUNA_MODULE_SPI=ON -DRUNA_MODULE_I2C=ON -DRUNA_MODULE_UART=ON \
  -DRUNA_MODULE_CAN=ON -DRUNA_MODULE_PULSE=ON -DRUNA_MODULE_DAC=ON \
  -DRUNA_MODULE_ENCODER=ON -DRUNA_MODULE_ONEWIRE=ON \
  -DRUNA_MODULE_BLOCK_DEVICE=ON -DRUNA_MODULE_RTC=ON -DRUNA_MODULE_WATCHDOG=ON
cmake --build build-exhaustive --parallel
ctest --test-dir build-exhaustive -R runa_exhaustive_verification --output-on-failure
```
