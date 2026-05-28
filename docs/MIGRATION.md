# MAX31865 Migration Notes

## Unreleased Transport/Header Split

This stage makes `MAX31865BeginConfig` framework-neutral. It no longer contains
an Arduino SPI pointer.

Arduino callers that used this pattern:

```cpp
MAX31865BeginConfig cfg{};
cfg.spi = &SPI;
cfg.pins = {sck, miso, mosi, cs, drdy};
rtd.begin(cfg);
```

should migrate to:

```cpp
MAX31865BeginConfig cfg{};
cfg.pins = {sck, miso, mosi, cs, drdy};
rtd.begin(SPI, cfg);
```

Compact Arduino callers may keep using:

```cpp
rtd.begin(SPI, sck, miso, mosi, cs, drdy, spiHz);
```

Framework-neutral and ESP-IDF callers should use direct `rtd.begin(cfg)` with
`cfg.transport.transfer`, `cfg.transport.nowMs`, `cfg.transport.delayMs`, and
`cfg.transport.delayUs` populated. `cfg.transport.lock` and
`cfg.transport.unlock` are optional but must be supplied as a pair.

The core/source path no longer creates a FreeRTOS mutex. If your transport
shares an SPI bus and needs serialization, provide bounded `lock`/`unlock`
callbacks in `MAX31865TransportConfig`. Callback failures keep their
`MAX31865Status::msg` and `detail` in `lastOperationStatus()`.

Use `#include "MAX31865/Core.h"` for the framework-neutral driver entry point.
It exposes `MAX31865Core`, an alias over the same transport-backed driver
implementation used by `MAX31865`. This avoids duplicate protocol state while
giving non-Arduino consumers a clean include path. Use
`#include "MAX31865/MAX31865.h"` for the compatibility driver class and guarded
Arduino overloads.

## Unreleased Source Split Step

No public API signatures changed in this step. The guarded Arduino compatibility
overloads and fallback SPI/GPIO/timing implementation moved to
`src/MAX31865Arduino.cpp`; `src/MAX31865.cpp` now remains the transport-backed
protocol/register/RTD/fault/health implementation.

Builds that define `MAX31865_HAS_ARDUINO_BACKEND=0` continue to compile the
core path without Arduino headers or runtime calls. Arduino sketches may keep
using `rtd.begin(SPI, cfg)` or the compact positional overload.

`MAX31865.h` no longer includes `<Arduino.h>` or `<SPI.h>` transitively. Normal
Arduino sketches receive those declarations from the framework prelude; other
translation units that enable `MAX31865_HAS_ARDUINO_BACKEND` and use `SPIClass`
directly should include `<SPI.h>` themselves.

Generated Doxygen HTML is local ignored output under `docs/generated/`; it
should be regenerated with `doxygen Doxyfile` after source or public-doc edits.

## Unreleased Pure-Core Facade

`MAX31865/Core.h` now includes the transport-backed driver and provides:

```cpp
using MAX31865Core = MAX31865;
```

This is intentionally an alias, not a second wrapper class. Existing
transport-backed code can migrate by including `MAX31865/Core.h` and declaring
`MAX31865Core rtd;`; existing Arduino compatibility sketches can continue to
include `MAX31865/MAX31865.h`.

If `MAX31865_HAS_ARDUINO_BACKEND=1` is already selected, `Core.h` rejects the
include so the pure-core path cannot accidentally grow Arduino overloads.
