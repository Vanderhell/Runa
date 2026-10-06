# ESP32-S3 hardware test guide

This guide describes repeatable physical checks for the current ESP32-S3
firmware composition. It is a test plan, not a claim that the listed checks
have passed. Run each test against the exact board, firmware build, and wiring
being recorded. Start by querying device information and Capabilities V2; use
resource IDs and configuration reported by that firmware rather than assuming
IDs from another build.

Capabilities V2 reports registered modules and their operation capabilities;
it does not report resource IDs, resource permissions, or physical pin
mappings. For this firmware, construct the host `ResourceDescriptor` values
from the matching firmware composition and module configuration, and confirm
they match the tested image before execution.

Resource map in the current `platform/esp32/main/esp32_main.c` composition:

| Resource ID | Module | Permissions | Current mapping |
|---:|---|---|---|
| 1 | GPIO | Write | GPIO4 output |
| 2 | GPIO | Read | GPIO5 input |
| 3 | ADC | Read | ADC1 channel 0, GPIO1 |
| 4 | PWM | Write | GPIO6 |
| 5 | UART | Read, write | UART1, TX17/RX18, 115200 8-N-1 |
| 6 | CAN/TWAI | Read, write | Controller 0, TX21/RX22, 500 kbit/s |
| 7 | Pulse | Read | GPIO7 |
| 8 | Encoder | Read, write | A=GPIO25, B=GPIO26, X4 |
| 9 | SPI | Read, write | SPI2, MOSI11/MISO12/SCLK13, CS10 |
| 10 | I2C | Read, write | I2C0, SDA8/SCL9, address `0x50`, 100 kHz |

The table describes this source composition. Check the firmware revision and
board-specific pin availability before constructing matching host descriptors.

The current image registers GPIO, ADC, PWM, SPI, I2C, UART, CAN/TWAI, Pulse,
and Encoder. DAC, OneWire, BlockDevice, RTC, and Watchdog have generic modules
and .NET builders, but no adapter or registered resource in this image. Their
examples below are for a future compatible platform composition and cannot be
executed on the current ESP32-S3 firmware.

## Common setup

Required: ESP32-S3 board running the Runa image, .NET 9 SDK, a Windows serial
port name for the board when using `WindowsSerialHardwareTransport`, and the
external test equipment listed per test. Use a current copy of the firmware
and host packages built from the same checkout.

```csharp
using McuRemote;
using Runa.Transport.Serial;

await using var transport = new WindowsSerialHardwareTransport("COMx");
var runner = new HardwareTestRunner(transport);
byte[] info = await runner.GetInfoAsync();
DeviceCapabilities capabilities = await runner.GetCapabilitiesV2Async();
```

Replace `COMx` with the port assigned by the operating system. Confirm the
returned device identity and capabilities before selecting each test's
resource. Build each job with `new JobBuilder(jobId: 1)`, the typed extension
shown below, and `builder.Build()`. Execute it with
`await runner.ExecuteAsync(job)` and inspect its single RESULT and any module
data frames. A transport response or accepted job alone does not prove an
electrical test passed.

Disconnect external wiring before changing connections. Use 3.3 V logic only;
never connect a signal above the board's rated input voltage. Share ground
between the board and external test equipment. Verify pin availability for
the exact board before connecting anything.

## USB Serial/JTAG transport

**Example:** query device information and Capabilities V2 with the common
setup code above, then execute a core-only job:

```csharp
var builder = new JobBuilder(jobId: 1);
builder.Return(builder.Load(42));
byte[] job = builder.Build();
ExecutionTrace trace = await runner.ExecuteAsync(job);
```

**Procedure:** connect the board using its USB Serial/JTAG connector; identify
the enumerated serial port; open it with `WindowsSerialHardwareTransport`;
call `GetInfoAsync` and `GetCapabilitiesV2Async`; confirm each query returns a
valid frame and that the advertised modules match the firmware composition.
Then send the harmless job above and confirm one successful RESULT containing
42. Record board, firmware
revision, host OS, port, and outcome. USB transport is the job channel, not a
peripheral module; this check does not validate USB throughput or electrical
compliance.

## GPIO

**Example:** `builder.GpioWrite(output, builder.Load(1));` followed by
`Register observed = builder.GpioRead(input); builder.Return(observed);`

**Wiring:** connect the configured output (current image: GPIO4) to configured
input (GPIO5), with common ground.

**Procedure:** select GPIO write resource 1 and read resource 2 from the
matching firmware composition.
Build and execute separate jobs writing 0 and 1, reading the input after each
write. Expect the corresponding level each time. Repeat at least 100
transitions and confirm no missed or inverted readings. Do not attach another
output to the loopback net.

## ADC

**Example:** `Register sample = builder.AdcRead(adc); builder.Return(sample);`

**Wiring:** current adapter configures ADC1 channel 0 (GPIO1 on ESP32-S3).
Connect the input to ground for the low check, then to 3.3 V for the high
check; stay within the board's ADC input limits.

**Procedure:** execute 100 reads with the input grounded and record min/max;
repeat at 3.3 V. Values should be near the corresponding ends of the reported
raw range (configured maximum 4095), allowing for board noise and ADC
non-linearity. This is a raw code check, not calibrated voltage measurement.

## PWM

**Example:** `builder.PwmWrite(pwm, builder.Load(5000));` for a 50% normalized
duty request (range 0–10000).

**Wiring:** current adapter drives GPIO6. Connect GPIO6 to an oscilloscope or
logic analyzer. For a separate digital loopback check, connect it to the GPIO
input on GPIO5 only while GPIO4 is disconnected from GPIO5.

**Procedure:** request duty 0, 5000, and 10000. Confirm low, approximately
half-period, and high output respectively. Record measured frequency and duty
for the middle setting. The digital loopback only checks boundary levels;
use an instrument to establish PWM waveform and timing.

## SPI

**Example:** `builder.SpiTransfer(spi, new byte[] { 0x9F }, 3, timeoutMs: 20);`

**Wiring:** current image uses SPI2: MOSI GPIO11, MISO GPIO12, SCLK GPIO13,
automatic CS GPIO10. Connect a known SPI target or a suitable logic analyzer;
set the target for mode 0 and the advertised clock (currently 1 MHz). Ensure
all signal levels are compatible.

**Procedure:** first capture a transfer and verify clock, mode, CS, MOSI bytes,
and returned byte count. Then use a documented, non-destructive command for
the selected target (for example, its documented identification command) and
compare response bytes with that target's datasheet. `0x9F` above is only a
common command example; execute it only if the connected device documents it.
Without a target-specific response oracle, the logic capture verifies bus
activity but not functional SPI communication.

## I2C

**Example:** `builder.I2cTransfer(i2c, new byte[] { registerAddress }, 1, timeoutMs: 20);`

**Wiring:** current image uses SDA GPIO8, SCL GPIO9, 100 kHz, and fixed
7-bit target address `0x50`. Connect a compatible I2C target and appropriate
pull-ups to 3.3 V; do not add pull-ups if the board/target already provides
them without checking the resulting resistance.

**Procedure:** confirm the target address and a safe read-only register from
its datasheet. Select the I2C resource, send that register address and read
one byte as above, and compare the returned value to the target's documented
value. Confirm with a logic analyzer that address, ACK, register, repeated
start/read, and STOP are correct. A missing target should produce a NACK/error,
not a fabricated successful read.

## UART

**Example:** `builder.UartTransfer(uart, new byte[] { 0x52, 0x55, 0x4E, 0x41 }, 4, UartReceivePolicy.FixedLength, 100_000);`

**Wiring:** current UART1 uses TX GPIO17 and RX GPIO18, 115200 baud, 8-N-1,
no flow control. For loopback, connect TX to RX and common ground; do not
connect the USB Serial/JTAG port to these pins.

**Procedure:** transmit the four-byte sequence shown and request four bytes
back. Confirm exact byte equality, then repeat with different values and
several lengths. If using a second UART device instead, cross TX/RX and match
baud/framing. A local loopback tests the UART path, not interoperability with
every external device.

## CAN/TWAI

**Example:** `builder.CanTransmit(can, new CanFrame(0x123, false, new byte[] { 0x52, 0x4E }), 100_000);`

**Wiring:** current classic TWAI configuration uses TX GPIO21, RX GPIO22,
500 kbit/s. Attach a CAN transceiver and a second active CAN node; MCU pins
must not connect directly to the differential bus. Provide correct bus
termination at the ends of the bus and common reference ground as required by
the transceivers.

**Procedure:** configure the peer for classic CAN at 500 kbit/s. Receive on
the peer, transmit the standard frame above, and confirm ID `0x123`, two data
bytes, and no transmit error. Then transmit a known frame from the peer and
use `builder.CanReceive(can, 100_000);` to confirm the reported frame matches.
Run with two properly terminated nodes; a lone transmitter does not establish
successful CAN acknowledgement.

## Pulse capture

**Example:** `Register period = builder.MeasurePulsePeriod(pulse, PulseEdge.Rising, 1_000_000); builder.Return(period);`

**Wiring:** current capture input is GPIO7. Drive it from a signal generator
or a second MCU using 0–3.3 V square wave and common ground. Begin at 1 kHz,
50% duty cycle.

**Procedure:** measure period with a timeout above the expected period. Expect
approximately 1000 microseconds at 1 kHz. Repeat at 100 Hz and compare
measured periods with the instrument. For width measurement use
`builder.MeasurePulseWidth(pulse, PulseLevel.High, 1_000_000);` and compare
against the configured high time. Record frequency, duty, timeout and observed
values; input conditioning and timing accuracy depend on the board and signal.

## Encoder

**Example:** `builder.EncoderReset(encoder); Register position = builder.EncoderRead(encoder); builder.Return(position);`

**Wiring:** current quadrature inputs are A=GPIO25 and B=GPIO26, X4 decoding.
Connect a quadrature encoder with compatible 3.3 V outputs and common ground,
or a quadrature signal generator. Do not assume mechanical encoder outputs
include suitable pull-ups or debouncing.

**Procedure:** reset the count and read the initial position. Rotate a known
number of encoder detents or supply a known number of valid quadrature cycles;
read again and compare count with the encoder's pulses-per-revolution and X4
decoding specification. Reverse direction and confirm the signed count moves
in the opposite direction. Use `EncoderReadReset` when the test needs an
atomic read-and-clear operation.

## Generic modules without ESP32-S3 adapters

The following code fragments use the corresponding .NET extension and a
resource descriptor supplied by a future platform composition. They are not
executable against the current ESP32-S3 image. Confirm that the platform
provides the resource configuration and permissions required by each
operation.

### DAC

**Example:** `var config = (DacResourceConfiguration)dac.Configuration!;`
`builder.DacWrite(dac, 0u); builder.DacWrite(dac, config.MaximumValue);`

**Procedure:** with a compatible DAC adapter and a high-impedance voltmeter,
measure output at minimum and maximum codes and compare with the adapter's
documented scale/reference. Never assume voltage range or pin mapping from the
generic module alone. Current ESP32-S3 image: no DAC adapter/resource.

### OneWire

**Example:** `Register present = builder.OneWireReset(oneWireBus, 100_000);`

**Procedure:** attach a known 1-Wire device using the adapter's specified
pull-up and power arrangement; verify reset/presence, search, and then perform
a device-specific documented read and CRC check. Do not use broadcast SKIP
ROM writes unless the bus configuration explicitly permits them and every
attached device is intended to receive the command. Current image: no
OneWire adapter/resource.

### BlockDevice

**Example:** `var config = (BlockDeviceResourceConfiguration)blockDevice.Configuration!;`
`builder.BlockRead(blockDevice, offset: 0, length: config.ReadAlignment);`

**Procedure:** use a disposable test medium and a reserved test region. Read a
known baseline, write a reversible test pattern, read and compare it, sync if
supported, and restore the original contents. Test erase only on an explicitly
disposable region aligned to the advertised erase size. Never run write/erase
tests against an unbacked-up production medium. Current image: no BlockDevice
adapter/resource.

### RTC

**Example:** `builder.RtcRead(rtc);` followed by a read of the module data
response using `RtcModuleDataDecoder.DecodeRead(frame)`.

**Procedure:** compare the returned UTC time with a trusted reference before
and after a measured wait. Test `RtcSet` only on a designated test device and
only when its resource advertises set permission; record and restore its
previous time if needed. Current image: no RTC adapter/resource.

### Watchdog

**Example:** `var config = (WatchdogResourceConfiguration)watchdog.Configuration!;`
`builder.WatchdogGetStatus(watchdog);`
`builder.WatchdogArm(watchdog, config.MinimumTimeoutMs);`

**Procedure:** use a dedicated test target with a documented recovery path.
Read status, arm a supported timeout, and run a controlled test application
that deliberately stops feeding the watchdog. Confirm reset cause/status after
reboot, then verify normal feed behavior. Expect the deliberate timeout to
reset or interrupt the test target; do not run on a device controlling unsafe
loads. Current image: no Watchdog adapter/resource.

## Test status and records

The following pre-flight classification records which checks still require a
physical run against the current image:

| Check | Pre-flight classification |
|---|---|
| USB Serial/JTAG | BLOCKED — HARDWARE REQUIRED |
| GPIO | BLOCKED — HARDWARE REQUIRED |
| ADC | BLOCKED — HARDWARE REQUIRED |
| PWM | BLOCKED — HARDWARE REQUIRED |
| SPI | BLOCKED — HARDWARE REQUIRED |
| I2C | BLOCKED — HARDWARE REQUIRED |
| UART | BLOCKED — HARDWARE REQUIRED |
| CAN/TWAI | BLOCKED — HARDWARE REQUIRED |
| Pulse | BLOCKED — HARDWARE REQUIRED |
| Encoder | BLOCKED — HARDWARE REQUIRED |

These are pre-flight classifications, not results from the procedures above.
The archived Phase 3 GPIO, ADC, and PWM PASS applies to its recorded older
board/firmware configuration. No PASS for the current composition is implied
by this guide or by a successful native build. After physical tests, record
the date, board revision, firmware revision, wiring/test equipment, procedure,
result, and deviations. Keep generic native module tests distinct from
physical adapter validation.
