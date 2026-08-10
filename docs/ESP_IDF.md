# ESP-IDF callback integration

The repository packages the framework-neutral MAX31865 core as an ESP-IDF
component. It intentionally does not bundle a native ESP-IDF SPI/GPIO backend
or claim a native target build. An ESP-IDF application supplies and validates
its own `MAX31865Transport` callbacks and owns the SPI bus/device handles, GPIO
lifecycle, timing functions, locking, and `esp_err_t` mapping.

The application adapter must provide:

- manual active-low CS control;
- one exact contiguous, MSB-first, full-duplex transfer of at most nine bytes,
  using SPI mode 1 or mode 3 at no more than 5 MHz;
- a modulo-2^32 millisecond clock, yielding millisecond sleep, and microsecond
  delay;
- optional microsecond sample clock;
- optional active-low DRDY read, declared consistently in capabilities;
- optional paired finite bus lock/unlock callbacks.

Because the core controls CS separately from the transfer callback, configure
the IDF SPI device with automatic CS disabled (for example, no `spics_io_num`)
and drive the reviewed CS GPIO only through `setChipSelect`. Do not combine
automatic SPI-master CS with the core's manual CS edges.

The framework-neutral callback table carries no inspectable SPI-mode or clock
metadata. The application adapter must therefore enforce CPHA=1, MSB-first
ordering, and the `max31865_cmd::SPI_MAX_HZ` limit when it creates/configures
the IDF device handle; a successful core `begin()` cannot prove those external
electrical settings.

A typical adapter maps `spi_device_polling_transmit()` failures to
`SpiTransferFailed`, GPIO failures to `ChipSelectFailed`/`GpioFailed`, and bus
acquisition timeout to `BusLockTimeout`, retaining the platform error value in
`detail`. Callbacks make one attempt and return; retry/recovery policy remains
above the library. `sleepMs` normally uses a yielding FreeRTOS delay and
`delayUs` uses an appropriate bounded ROM/timer primitive.

For a shared SPI host, all device clients must use the same application arbiter.
The core's lock callbacks receive remaining operation budget and surround the
entire CS/frame transaction. They do not replace an instance mutex when
multiple tasks call one `MAX31865` object.

The root component metadata compiles only the neutral core/protocol sources and
public headers. An application component that includes ESP-IDF SPI/GPIO APIs
adds its own `esp_driver_spi`, `esp_driver_gpio`, `esp_timer`, and FreeRTOS
dependencies. This separation prevents ESP-IDF or Arduino implementation types
from entering the installed core API.

Required integration acceptance includes native adapter tests, ESP32-S2/S3 IDF
target builds, logic-analyzer verification of mode/CS timings, DRDY behavior,
fault injection, and real RTD accuracy checks. Repository core tests alone do
not establish those results.
