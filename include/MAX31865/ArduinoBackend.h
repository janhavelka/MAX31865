/**
 * @file ArduinoBackend.h
 * @brief Arduino SPI/GPIO adapter for MAX31865.
 */

#pragma once

#if defined(ARDUINO) || defined(DOXYGEN)

#include <Arduino.h>
#include <SPI.h>

#include "MAX31865/Config.h"
#include "MAX31865/Transport.h"

/**
 * @brief Optional application-owned shared-SPI-bus arbiter.
 *
 * The callback table is copied by max31865ArduinoBackendBind(), while `user`
 * and the object it names remain borrowed. They must outlive every driver that
 * uses a transport made from the backend. A successful `lock` must grant the
 * same bus ownership used by all other clients of the SPI host.
 */
struct MAX31865ArduinoBusArbiter
{
    void *user; ///< Borrowed application arbiter context.

    /**
     * @brief Make one finite attempt to acquire exclusive bus ownership.
     * @param user Borrowed application arbiter context.
     * @param timeoutMs Remaining whole-operation budget in milliseconds; zero
     * permits only an immediate, non-waiting attempt.
     * @return `Ok` only after acquisition, or `BusLockTimeout`/
     * `BusLockFailed` on failure. Other errors are mapped to `BusLockFailed`
     * with a static adapter message and their original code in `detail`.
     */
    MAX31865Status (*lock)(void *user, uint32_t timeoutMs);

    /**
     * @brief Release exactly one lock acquired by a successful lock callback.
     * @param user The same borrowed context passed to lock.
     */
    void (*unlock)(void *user);
};

/**
 * @brief Application-owned Arduino backend state borrowed by the driver.
 *
 * Binding and GPIO configuration are intentionally separate. The application
 * initializes its `SPIClass` first, normally with
 * `spi.begin(pins.sck, pins.miso, pins.mosi, MAX31865_PIN_UNUSED)`, then binds
 * this state, configures the control pins, constructs a transport, and calls
 * driver begin(). Hardware-controlled CS must remain disabled because the core
 * driver owns every active-low CS edge and its associated device side effects.
 * Once a transport made from this object is bound to a driver, the backend and
 * every borrowed object reachable through it are stable resources: do not
 * bind, rebind, reconfigure, or mutate them until that driver has called
 * end() or has been destroyed. Per-instance serialization does not make a live
 * backend rebind safe because the driver copies capabilities but callbacks
 * continue to dereference this object.
 *
 * Arduino SPI cannot report a wire-level error or cancel an in-flight fixed
 * transfer. The nine-byte frame bound and application-selected SPI clock bound
 * normal wire time. `beginTransaction()` scopes settings; it is not a mutex.
 */
struct MAX31865ArduinoBackend
{
    SPIClass *spi; ///< Borrowed initialized SPI object; null when unbound.
    SPISettings settings; ///< Per-device clock, bit-order, and mode settings.
    uint32_t spiClockHz; ///< Explicit validated clock metadata for settings.
    MAX31865Pins pins; ///< Copied application wiring used by the adapter.
    MAX31865ArduinoBusArbiter arbiter; ///< Copied optional arbiter callbacks.
    bool pinsConfigured; ///< True only after explicit safe pin configuration.

    /**
     * @brief Construct an inert, unbound backend without performing I/O.
     *
     * `spi` is null, `spiClockHz` is zero, every pin is
     * `MAX31865_PIN_UNUSED`, the arbiter is empty, and `pinsConfigured` is
     * false.
     */
    MAX31865ArduinoBackend();
    MAX31865ArduinoBackend(const MAX31865ArduinoBackend &) = delete;
    MAX31865ArduinoBackend &operator=(const MAX31865ArduinoBackend &) = delete;
    MAX31865ArduinoBackend(MAX31865ArduinoBackend &&) = delete;
    MAX31865ArduinoBackend &operator=(MAX31865ArduinoBackend &&) = delete;
};

/**
 * @brief Validate and transactionally store a nonowning Arduino binding.
 *
 * This function performs no GPIO or SPI I/O. Every successful bind or rebind
 * clears `pinsConfigured`; the application must explicitly configure the new
 * control-pin binding. It does not clean up pins from a previous binding.
 * SCK, MISO, MOSI, and chipSelect are required; dataReady may be
 * `MAX31865_PIN_UNUSED`. All used roles must be distinct and valid for the
 * target. MISO and dataReady must be input-capable; SCK, MOSI, and chipSelect
 * must be output-capable. Invalid input preserves the complete prior state.
 * Rebinding is legal only while no driver borrows a transport made from this
 * backend; end or destroy every such driver first.
 *
 * @param backend Application-owned backend state to bind.
 * @param spi Borrowed, application-initialized SPI object.
 * @param settings MAX31865 transaction settings. Use MSB first and SPI mode 1
 * or mode 3. Arduino keeps these fields opaque after construction.
 * @param spiClockHz Exact clock used to construct `settings`; validated as
 * nonzero and no greater than `max31865_cmd::SPI_MAX_HZ`.
 * @param pins Complete application wiring.
 * @param arbiter Optional shared-bus arbiter copied by value.
 * @return Complete validation status; failure performs no mutation or I/O.
 */
MAX31865Status max31865ArduinoBackendBind(
    MAX31865ArduinoBackend &backend,
    SPIClass &spi,
    const SPISettings &settings,
    uint32_t spiClockHz,
    const MAX31865Pins &pins,
    const MAX31865ArduinoBusArbiter *arbiter = nullptr);

/**
 * @brief Configure the MAX31865 control pins in a glitch-safe order.
 *
 * The required CS output latch is preloaded high before its output driver is
 * enabled. Optional DRDY is configured as an input without selecting a pull.
 * SCK, MISO, and MOSI remain application-owned and untouched. Arduino GPIO
 * calls do not report errors, so `Ok` proves that the ordered calls were issued,
 * not that a physical pin-state readback succeeded.
 *
 * @param backend Valid bound backend to configure.
 * @return Complete precondition/setup status.
 */
MAX31865Status max31865ArduinoConfigureControlPins(
    MAX31865ArduinoBackend &backend);

/**
 * @brief Construct a borrowed transport table for a configured backend.
 *
 * Callbacks reject use while `pinsConfigured` is false. The backend, SPI bus,
 * callback code, and object reachable through copied `arbiter.user` must
 * remain alive and unchanged for the complete driver binding. The returned
 * table is a snapshot of capabilities but its callbacks dereference `backend`;
 * therefore binding or configuring this backend again while a driver uses the
 * table is invalid. The callback-table object supplied to driver begin need
 * not outlive that call. If an unconfigured transport is passed to driver
 * begin(), recover by ending the driver, configuring the pins, constructing a
 * fresh transport table, and beginning again.
 *
 * @param backend Borrowed backend state that outlives the transport binding.
 * @return Framework-neutral callback table copied by driver begin().
 */
MAX31865Transport max31865ArduinoTransport(
    MAX31865ArduinoBackend &backend);

#endif // defined(ARDUINO) || defined(DOXYGEN)
