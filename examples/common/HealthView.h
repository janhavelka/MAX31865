#pragma once

#include <Arduino.h>
#include "MAX31865/MAX31865.h"

#include "CliStyle.h"

namespace health_view {

inline const char* colorGreen() { return LOG_COLOR_GREEN; }
inline const char* colorYellow() { return LOG_COLOR_YELLOW; }
inline const char* colorRed() { return LOG_COLOR_RED; }
inline const char* colorGray() { return LOG_COLOR_GRAY; }
inline const char* colorReset() { return cli::resetColor(); }

inline const char* boolColor(bool value) {
  return value ? colorGreen() : colorRed();
}

inline const char* failureColor(uint32_t failures) {
  if (failures == 0U) return colorGreen();
  if (failures < 3U) return colorYellow();
  return colorRed();
}

inline const char* successColor(uint32_t successes) {
  return (successes > 0U) ? colorGreen() : colorGray();
}

struct Snapshot {
  MAX31865DriverState driverState = MAX31865DriverState::UNINIT;
  MAX31865State lifecycleState = MAX31865State::Uninitialized;
  MAX31865Error lastError = MAX31865Error::Ok;
  bool online = false;
  uint8_t consecutiveFailures = 0;
  uint32_t totalFailures = 0;
  uint32_t totalSuccess = 0;
  uint32_t spiErrors = 0;
  uint32_t lockTimeouts = 0;
  uint32_t referenceAlarms = 0;
  uint32_t drdyTimeouts = 0;
  size_t dropped = 0;
  size_t overruns = 0;
  size_t depth = 0;
  uint32_t faultStatus = 0;

  void capture(const MAX31865& rtd) {
    MAX31865Health health = rtd.health();
    driverState = health.driver_state;
    lifecycleState = health.state;
    lastError = health.last_error;
    online = health.online;
    consecutiveFailures = health.consecutive_failures;
    totalFailures = health.total_failures;
    totalSuccess = health.total_success;
    spiErrors = health.spi_error_count;
    lockTimeouts = health.spi_lock_timeout_count;
    referenceAlarms = health.reference_alarm_count;
    drdyTimeouts = health.drdy_timeout_count;
    dropped = health.dropped_count;
    overruns = health.overrun_count;
    depth = health.buffer_depth;
    faultStatus = health.last_fault_status;
  }
};

inline void printHealthView(const MAX31865& rtd) {
  MAX31865Health health = rtd.health();
  const uint32_t total = health.total_success + health.total_failures;
  const float pct = (total > 0U)
                        ? (100.0f * static_cast<float>(health.total_success) /
                           static_cast<float>(total))
                        : 0.0f;

  Serial.printf("Health: state=%s%s%s online=%s%s%s consec=%s%u%s ok=%s%lu%s fail=%s%lu%s rate=%s%.1f%%%s\n",
                failureColor(static_cast<uint32_t>(health.consecutive_failures)),
                max31865DriverStateName(health.driver_state),
                colorReset(),
                boolColor(health.online),
                health.online ? "true" : "false",
                colorReset(),
                failureColor(static_cast<uint32_t>(health.consecutive_failures)),
                static_cast<unsigned>(health.consecutive_failures),
                colorReset(),
                successColor(health.total_success),
                static_cast<unsigned long>(health.total_success),
                colorReset(),
                failureColor(health.total_failures),
                static_cast<unsigned long>(health.total_failures),
                colorReset(),
                cli::successRateColor(pct),
                pct,
                colorReset());
  Serial.printf("RTD: lifecycle=%s err=%s depth=%u/%u dropped=%lu overrun=%lu reads=%lu kept=%lu spi=%lu lock=%lu ref=%lu timeout=%lu fault=0x%02X age_us=%lu auto=%s\n",
                max31865StateName(health.state),
                max31865ErrorName(health.last_error),
                static_cast<unsigned int>(health.buffer_depth),
                static_cast<unsigned int>(health.buffer_capacity),
                static_cast<unsigned long>(health.dropped_count),
                static_cast<unsigned long>(health.overrun_count),
                static_cast<unsigned long>(health.total_read_count),
                static_cast<unsigned long>(health.kept_sample_count),
                static_cast<unsigned long>(health.spi_error_count),
                static_cast<unsigned long>(health.spi_lock_timeout_count),
                static_cast<unsigned long>(health.reference_alarm_count),
                static_cast<unsigned long>(health.drdy_timeout_count),
                static_cast<unsigned>(health.last_fault_status),
                static_cast<unsigned long>(health.last_sample_age_us),
                health.auto_convert ? "on" : "off");
}

inline void printOneLine(const MAX31865& rtd) {
  printHealthView(rtd);
}

inline void printHealthDiff(const Snapshot& before, const Snapshot& after) {
  bool changed = false;
  if (before.driverState != after.driverState) {
    Serial.printf("  Health state: %s%s%s -> %s%s%s\n",
                  failureColor(before.consecutiveFailures),
                  max31865DriverStateName(before.driverState),
                  colorReset(),
                  failureColor(after.consecutiveFailures),
                  max31865DriverStateName(after.driverState),
                  colorReset());
    changed = true;
  }
  if (before.lifecycleState != after.lifecycleState) {
    Serial.printf("  Lifecycle: %s -> %s\n",
                  max31865StateName(before.lifecycleState),
                  max31865StateName(after.lifecycleState));
    changed = true;
  }
  if (before.lastError != after.lastError) {
    Serial.printf("  Last error: %s -> %s\n",
                  max31865ErrorName(before.lastError),
                  max31865ErrorName(after.lastError));
    changed = true;
  }
  if (before.online != after.online) {
    Serial.printf("  Online: %s%s%s -> %s%s%s\n",
                  boolColor(before.online),
                  before.online ? "true" : "false",
                  colorReset(),
                  boolColor(after.online),
                  after.online ? "true" : "false",
                  colorReset());
    changed = true;
  }
  if (before.consecutiveFailures != after.consecutiveFailures) {
    Serial.printf("  ConsecFail: %s%u -> %u%s\n",
                  failureColor(static_cast<uint32_t>(after.consecutiveFailures)),
                  static_cast<unsigned>(before.consecutiveFailures),
                  static_cast<unsigned>(after.consecutiveFailures),
                  colorReset());
    changed = true;
  }
  if (before.totalSuccess != after.totalSuccess) {
    Serial.printf("  TotalOK: %lu -> %s%lu (+%lu)%s\n",
                  static_cast<unsigned long>(before.totalSuccess),
                  colorGreen(),
                  static_cast<unsigned long>(after.totalSuccess),
                  static_cast<unsigned long>(after.totalSuccess - before.totalSuccess),
                  colorReset());
    changed = true;
  }
  if (before.totalFailures != after.totalFailures) {
    Serial.printf("  TotalFail: %lu -> %s%lu (+%lu)%s\n",
                  static_cast<unsigned long>(before.totalFailures),
                  colorRed(),
                  static_cast<unsigned long>(after.totalFailures),
                  static_cast<unsigned long>(after.totalFailures - before.totalFailures),
                  colorReset());
    changed = true;
  }
  if ((before.spiErrors != after.spiErrors) ||
      (before.lockTimeouts != after.lockTimeouts) ||
      (before.referenceAlarms != after.referenceAlarms) ||
      (before.drdyTimeouts != after.drdyTimeouts) ||
      (before.dropped != after.dropped) ||
      (before.overruns != after.overruns) ||
      (before.depth != after.depth) ||
      (before.faultStatus != after.faultStatus)) {
    Serial.printf("  Counters: spi %lu->%lu, lock %lu->%lu, ref %lu->%lu, timeout %lu->%lu, dropped %lu->%lu, overrun %lu->%lu, depth %lu->%lu, fault 0x%02lX->0x%02lX\n",
                  static_cast<unsigned long>(before.spiErrors),
                  static_cast<unsigned long>(after.spiErrors),
                  static_cast<unsigned long>(before.lockTimeouts),
                  static_cast<unsigned long>(after.lockTimeouts),
                  static_cast<unsigned long>(before.referenceAlarms),
                  static_cast<unsigned long>(after.referenceAlarms),
                  static_cast<unsigned long>(before.drdyTimeouts),
                  static_cast<unsigned long>(after.drdyTimeouts),
                  static_cast<unsigned long>(before.dropped),
                  static_cast<unsigned long>(after.dropped),
                  static_cast<unsigned long>(before.overruns),
                  static_cast<unsigned long>(after.overruns),
                  static_cast<unsigned long>(before.depth),
                  static_cast<unsigned long>(after.depth),
                  static_cast<unsigned long>(before.faultStatus),
                  static_cast<unsigned long>(after.faultStatus));
    changed = true;
  }
  if (!changed) {
    Serial.println("  (no health changes)");
  }
}

}  // namespace health_view

using HealthSnapshot = health_view::Snapshot;
using health_view::printHealthDiff;
using health_view::printHealthView;
