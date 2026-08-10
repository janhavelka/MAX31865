# Arduino integration

Arduino types are confined to `MAX31865/ArduinoBackend.h`, its guarded source,
and examples. The application initializes and owns `SPIClass`; the library
never calls `SPI.begin()` or selects board pins.

```cpp
MAX31865Pins pins{12, 13, 11, 10, MAX31865_PIN_UNUSED};
SPI.begin(pins.sck, pins.miso, pins.mosi, MAX31865_PIN_UNUSED);

MAX31865ArduinoBackend backend;
const SPISettings settings(1000000U, MSBFIRST, SPI_MODE1);
MAX31865Status status =
    max31865ArduinoBackendBind(backend, SPI, settings, 1000000U, pins);
if (status.ok()) {
  status = max31865ArduinoConfigureControlPins(backend);
}

MAX31865BeginConfig config = max31865DefaultBeginConfig();
config.transport = max31865ArduinoTransport(backend);
config.initialDeviceConfig.wireMode = MAX31865WireMode::FourWire;
config.initialDeviceConfig.filter = MAX31865Filter::Hz60;

MAX31865 rtd;
if (status.ok()) {
  status = rtd.begin(config);
}
```

Binding validates and stores nonowning SPI/settings/pin state without I/O. The
explicit clock argument must match the value used to construct `SPISettings`;
the adapter rejects zero or values above 5 MHz. Arduino keeps bit order and mode
opaque, so MSB-first and mode 1/3 remain an application-visible contract.
SCK, MISO, MOSI, and active-low CS are required and distinct; DRDY may be
`MAX31865_PIN_UNUSED`. A successful bind or rebind clears `pinsConfigured`.
`max31865ArduinoConfigureControlPins()` preloads CS high before enabling its
output and configures DRDY as a plain input. It deliberately does not touch
SCK/MISO/MOSI or clean up a prior binding.

The backend is stable borrowed state after its transport is passed to driver
`begin()`. Do not bind, rebind, reconfigure, or directly mutate it until every
driver using that backend has called `end()` or has been destroyed. Transport
capabilities are copied when the table is made, while its callbacks continue to
dereference the backend; a live rebind would otherwise mix stale capabilities
with new pins, settings, or arbiter state.

Use MSB-first SPI mode 1 or mode 3 at no more than the data-sheet 5 MHz limit.
Examples use mode 1 at 1 MHz. Hardware-controlled/automatic CS must be disabled:
the core owns each CS edge because one-shot and fault commands are committed
when CS rises.

Do not pass a transport to driver `begin()` before control-pin configuration
succeeds. If that precondition is violated, call `end()`, configure the backend,
create a fresh transport table, and begin again. `recover()` cannot repair an
adapter that was never configured.

## Shared SPI

`MAX31865ArduinoBusArbiter` is optional. Its callback table is copied, but its
`user` object is borrowed. `lock(user, remainingMs)` must make one finite attempt
and return `Ok` only after ownership; the backend calls `unlock` exactly once
after a successful lock. Every device client on the same SPI host must use the
same arbiter.

With no arbiter, the backend is valid only for a dedicated bus or one proven
bus-calling context. `SPI.beginTransaction()` scopes device settings but does
not provide cross-task serialization.

Arduino SPI cannot report a wire-level error or cancel an in-flight transfer.
The configured clock and nine-byte maximum frame bound ordinary wire time; the
driver still enforces deadlines at callback boundaries. Physical MISO
stuck-high/low behavior may therefore appear as register mismatch rather than
`SpiTransferFailed`.

## Example wiring

`examples/common/BoardConfig.h` contains example-only pins, physical RTD wire
mode, 50/60 Hz notch choice, measured RREF, RTD R0, and fitted filter RC. Review
all values for the actual board. It is not installed configuration and must not
become library policy.

The first five examples demonstrate focused lifecycle, continuous, one-shot,
fault, and RTD-configuration workflows. They create no task and use no dynamic
`String`; scheduling and failure policy remain visibly application-owned.
