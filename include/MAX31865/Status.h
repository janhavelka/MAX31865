/// @file Status.h
/// @brief Error codes and status handling for MAX31865 driver.
#pragma once

#include <cstdint>

namespace MAX31865 {

/// Error codes for all MAX31865 operations.
enum class Err : uint8_t {
  OK = 0,                  ///< Operation successful.
  NOT_INITIALIZED,         ///< begin() has not completed.
  INVALID_CONFIG,          ///< Invalid driver configuration.
  SPI_ERROR,               ///< SPI transport reported an error.
  SPI_TIMEOUT,             ///< SPI transaction timed out.
  TIMEOUT,                 ///< Operation timed out.
  INVALID_PARAM,           ///< Invalid caller parameter.
  DEVICE_NOT_FOUND,        ///< Device did not appear to respond.
  CONVERSION_NOT_READY,    ///< Conversion is not ready yet.
  MEASUREMENT_NOT_READY = CONVERSION_NOT_READY, ///< Cross-library alias.
  BUSY,                    ///< Device/driver is busy with another operation.
  IN_PROGRESS,             ///< Operation was started and is still running.
  FAULT_PRESENT            ///< RTD/fault status indicates a sensor fault.
};

/// Status structure returned by fallible operations.
struct Status {
  Err code = Err::OK;
  int32_t detail = 0;
  const char* msg = "";

  constexpr Status() = default;
  constexpr Status(Err codeIn, int32_t detailIn, const char* msgIn)
      : code(codeIn), detail(detailIn), msg(msgIn) {}

  /// @return true if operation succeeded.
  constexpr bool ok() const { return code == Err::OK; }

  /// @return true if status matches the supplied error code.
  constexpr bool is(Err err) const { return code == err; }

  /// @return true if operation is still in progress.
  constexpr bool inProgress() const { return code == Err::IN_PROGRESS; }

  /// @return true if operation succeeded.
  explicit constexpr operator bool() const { return ok(); }

  /// Create a success status.
  static constexpr Status Ok() { return Status{Err::OK, 0, "OK"}; }

  /// Create an error status.
  static constexpr Status Error(Err err, const char* message, int32_t detailCode = 0) {
    return Status{err, detailCode, message};
  }
};

}  // namespace MAX31865
