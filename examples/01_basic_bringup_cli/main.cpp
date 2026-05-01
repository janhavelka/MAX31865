#include <Arduino.h>
#include <SPI.h>
#include <MAX31865.h>
#include <MAX31865/Version.h>
#include <ctype.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "../common/BoardConfig.h"
#include "../common/Log.h"
#include "../common/TransportAdapter.h"
#include "../common/BusDiag.h"
#include "../common/HealthView.h"
#include "../common/CliStyle.h"

static const uint32_t DEFAULT_SPI_HZ = MAX31865_DEFAULT_SPI_HZ;
static constexpr uint32_t STRESS_PROGRESS_UPDATES = 10U;

MAX31865 rtd;

static char g_line[192];
static size_t g_line_len = 0;
static bool g_stream_enabled = false;
static bool g_verbose = true;
static uint32_t g_spi_hz = DEFAULT_SPI_HZ;
static uint32_t g_stream_interval_ms = 1000U;

static const char* onOffColor(bool enabled) {
    return enabled ? cli::kGreen : cli::kYellow;
}

static const char* okText(bool ok) {
    return ok ? "OK" : "FAIL";
}

static const char* wireModeName(MAX31865WireMode mode) {
    switch (mode) {
        case MAX31865WireMode::TwoWire: return "2-wire";
        case MAX31865WireMode::ThreeWire: return "3-wire";
        case MAX31865WireMode::FourWire: return "4-wire";
        default: return "unknown";
    }
}

static const char* filterName(MAX31865Filter filter) {
    return (filter == MAX31865Filter::Hz50) ? "50 Hz" : "60 Hz";
}

static const char* faultCycleName(uint8_t field) {
    switch (field & max31865_cmd::CONFIG_FAULT_CYCLE_MASK) {
        case max31865_cmd::CONFIG_FAULT_CYCLE_NONE: return "none";
        case max31865_cmd::CONFIG_FAULT_CYCLE_AUTO: return "auto";
        case max31865_cmd::CONFIG_FAULT_CYCLE_MANUAL_1: return "manual step 1";
        case max31865_cmd::CONFIG_FAULT_CYCLE_MANUAL_2: return "manual step 2";
        default: return "unknown";
    }
}

static const char* faultColor(const MAX31865FaultStatus& fault) {
    return fault.any() ? cli::kRed : cli::kGreen;
}

static uint32_t elapsedSince(uint32_t thenMs, uint32_t nowMs) {
    return (thenMs == 0U) ? 0U : (nowMs - thenMs);
}

static void toLowerInPlace(char* str) {
    while ((str != nullptr) && (*str != '\0')) {
        *str = static_cast<char>(tolower(static_cast<unsigned char>(*str)));
        str++;
    }
}

static bool parseU32(const char* token, uint32_t* out) {
    if ((token == nullptr) || (out == nullptr)) return false;
    char* end = nullptr;
    unsigned long value = strtoul(token, &end, 0);
    if ((end == token) || (*end != '\0')) return false;
    *out = static_cast<uint32_t>(value);
    return true;
}

static bool parseFloat(const char* token, float* out) {
    if ((token == nullptr) || (out == nullptr)) return false;
    char* end = nullptr;
    const float value = strtof(token, &end);
    if ((end == token) || (*end != '\0') || !isfinite(value)) return false;
    *out = value;
    return true;
}

static bool parseOnOff(char* token, bool* enabled) {
    if ((token == nullptr) || (enabled == nullptr)) return false;
    toLowerInPlace(token);
    if ((strcmp(token, "on") == 0) || (strcmp(token, "1") == 0) || (strcmp(token, "true") == 0)) {
        *enabled = true;
        return true;
    }
    if ((strcmp(token, "off") == 0) || (strcmp(token, "0") == 0) || (strcmp(token, "false") == 0)) {
        *enabled = false;
        return true;
    }
    return false;
}

static bool parseWireMode(char* token, MAX31865WireMode* mode) {
    if ((token == nullptr) || (mode == nullptr)) return false;
    toLowerInPlace(token);
    if ((strcmp(token, "2") == 0) || (strcmp(token, "2wire") == 0) || (strcmp(token, "two") == 0)) {
        *mode = MAX31865WireMode::TwoWire;
        return true;
    }
    if ((strcmp(token, "3") == 0) || (strcmp(token, "3wire") == 0) || (strcmp(token, "three") == 0)) {
        *mode = MAX31865WireMode::ThreeWire;
        return true;
    }
    if ((strcmp(token, "4") == 0) || (strcmp(token, "4wire") == 0) || (strcmp(token, "four") == 0)) {
        *mode = MAX31865WireMode::FourWire;
        return true;
    }
    return false;
}

static bool parseFilter(char* token, MAX31865Filter* filter) {
    if ((token == nullptr) || (filter == nullptr)) return false;
    toLowerInPlace(token);
    if ((strcmp(token, "50") == 0) || (strcmp(token, "50hz") == 0)) {
        *filter = MAX31865Filter::Hz50;
        return true;
    }
    if ((strcmp(token, "60") == 0) || (strcmp(token, "60hz") == 0)) {
        *filter = MAX31865Filter::Hz60;
        return true;
    }
    return false;
}

static void printUsage(const char* usage) {
    Serial.printf("%sUsage:%s %s\n", cli::kYellow, cli::kReset, usage);
}

static void printResult(const char* label, bool ok) {
    Serial.printf("%s: %s%s%s\n", label, cli::okColor(ok), okText(ok), cli::kReset);
}

static void printStatus(const MAX31865Status& status) {
    Serial.printf("  Status: %s%s%s (code=%u, detail=%ld)\n",
                  LOG_COLOR_RESULT(status.ok()),
                  max31865ErrorName(status.code),
                  LOG_COLOR_RESET,
                  static_cast<unsigned>(status.code),
                  static_cast<long>(status.detail));
    if ((status.msg != nullptr) && (status.msg[0] != '\0') && (strcmp(status.msg, "OK") != 0)) {
        Serial.printf("  Message: %s%s%s\n", LOG_COLOR_YELLOW, status.msg, LOG_COLOR_RESET);
    }
}

static void printLastStatus() {
    printStatus(rtd.lastOperationStatus());
}

static uint32_t stressProgressStep(uint32_t total) {
    if (total == 0U) return 0U;
    const uint32_t step = total / STRESS_PROGRESS_UPDATES;
    return (step == 0U) ? 1U : step;
}

static void printStressProgress(uint32_t completed, uint32_t total, uint32_t okCount, uint32_t failCount) {
    if ((completed == 0U) || (total == 0U)) return;
    const uint32_t step = stressProgressStep(total);
    if ((completed != total) && ((completed % step) != 0U)) return;
    const float pct = (100.0f * static_cast<float>(completed)) / static_cast<float>(total);
    Serial.printf("  Progress: %lu/%lu (%s%.0f%%%s, ok=%s%lu%s, fail=%s%lu%s)\n",
                  static_cast<unsigned long>(completed),
                  static_cast<unsigned long>(total),
                  cli::successRateColor(pct),
                  pct,
                  LOG_COLOR_RESET,
                  cli::nonZeroGoodColor(okCount),
                  static_cast<unsigned long>(okCount),
                  LOG_COLOR_RESET,
                  cli::zeroGoodColor(failCount),
                  static_cast<unsigned long>(failCount),
                  LOG_COLOR_RESET);
}

static void printHelp() {
    Serial.println();
    cli::printHelpHeader("MAX31865 Unified Bringup CLI");
    cli::printHelpSection("Common");
    cli::printHelpItem("help / ?", "Show this help");
    cli::printHelpItem("version / ver", "Print firmware and library version info");
    cli::printHelpItem("scan", "Print SPI wiring, pin levels, raw probe, and registers");
    cli::printHelpItem("probe", "Probe writable register readback without health side effects");
    cli::printHelpItem("recover", "Tracked recover with cached CONFIG restore");
    cli::printHelpItem("drv / health", "Detailed driver and health counters");
    cli::printHelpItem("state", "Compact one-line health summary");
    cli::printHelpItem("read", "Read one one-shot sample");
    cli::printHelpItem("verbose [0|1]", "Get/set verbose mode");
    cli::printHelpItem("stress [N] [probe|read]", "Run N probe or read iterations");
    cli::printHelpItem("selftest", "Run colored safe command self-test report");
    cli::printHelpItem("id", "Explain no-ID probe semantics");
    cli::printHelpItem("comms", "SPI comms + safe threshold write/readback test");
    cli::printHelpItem("reset [regs]", "Restore documented register defaults");
    cli::printHelpItem("cmd <clear|faultauto|faultmanual|oneshot>", "Run direct command alias");
    cli::printHelpItem("lock / unlock", "Report unsupported register-lock feature");
    cli::printHelpItem("regs", "Dump all registers");
    cli::printHelpItem("rreg <addr>", "Read one register");
    cli::printHelpItem("rregs <addr> <len>", "Read register range");
    cli::printHelpItem("wreg <addr> <value>", "Write one writable register");
    cli::printHelpItem("wregv <addr> <value>", "Write and verify one register");
    cli::printHelpItem("cfg / settings", "Decode key MAX31865 configuration");
    cli::printHelpItem("clearhealth", "Clear driver health counters");

    cli::printHelpSection("Measurement");
    cli::printHelpItem("start", "Enable continuous conversion");
    cli::printHelpItem("stop", "Stop continuous conversion");
    cli::printHelpItem("stream [on|off|ms]", "Toggle streaming or set stream interval");
    cli::printHelpItem("sample [N]", "Read N one-shot samples");
    cli::printHelpItem("sps [ms]", "Timed one-shot throughput benchmark");
    cli::printHelpItem("temp", "Read one temperature sample");
    cli::printHelpItem("stats", "Print sample and health counters");
    cli::printHelpItem("fault", "Read decoded fault status");
    cli::printHelpItem("clear", "Clear fault latch");
    cli::printHelpItem("faultauto", "Run automatic fault-detection cycle");
    cli::printHelpItem("faultmanual [us]", "Run manual fault cycle with settle delay");
    cli::printHelpItem("threshold", "Print threshold raw, ohm, and Celsius values");
    cli::printHelpItem("threshold raw <low> <high>", "Set threshold ADC codes");
    cli::printHelpItem("threshold ohm <low> <high>", "Set threshold resistance");
    cli::printHelpItem("threshold temp <low> <high>", "Set threshold Celsius");

    cli::printHelpSection("Config");
    cli::printHelpItem("spihz [hz]", "Get/set SPI clock");
    cli::printHelpItem("lockms [ms]", "Get/set SPI lock timeout");
    cli::printHelpItem("filter [50|60]", "Get/set digital notch filter");
    cli::printHelpItem("wires [2|3|4]", "Get/set RTD wiring mode");
    cli::printHelpItem("bias [on|off]", "Get/set VBIAS");
    cli::printHelpItem("auto [on|off]", "Get/set continuous conversion");
    cli::printHelpItem("rtd", "Print RTD/reference parameters");
    cli::printHelpItem("rtd <rref> <rnom>", "Set reference resistor and nominal RTD ohms");
    cli::printHelpItem("rtd coeff <a> <b> <c>", "Set Callendar-Van Dusen coefficients");
    cli::printHelpItem("scale [code|ohm|temp] <value>", "Convert raw code, resistance, or Celsius");
}

static void printVersion() {
    Serial.printf("  Example firmware build: %s %s\n", __DATE__, __TIME__);
    Serial.printf("  MAX31865 library version: %s\n", MAX31865Version::VERSION);
    Serial.printf("  MAX31865 library full: %s\n", MAX31865Version::VERSION_FULL);
    Serial.printf("  MAX31865 library build: %s\n", MAX31865Version::BUILD_TIMESTAMP);
    Serial.printf("  MAX31865 library commit: %s (%s)\n",
                  MAX31865Version::GIT_COMMIT,
                  MAX31865Version::GIT_STATUS);
    Serial.printf("  MAX31865 version code: %lu (major=%u minor=%u patch=%u)\n",
                  static_cast<unsigned long>(MAX31865Version::VERSION_CODE),
                  static_cast<unsigned>(MAX31865Version::VERSION_MAJOR),
                  static_cast<unsigned>(MAX31865Version::VERSION_MINOR),
                  static_cast<unsigned>(MAX31865Version::VERSION_PATCH));
}

static void printFaultDecoded(const MAX31865FaultStatus& fault) {
    Serial.printf("  FAULT: %s0x%02X%s high=%u low=%u refinHigh=%u refinLow=%u rtdinLow=%u ovuv=%u\n",
                  faultColor(fault),
                  fault.raw,
                  cli::kReset,
                  fault.high_threshold ? 1U : 0U,
                  fault.low_threshold ? 1U : 0U,
                  fault.refin_high ? 1U : 0U,
                  fault.refin_low ? 1U : 0U,
                  fault.rtdin_low ? 1U : 0U,
                  fault.over_under_voltage ? 1U : 0U);
}

static void printRegisterDump() {
    MAX31865RegisterDump rows[8];
    const size_t n = rtd.dumpRegisters(rows, 8);
    for (size_t i = 0; i < n; ++i) {
        Serial.printf("  0x%02X %-12s 0x%02X\n", rows[i].addr, rows[i].name, rows[i].value);
    }
}

static void printSettings() {
    MAX31865Settings settings{};
    if (!rtd.getSettings(settings)) {
        printLastStatus();
        return;
    }
    MAX31865Health health = rtd.health();
    const uint32_t now = millis();
    Serial.printf("  Health: %s%s%s online=%s%s%s lifecycle=%s lastError=%s\n",
                  health_view::failureColor(health.consecutive_failures),
                  max31865DriverStateName(health.driver_state),
                  cli::kReset,
                  health_view::boolColor(health.online),
                  health.online ? "true" : "false",
                  cli::kReset,
                  max31865StateName(health.state),
                  max31865ErrorName(health.last_error));
    Serial.printf("  SPI clock: %lu Hz  lockTimeout=%lu ms\n",
                  static_cast<unsigned long>(rtd.spiHz()),
                  static_cast<unsigned long>(rtd.spiLockTimeoutMs()));
    Serial.printf("  RTD model: Rref=%.5f ohm R0=%.5f ohm coeff=[%.8g %.8g %.8g]\n",
                  rtd.referenceResistorOhms(),
                  rtd.rtdNominalOhms(),
                  rtd.rtdCoefficients().a,
                  rtd.rtdCoefficients().b,
                  rtd.rtdCoefficients().c);
    Serial.printf("  CONFIG: 0x%02X bias=%s%s%s auto=%s%s%s oneshot=%u wire=%s filter=%s faultCycle=%s\n",
                  settings.config_register,
                  onOffColor(settings.bias_enabled),
                  settings.bias_enabled ? "on" : "off",
                  cli::kReset,
                  onOffColor(settings.auto_convert),
                  settings.auto_convert ? "on" : "off",
                  cli::kReset,
                  settings.one_shot ? 1U : 0U,
                  wireModeName(settings.wire_mode),
                  filterName(settings.filter),
                  faultCycleName(settings.fault_cycle));
    Serial.printf("  Timing: single=%lu ms continuous=%lu ms biasSettle=%lu us lastOk=%lu ms ago lastErr=%lu ms ago\n",
                  static_cast<unsigned long>(rtd.getSingleConversionTimeMs()),
                  static_cast<unsigned long>(rtd.getContinuousConversionTimeMs()),
                  static_cast<unsigned long>(rtd.getBiasSettleTimeUs()),
                  static_cast<unsigned long>(elapsedSince(health.last_ok_ms, now)),
                  static_cast<unsigned long>(elapsedSince(health.last_error_ms, now)));
    Serial.printf("  Threshold low: code=%u R=%.5f ohm T=%.4f C\n",
                  settings.thresholds.low_code,
                  settings.low_threshold_ohms,
                  settings.low_threshold_c);
    Serial.printf("  Threshold high: code=%u R=%.5f ohm T=%.4f C\n",
                  settings.thresholds.high_code,
                  settings.high_threshold_ohms,
                  settings.high_threshold_c);
}

static void printDetailedHealth() {
    MAX31865Health health = rtd.health();
    const uint32_t now = millis();
    const uint32_t total = health.total_success + health.total_failures;
    const float pct = (total > 0U)
                          ? (100.0f * static_cast<float>(health.total_success) / static_cast<float>(total))
                          : 0.0f;
    Serial.printf("  Health state: %s%s%s\n",
                  health_view::failureColor(health.consecutive_failures),
                  max31865DriverStateName(health.driver_state),
                  cli::kReset);
    Serial.printf("  Online: %s%s%s\n",
                  health_view::boolColor(health.online),
                  health.online ? "true" : "false",
                  cli::kReset);
    Serial.printf("  Lifecycle: %s\n", max31865StateName(health.state));
    Serial.printf("  Last error: %s%s%s\n",
                  (health.last_error == MAX31865Error::Ok) ? cli::kGreen : cli::kRed,
                  max31865ErrorName(health.last_error),
                  cli::kReset);
    Serial.printf("  Consecutive failures: %s%u%s / threshold %u\n",
                  health_view::failureColor(health.consecutive_failures),
                  static_cast<unsigned>(health.consecutive_failures),
                  cli::kReset,
                  static_cast<unsigned>(health.offline_threshold));
    Serial.printf("  Total success: %s%lu%s\n",
                  health_view::successColor(health.total_success),
                  static_cast<unsigned long>(health.total_success),
                  cli::kReset);
    Serial.printf("  Total failures: %s%lu%s\n",
                  health_view::failureColor(health.total_failures),
                  static_cast<unsigned long>(health.total_failures),
                  cli::kReset);
    Serial.printf("  Success rate: %s%.1f%%%s\n", cli::successRateColor(pct), pct, cli::kReset);
    if (health.last_ok_ms > 0U) {
        Serial.printf("  Last OK: %s%lu ms ago (at %lu ms)%s\n",
                      cli::kGreen,
                      static_cast<unsigned long>(elapsedSince(health.last_ok_ms, now)),
                      static_cast<unsigned long>(health.last_ok_ms),
                      cli::kReset);
    } else {
        Serial.printf("  Last OK: %snever%s\n", cli::kYellow, cli::kReset);
    }
    if (health.last_error_ms > 0U) {
        Serial.printf("  Last error: %s%lu ms ago (at %lu ms)%s\n",
                      cli::kYellow,
                      static_cast<unsigned long>(elapsedSince(health.last_error_ms, now)),
                      static_cast<unsigned long>(health.last_error_ms),
                      cli::kReset);
    } else {
        Serial.printf("  Last error: %snever%s\n", cli::kGreen, cli::kReset);
    }
    Serial.printf("  Conversion: active=%s%s%s auto=%s%s%s available=%s%s%s\n",
                  onOffColor(health.converting),
                  health.converting ? "yes" : "no",
                  cli::kReset,
                  onOffColor(health.auto_convert),
                  health.auto_convert ? "on" : "off",
                  cli::kReset,
                  onOffColor(rtd.available()),
                  rtd.available() ? "yes" : "no",
                  cli::kReset);
    Serial.printf("  Reads: total=%lu kept=%lu dropped=%s%lu%s overrun=%s%lu%s depth=%u/%u highWater=%u\n",
                  static_cast<unsigned long>(health.total_read_count),
                  static_cast<unsigned long>(health.kept_sample_count),
                  cli::zeroGoodColor(static_cast<uint32_t>(health.dropped_count)),
                  static_cast<unsigned long>(health.dropped_count),
                  cli::kReset,
                  cli::zeroGoodColor(static_cast<uint32_t>(health.overrun_count)),
                  static_cast<unsigned long>(health.overrun_count),
                  cli::kReset,
                  static_cast<unsigned>(health.buffer_depth),
                  static_cast<unsigned>(health.buffer_capacity),
                  static_cast<unsigned>(health.queue_high_water));
    Serial.printf("  Alarms: spi=%s%lu%s lockTimeout=%s%lu%s ref=%s%lu%s drdyTimeout=%s%lu%s fault=0x%02X lastSampleAge=%lu us\n",
                  cli::zeroGoodColor(health.spi_error_count),
                  static_cast<unsigned long>(health.spi_error_count),
                  cli::kReset,
                  cli::zeroGoodColor(health.spi_lock_timeout_count),
                  static_cast<unsigned long>(health.spi_lock_timeout_count),
                  cli::kReset,
                  cli::zeroGoodColor(health.reference_alarm_count),
                  static_cast<unsigned long>(health.reference_alarm_count),
                  cli::kReset,
                  cli::zeroGoodColor(health.drdy_timeout_count),
                  static_cast<unsigned long>(health.drdy_timeout_count),
                  cli::kReset,
                  static_cast<unsigned>(health.last_fault_status),
                  static_cast<unsigned long>(health.last_sample_age_us));
}

static void printStats() {
    MAX31865Health health = rtd.health();
    Serial.printf("Total reads: %lu\n", static_cast<unsigned long>(health.total_read_count));
    Serial.printf("Kept samples: %lu\n", static_cast<unsigned long>(health.kept_sample_count));
    Serial.printf("Dropped samples: %s%lu%s\n",
                  cli::zeroGoodColor(static_cast<uint32_t>(health.dropped_count)),
                  static_cast<unsigned long>(health.dropped_count),
                  cli::kReset);
    Serial.printf("Overruns: %s%lu%s\n",
                  cli::zeroGoodColor(static_cast<uint32_t>(health.overrun_count)),
                  static_cast<unsigned long>(health.overrun_count),
                  cli::kReset);
    Serial.printf("SPI errors: %s%lu%s\n",
                  cli::zeroGoodColor(health.spi_error_count),
                  static_cast<unsigned long>(health.spi_error_count),
                  cli::kReset);
    Serial.printf("Reference alarms: %s%lu%s\n",
                  cli::zeroGoodColor(health.reference_alarm_count),
                  static_cast<unsigned long>(health.reference_alarm_count),
                  cli::kReset);
    Serial.printf("Timeouts: %s%lu%s\n",
                  cli::zeroGoodColor(health.drdy_timeout_count),
                  static_cast<unsigned long>(health.drdy_timeout_count),
                  cli::kReset);
}

static void printSample(const MAX31865Sample& sample) {
    Serial.printf("%lu,%lu,code=%u,raw=0x%04X,R=%.5f,T=%.4f,fault=%u\n",
                  static_cast<unsigned long>(sample.timestamp_ms),
                  static_cast<unsigned long>(sample.sample_counter),
                  sample.raw.code,
                  sample.raw.raw_register,
                  sample.resistance_ohms,
                  sample.temperature_c,
                  sample.raw.fault ? 1U : 0U);
    if (sample.has_fault_status) {
        printFaultDecoded(sample.fault_status);
    }
}

static bool readOne() {
    HealthSnapshot before;
    before.capture(rtd);
    MAX31865Sample sample{};
    const bool ok = rtd.readSingle(sample);
    if (ok) {
        printSample(sample);
    }
    printLastStatus();
    HealthSnapshot after;
    after.capture(rtd);
    if (g_verbose) printHealthDiff(before, after);
    return ok;
}

static void printFault() {
    MAX31865FaultStatus fault{};
    const bool ok = rtd.readFaultStatus(fault);
    printLastStatus();
    if (ok) {
        printFaultDecoded(fault);
    }
}

static void printThresholds() {
    MAX31865Settings settings{};
    if (!rtd.getSettings(settings)) {
        printLastStatus();
        return;
    }
    Serial.printf("Low:  code=%u R=%.5f ohm T=%.4f C\n",
                  settings.thresholds.low_code,
                  settings.low_threshold_ohms,
                  settings.low_threshold_c);
    Serial.printf("High: code=%u R=%.5f ohm T=%.4f C\n",
                  settings.thresholds.high_code,
                  settings.high_threshold_ohms,
                  settings.high_threshold_c);
}

static void printScan() {
    MAX31865Pins pins = board::rtdPins();
    transport_adapter::SpiPins spiPins{pins.sck, pins.miso, pins.mosi, pins.cs, pins.drdy, -1, -1, -1};
    transport_adapter::printPins(spiPins);
    bus_diag::printDrdyLevel(pins.drdy);
    Serial.println("=== Probe ===");
    printStatus(rtd.probe());
    Serial.println("=== Settings ===");
    printSettings();
    Serial.println("=== Registers ===");
    printRegisterDump();
}

static void runProbeStress(uint32_t iterations) {
    uint32_t okCount = 0;
    uint32_t failCount = 0;
    const uint32_t start = millis();
    MAX31865Status firstFailure = MAX31865Status::Ok();
    for (uint32_t i = 0; i < iterations; ++i) {
        const MAX31865Status st = rtd.probe();
        if (st.ok()) {
            okCount++;
        } else {
            if (failCount == 0U) firstFailure = st;
            failCount++;
            LOGV(g_verbose, "probe %lu failed: %s", static_cast<unsigned long>(i + 1), max31865ErrorName(st.code));
        }
        printStressProgress(i + 1U, iterations, okCount, failCount);
    }
    const uint32_t elapsed = millis() - start;
    const float pct = (iterations > 0U) ? (100.0f * static_cast<float>(okCount) / static_cast<float>(iterations)) : 0.0f;
    Serial.println("Stress summary:");
    Serial.printf("  Total: %lu\n", static_cast<unsigned long>(iterations));
    Serial.printf("  Success: %s%lu%s\n", cli::nonZeroGoodColor(okCount), static_cast<unsigned long>(okCount), cli::kReset);
    Serial.printf("  Errors: %s%lu%s\n", cli::zeroGoodColor(failCount), static_cast<unsigned long>(failCount), cli::kReset);
    Serial.printf("  Success rate: %s%.2f%%%s\n", cli::successRateColor(pct), pct, cli::kReset);
    Serial.printf("  Duration: %lu ms\n", static_cast<unsigned long>(elapsed));
    if (elapsed > 0U) {
        Serial.printf("  Rate: %.2f probes/s\n", (1000.0f * static_cast<float>(iterations)) / static_cast<float>(elapsed));
    }
    if (failCount > 0U) {
        printStatus(firstFailure);
    }
}

static void runReadStress(uint32_t iterations) {
    uint32_t okCount = 0;
    uint32_t failCount = 0;
    const uint32_t start = millis();
    for (uint32_t i = 0; i < iterations; ++i) {
        if (readOne()) okCount++; else failCount++;
        printStressProgress(i + 1U, iterations, okCount, failCount);
    }
    const uint32_t elapsed = millis() - start;
    const float pct = (iterations > 0U) ? (100.0f * static_cast<float>(okCount) / static_cast<float>(iterations)) : 0.0f;
    Serial.println("Read stress summary:");
    Serial.printf("  Total: %lu\n", static_cast<unsigned long>(iterations));
    Serial.printf("  Success: %s%lu%s\n", cli::nonZeroGoodColor(okCount), static_cast<unsigned long>(okCount), cli::kReset);
    Serial.printf("  Errors: %s%lu%s\n", cli::zeroGoodColor(failCount), static_cast<unsigned long>(failCount), cli::kReset);
    Serial.printf("  Success rate: %s%.2f%%%s\n", cli::successRateColor(pct), pct, cli::kReset);
    Serial.printf("  Duration: %lu ms\n", static_cast<unsigned long>(elapsed));
}

static void runSps(uint32_t durationMs) {
    if (durationMs == 0U) {
        durationMs = 5000U;
    }
    uint32_t count = 0;
    uint32_t fail = 0;
    uint32_t minDelta = UINT32_MAX;
    uint32_t maxDelta = 0;
    uint64_t sumDelta = 0;
    uint32_t lastMs = 0;
    const uint32_t start = millis();
    Serial.printf("SPS run started (%lu ms)\n", static_cast<unsigned long>(durationMs));
    while ((millis() - start) < durationMs) {
        MAX31865Sample sample{};
        const uint32_t before = millis();
        if (rtd.readSingle(sample)) {
            const uint32_t now = millis();
            if (lastMs != 0U) {
                const uint32_t delta = now - lastMs;
                if (delta < minDelta) minDelta = delta;
                if (delta > maxDelta) maxDelta = delta;
                sumDelta += delta;
            }
            lastMs = now;
            count++;
        } else {
            fail++;
        }
        if ((millis() - before) == 0U) {
            delay(1);
        }
    }
    const float seconds = static_cast<float>(millis() - start) / 1000.0f;
    const float sps = (seconds > 0.0f) ? (static_cast<float>(count) / seconds) : 0.0f;
    Serial.println("SPS summary:");
    Serial.printf("  Duration: %.3f s\n", seconds);
    Serial.printf("  Samples: %s%lu%s\n", cli::nonZeroGoodColor(count), static_cast<unsigned long>(count), cli::kReset);
    Serial.printf("  Failures: %s%lu%s\n", cli::zeroGoodColor(fail), static_cast<unsigned long>(fail), cli::kReset);
    Serial.printf("  Effective SPS: %.2f\n", sps);
    if (count > 1U) {
        const float avg = static_cast<float>(sumDelta) / static_cast<float>(count - 1U);
        Serial.printf("  Delta ms min/avg/max: %lu / %.2f / %lu\n",
                      static_cast<unsigned long>(minDelta),
                      avg,
                      static_cast<unsigned long>(maxDelta));
    }
}

struct SelfTestCounters {
    uint32_t pass = 0;
    uint32_t fail = 0;
    uint32_t skip = 0;
};

static void reportSelfTest(SelfTestCounters& counters, const char* tag, const char* name, bool pass, bool skipped = false, const char* note = nullptr) {
    const char* color = skipped ? cli::kYellow : cli::okColor(pass);
    if (skipped) counters.skip++;
    else if (pass) counters.pass++;
    else counters.fail++;
    Serial.printf("  [%s%s%s] %s", color, skipped ? "SKIP" : (pass ? "PASS" : "FAIL"), cli::kReset, name);
    if (note != nullptr) {
        Serial.printf(" - %s", note);
    }
    Serial.printf(" (%s)\n", tag);
}

static void runSelfTest() {
    SelfTestCounters counters;
    Serial.println("Selftest:");
    const MAX31865Status probe = rtd.probe();
    reportSelfTest(counters, "probe", "Writable-register probe", probe.ok(), false, probe.msg);

    uint8_t readback = 0;
    const bool comms = rtd.registerReadbackTest(&readback);
    reportSelfTest(counters, "comms", "Threshold write/readback", comms, false, comms ? "readback restored" : rtd.lastErrorName());

    MAX31865Settings settings{};
    reportSelfTest(counters, "cfg", "Read and decode settings", rtd.getSettings(settings), false, rtd.lastErrorName());

    MAX31865FaultStatus fault{};
    reportSelfTest(counters, "fault", "Read fault register", rtd.readFaultStatus(fault), false, fault.any() ? "fault bits set" : "no faults");

    MAX31865FaultThresholds thresholds{};
    reportSelfTest(counters, "threshold", "Read threshold registers", rtd.getFaultThresholdsRaw(thresholds), false, rtd.lastErrorName());

    MAX31865Sample sample{};
    const bool sampleOk = rtd.readSingle(sample);
    reportSelfTest(counters, "read", "One-shot conversion", sampleOk, false, sampleOk ? "temperature decoded" : rtd.lastErrorName());

    Serial.printf("Selftest result: pass=%s%lu%s fail=%s%lu%s skip=%s%lu%s\n",
                  cli::nonZeroGoodColor(counters.pass),
                  static_cast<unsigned long>(counters.pass),
                  cli::kReset,
                  cli::zeroGoodColor(counters.fail),
                  static_cast<unsigned long>(counters.fail),
                  cli::kReset,
                  cli::warningIfNonZeroColor(counters.skip),
                  static_cast<unsigned long>(counters.skip),
                  cli::kReset);
}

static void handleThresholdCommand(char* sub) {
    if (sub == nullptr) {
        printThresholds();
        return;
    }
    toLowerInPlace(sub);
    char* lowTok = strtok(nullptr, " \t\r\n");
    char* highTok = strtok(nullptr, " \t\r\n");
    if ((lowTok == nullptr) || (highTok == nullptr)) {
        printUsage("threshold raw|ohm|temp <low> <high>");
        return;
    }
    bool ok = false;
    if (strcmp(sub, "raw") == 0) {
        uint32_t low = 0;
        uint32_t high = 0;
        ok = parseU32(lowTok, &low) && parseU32(highTok, &high) &&
             rtd.setFaultThresholdsRaw(static_cast<uint16_t>(low), static_cast<uint16_t>(high));
    } else if (strcmp(sub, "ohm") == 0) {
        float low = 0.0f;
        float high = 0.0f;
        ok = parseFloat(lowTok, &low) && parseFloat(highTok, &high) &&
             rtd.setFaultThresholdsResistance(low, high);
    } else if (strcmp(sub, "temp") == 0) {
        float low = 0.0f;
        float high = 0.0f;
        ok = parseFloat(lowTok, &low) && parseFloat(highTok, &high) &&
             rtd.setFaultThresholdsTemperature(low, high);
    } else {
        printUsage("threshold raw|ohm|temp <low> <high>");
        return;
    }
    printResult("Threshold", ok);
    printLastStatus();
    if (ok) printThresholds();
}

static void handleScaleCommand(char* sub) {
    if (sub == nullptr) {
        sub = const_cast<char*>("code");
    }
    toLowerInPlace(sub);
    char* valueTok = strtok(nullptr, " \t\r\n");
    float value = 0.0f;
    if ((strcmp(sub, "code") == 0) || (strcmp(sub, "raw") == 0)) {
        uint32_t code = 8192;
        if (valueTok != nullptr && !parseU32(valueTok, &code)) {
            printUsage("scale code <0..32767>");
            return;
        }
        const uint16_t clipped = (code > max31865_cmd::ADC_CODE_MAX) ? max31865_cmd::ADC_CODE_MAX : static_cast<uint16_t>(code);
        const float ohms = rtd.codeToResistance(clipped);
        Serial.printf("  Code: %u\n", clipped);
        Serial.printf("  Ratio: %.9f FS\n", MAX31865::codeToRatio(clipped));
        Serial.printf("  Resistance: %.6f ohm\n", ohms);
        Serial.printf("  Temperature: %.6f C\n", rtd.resistanceToTemperature(ohms));
        return;
    }
    if ((valueTok == nullptr) || !parseFloat(valueTok, &value)) {
        printUsage("scale code|ohm|temp <value>");
        return;
    }
    if ((strcmp(sub, "ohm") == 0) || (strcmp(sub, "r") == 0)) {
        const uint16_t code = rtd.resistanceToCode(value);
        Serial.printf("  Resistance: %.6f ohm\n", value);
        Serial.printf("  Code: %u\n", code);
        Serial.printf("  Ratio: %.9f FS\n", MAX31865::codeToRatio(code));
        Serial.printf("  Temperature: %.6f C\n", rtd.resistanceToTemperature(value));
    } else if ((strcmp(sub, "temp") == 0) || (strcmp(sub, "c") == 0)) {
        const float ohms = rtd.temperatureToResistance(value);
        const uint16_t code = rtd.temperatureToCode(value);
        Serial.printf("  Temperature: %.6f C\n", value);
        Serial.printf("  Resistance: %.6f ohm\n", ohms);
        Serial.printf("  Code: %u\n", code);
        Serial.printf("  Ratio: %.9f FS\n", MAX31865::codeToRatio(code));
    } else {
        printUsage("scale code|ohm|temp <value>");
    }
}

static void handleRtdCommand(char* sub) {
    if (sub == nullptr) {
        MAX31865RtdCoefficients c = rtd.rtdCoefficients();
        Serial.printf("Rref=%.6f ohm R0=%.6f ohm coeff=[%.8g %.8g %.8g]\n",
                      rtd.referenceResistorOhms(),
                      rtd.rtdNominalOhms(),
                      c.a, c.b, c.c);
        return;
    }
    toLowerInPlace(sub);
    if (strcmp(sub, "coeff") == 0) {
        float a = 0.0f;
        float b = 0.0f;
        float c = 0.0f;
        if (!parseFloat(strtok(nullptr, " \t\r\n"), &a) ||
            !parseFloat(strtok(nullptr, " \t\r\n"), &b) ||
            !parseFloat(strtok(nullptr, " \t\r\n"), &c)) {
            printUsage("rtd coeff <a> <b> <c>");
            return;
        }
        MAX31865RtdCoefficients coeff{a, b, c};
        printResult("RTD coefficients", rtd.setRtdParameters(rtd.referenceResistorOhms(), rtd.rtdNominalOhms(), &coeff));
        printLastStatus();
        return;
    }
    float rref = 0.0f;
    float rnom = 0.0f;
    if (!parseFloat(sub, &rref) || !parseFloat(strtok(nullptr, " \t\r\n"), &rnom)) {
        printUsage("rtd <rref> <rnom>");
        return;
    }
    printResult("RTD parameters", rtd.setRtdParameters(rref, rnom));
    printLastStatus();
}

static void handleCommand(char* line) {
    char* cmd = strtok(line, " \t\r\n");
    if (cmd == nullptr) return;
    toLowerInPlace(cmd);

    if ((strcmp(cmd, "help") == 0) || (strcmp(cmd, "?") == 0)) {
        printHelp();
        return;
    }
    if ((strcmp(cmd, "version") == 0) || (strcmp(cmd, "ver") == 0)) {
        printVersion();
        return;
    }
    if (strcmp(cmd, "scan") == 0) {
        printScan();
        return;
    }
    if (strcmp(cmd, "probe") == 0) {
        printStatus(rtd.probe());
        return;
    }
    if (strcmp(cmd, "recover") == 0) {
        LOGI("Attempting recovery...");
        printStatus(rtd.recover());
        printDetailedHealth();
        return;
    }
    if (strcmp(cmd, "state") == 0) {
        health_view::printOneLine(rtd);
        return;
    }
    if ((strcmp(cmd, "drv") == 0) || (strcmp(cmd, "health") == 0)) {
        printDetailedHealth();
        return;
    }
    if (strcmp(cmd, "read") == 0) {
        (void)readOne();
        return;
    }
    if (strcmp(cmd, "verbose") == 0) {
        char* arg = strtok(nullptr, " \t\r\n");
        bool enabled = false;
        if (arg != nullptr) {
            if (!parseOnOff(arg, &enabled)) {
                printUsage("verbose [0|1]");
                return;
            }
            g_verbose = enabled;
        }
        LOGI("Verbose mode: %s%s%s", onOffColor(g_verbose), g_verbose ? "ON" : "OFF", cli::kReset);
        return;
    }
    if (strcmp(cmd, "stress") == 0) {
        uint32_t count = 100;
        char* countTok = strtok(nullptr, " \t\r\n");
        if (countTok != nullptr && !parseU32(countTok, &count)) {
            printUsage("stress [N] [probe|read]");
            return;
        }
        char* mode = strtok(nullptr, " \t\r\n");
        if (mode != nullptr) toLowerInPlace(mode);
        if ((mode != nullptr) && (strcmp(mode, "read") == 0)) {
            runReadStress(count);
        } else {
            runProbeStress(count);
        }
        return;
    }
    if (strcmp(cmd, "selftest") == 0) {
        runSelfTest();
        return;
    }
    if (strcmp(cmd, "id") == 0) {
        Serial.printf("ID: %snot implemented in MAX31865 silicon%s\n", cli::kYellow, cli::kReset);
        Serial.println("  Probe uses a writable threshold-register readback and restore.");
        printStatus(rtd.probe());
        return;
    }
    if (strcmp(cmd, "comms") == 0) {
        uint8_t readback = 0;
        const bool ok = rtd.registerReadbackTest(&readback);
        Serial.printf("Comms write-readback: %s%s%s (readback=0x%02X)\n",
                      cli::okColor(ok), okText(ok), cli::kReset, readback);
        printLastStatus();
        return;
    }
    if (strcmp(cmd, "reset") == 0) {
        char* arg = strtok(nullptr, " \t\r\n");
        if (arg != nullptr) toLowerInPlace(arg);
        if ((arg != nullptr) && (strcmp(arg, "regs") != 0)) {
            printUsage("reset [regs]");
            return;
        }
        const bool ok = rtd.resetRegisters();
        Serial.printf("Reset registers: %s%s%s\n", cli::okColor(ok), okText(ok), cli::kReset);
        printLastStatus();
        return;
    }
    if ((strcmp(cmd, "lock") == 0) || (strcmp(cmd, "unlock") == 0)) {
        Serial.printf("%sRegister lock/unlock is not a MAX31865 feature.%s\n", cli::kYellow, cli::kReset);
        return;
    }
    if (strcmp(cmd, "cmd") == 0) {
        char* op = strtok(nullptr, " \t\r\n");
        if (op == nullptr) {
            printUsage("cmd <clear|faultauto|faultmanual|oneshot>");
            return;
        }
        toLowerInPlace(op);
        if (strcmp(op, "clear") == 0) {
            printResult("Command CLEAR_FAULT", rtd.clearFaults());
            printLastStatus();
        } else if (strcmp(op, "faultauto") == 0) {
            MAX31865FaultStatus fault{};
            printResult("Command FAULT_AUTO", rtd.runAutoFaultDetection(fault));
            printFaultDecoded(fault);
            printLastStatus();
        } else if (strcmp(op, "faultmanual") == 0) {
            MAX31865FaultStatus fault{};
            printResult("Command FAULT_MANUAL", rtd.runManualFaultDetection(fault, 1000U));
            printFaultDecoded(fault);
            printLastStatus();
        } else if ((strcmp(op, "oneshot") == 0) || (strcmp(op, "read") == 0)) {
            (void)readOne();
        } else {
            printUsage("cmd <clear|faultauto|faultmanual|oneshot>");
        }
        return;
    }
    if (strcmp(cmd, "regs") == 0) {
        printRegisterDump();
        return;
    }
    if (strcmp(cmd, "rreg") == 0) {
        uint32_t addr = 0;
        if (!parseU32(strtok(nullptr, " \t\r\n"), &addr) || addr > max31865_cmd::REG_LAST) {
            printUsage("rreg <0..7>");
            return;
        }
        uint8_t value = 0;
        const bool ok = rtd.readReg(static_cast<uint8_t>(addr), value);
        Serial.printf("RREG: %s%s%s  0x%02lX = 0x%02X\n",
                      cli::okColor(ok), okText(ok), cli::kReset,
                      static_cast<unsigned long>(addr), value);
        printLastStatus();
        return;
    }
    if (strcmp(cmd, "rregs") == 0) {
        uint32_t addr = 0;
        uint32_t len = 0;
        if (!parseU32(strtok(nullptr, " \t\r\n"), &addr) ||
            !parseU32(strtok(nullptr, " \t\r\n"), &len) ||
            addr > max31865_cmd::REG_LAST || len == 0U || len > 8U ||
            (addr + len - 1U) > max31865_cmd::REG_LAST) {
            printUsage("rregs <0..7> <1..8>");
            return;
        }
        uint8_t data[8] = {};
        const bool ok = rtd.readRegs(static_cast<uint8_t>(addr), data, len);
        Serial.printf("RREGS: %s%s%s\n", cli::okColor(ok), okText(ok), cli::kReset);
        if (ok) {
            for (uint32_t i = 0; i < len; ++i) {
                Serial.printf("  0x%02lX = 0x%02X\n", static_cast<unsigned long>(addr + i), data[i]);
            }
        }
        printLastStatus();
        return;
    }
    if ((strcmp(cmd, "wreg") == 0) || (strcmp(cmd, "wregv") == 0)) {
        uint32_t addr = 0;
        uint32_t value = 0;
        if (!parseU32(strtok(nullptr, " \t\r\n"), &addr) ||
            !parseU32(strtok(nullptr, " \t\r\n"), &value) ||
            addr > max31865_cmd::REG_LAST || value > 0xFFU) {
            printUsage("wreg <addr> <0..255>");
            return;
        }
        uint8_t rb = 0;
        const bool verify = (strcmp(cmd, "wregv") == 0);
        const bool ok = verify
                            ? rtd.writeRegVerify(static_cast<uint8_t>(addr), static_cast<uint8_t>(value), &rb)
                            : rtd.writeReg(static_cast<uint8_t>(addr), static_cast<uint8_t>(value));
        Serial.printf("%s: %s%s%s", verify ? "WREGV" : "WREG", cli::okColor(ok), okText(ok), cli::kReset);
        if (verify) Serial.printf(" (readback=0x%02X)", rb);
        Serial.println();
        printLastStatus();
        return;
    }
    if ((strcmp(cmd, "cfg") == 0) || (strcmp(cmd, "settings") == 0) || (strcmp(cmd, "getcfg") == 0)) {
        printSettings();
        return;
    }
    if (strcmp(cmd, "clearhealth") == 0) {
        rtd.clearHealthCounters();
        Serial.printf("Health counters: %scleared%s\n", cli::kGreen, cli::kReset);
        return;
    }
    if (strcmp(cmd, "spihz") == 0) {
        uint32_t hz = 0;
        char* arg = strtok(nullptr, " \t\r\n");
        if (arg == nullptr) {
            Serial.printf("SPI Hz: %lu\n", static_cast<unsigned long>(rtd.spiHz()));
            return;
        }
        if (!parseU32(arg, &hz)) {
            printUsage("spihz [hz]");
            return;
        }
        rtd.setSpiHz(hz);
        g_spi_hz = rtd.spiHz();
        Serial.printf("SPI Hz set to %lu\n", static_cast<unsigned long>(g_spi_hz));
        return;
    }
    if (strcmp(cmd, "lockms") == 0) {
        uint32_t ms = 0;
        char* arg = strtok(nullptr, " \t\r\n");
        if (arg == nullptr) {
            Serial.printf("SPI lock timeout: %lu ms\n", static_cast<unsigned long>(rtd.spiLockTimeoutMs()));
            return;
        }
        if (!parseU32(arg, &ms)) {
            printUsage("lockms [ms]");
            return;
        }
        rtd.setSpiLockTimeoutMs(ms);
        Serial.printf("SPI lock timeout: %lu ms\n", static_cast<unsigned long>(rtd.spiLockTimeoutMs()));
        return;
    }
    if (strcmp(cmd, "start") == 0) {
        const bool ok = rtd.startContinuous();
        Serial.printf("Continuous start: %s%s%s\n", cli::okColor(ok), okText(ok), cli::kReset);
        printLastStatus();
        return;
    }
    if (strcmp(cmd, "stop") == 0) {
        const bool ok = rtd.stop();
        Serial.printf("Continuous stop: %s%s%s\n", cli::okColor(ok), okText(ok), cli::kReset);
        printLastStatus();
        return;
    }
    if (strcmp(cmd, "stream") == 0) {
        char* arg = strtok(nullptr, " \t\r\n");
        if (arg == nullptr) {
            Serial.printf("stream is %s%s%s interval=%lu ms\n",
                          onOffColor(g_stream_enabled),
                          g_stream_enabled ? "on" : "off",
                          cli::kReset,
                          static_cast<unsigned long>(g_stream_interval_ms));
            return;
        }
        bool enabled = false;
        uint32_t interval = 0;
        if (parseOnOff(arg, &enabled)) {
            g_stream_enabled = enabled;
        } else if (parseU32(arg, &interval) && interval > 0U) {
            g_stream_interval_ms = interval;
            g_stream_enabled = true;
        } else {
            printUsage("stream [on|off|ms]");
            return;
        }
        Serial.printf("stream %s%s%s interval=%lu ms\n",
                      onOffColor(g_stream_enabled),
                      g_stream_enabled ? "on" : "off",
                      cli::kReset,
                      static_cast<unsigned long>(g_stream_interval_ms));
        return;
    }
    if (strcmp(cmd, "sample") == 0) {
        uint32_t count = 1;
        char* arg = strtok(nullptr, " \t\r\n");
        if (arg != nullptr && !parseU32(arg, &count)) {
            printUsage("sample [N]");
            return;
        }
        uint32_t printed = 0;
        for (uint32_t i = 0; i < count; ++i) {
            if (readOne()) printed++;
        }
        Serial.printf("Printed %lu sample(s)\n", static_cast<unsigned long>(printed));
        return;
    }
    if (strcmp(cmd, "sps") == 0) {
        uint32_t ms = 5000;
        char* arg = strtok(nullptr, " \t\r\n");
        if (arg != nullptr && !parseU32(arg, &ms)) {
            printUsage("sps [ms]");
            return;
        }
        runSps(ms);
        return;
    }
    if (strcmp(cmd, "temp") == 0) {
        MAX31865Sample sample{};
        if (!rtd.readSingle(sample)) {
            Serial.printf("Temp: %sread failed%s\n", cli::kRed, cli::kReset);
            printLastStatus();
        } else {
            Serial.printf("Temp: %.4f C  R=%.5f ohm  code=%u\n",
                          sample.temperature_c,
                          sample.resistance_ohms,
                          sample.raw.code);
        }
        return;
    }
    if (strcmp(cmd, "stats") == 0) {
        printStats();
        return;
    }
    if (strcmp(cmd, "fault") == 0) {
        printFault();
        return;
    }
    if (strcmp(cmd, "clear") == 0) {
        const bool ok = rtd.clearFaults();
        Serial.printf("Clear faults: %s%s%s\n", cli::okColor(ok), okText(ok), cli::kReset);
        printLastStatus();
        return;
    }
    if (strcmp(cmd, "faultauto") == 0) {
        MAX31865FaultStatus fault{};
        const bool ok = rtd.runAutoFaultDetection(fault);
        Serial.printf("Automatic fault cycle: %s%s%s\n", cli::okColor(ok), okText(ok), cli::kReset);
        printFaultDecoded(fault);
        printLastStatus();
        return;
    }
    if (strcmp(cmd, "faultmanual") == 0) {
        uint32_t us = 1000;
        char* arg = strtok(nullptr, " \t\r\n");
        if (arg != nullptr && !parseU32(arg, &us)) {
            printUsage("faultmanual [settle_us]");
            return;
        }
        MAX31865FaultStatus fault{};
        const bool ok = rtd.runManualFaultDetection(fault, us);
        Serial.printf("Manual fault cycle: %s%s%s\n", cli::okColor(ok), okText(ok), cli::kReset);
        printFaultDecoded(fault);
        printLastStatus();
        return;
    }
    if (strcmp(cmd, "threshold") == 0) {
        handleThresholdCommand(strtok(nullptr, " \t\r\n"));
        return;
    }
    if (strcmp(cmd, "filter") == 0) {
        char* arg = strtok(nullptr, " \t\r\n");
        MAX31865Filter filter = MAX31865Filter::Hz60;
        if (arg == nullptr) {
            Serial.printf("Filter: %s\n", filterName(rtd.filter()));
            return;
        }
        if (!parseFilter(arg, &filter)) {
            printUsage("filter [50|60]");
            return;
        }
        const MAX31865Filter before = rtd.filter();
        const bool ok = rtd.setFilter(filter);
        Serial.printf("Filter: %s%s%s  %s -> %s\n",
                      cli::okColor(ok), okText(ok), cli::kReset,
                      filterName(before), filterName(rtd.filter()));
        printLastStatus();
        return;
    }
    if (strcmp(cmd, "wires") == 0) {
        char* arg = strtok(nullptr, " \t\r\n");
        MAX31865WireMode mode = MAX31865WireMode::FourWire;
        if (arg == nullptr) {
            Serial.printf("Wires: %s\n", wireModeName(rtd.wireMode()));
            return;
        }
        if (!parseWireMode(arg, &mode)) {
            printUsage("wires [2|3|4]");
            return;
        }
        const MAX31865WireMode before = rtd.wireMode();
        const bool ok = rtd.setWireMode(mode);
        Serial.printf("Wires: %s%s%s  %s -> %s\n",
                      cli::okColor(ok), okText(ok), cli::kReset,
                      wireModeName(before), wireModeName(rtd.wireMode()));
        printLastStatus();
        return;
    }
    if (strcmp(cmd, "bias") == 0) {
        char* arg = strtok(nullptr, " \t\r\n");
        bool enabled = false;
        if (arg == nullptr) {
            Serial.printf("VBIAS: %s%s%s\n", onOffColor(rtd.biasEnabled()), rtd.biasEnabled() ? "on" : "off", cli::kReset);
            return;
        }
        if (!parseOnOff(arg, &enabled)) {
            printUsage("bias [on|off]");
            return;
        }
        const bool ok = rtd.setBias(enabled);
        Serial.printf("VBIAS: %s%s%s\n", cli::okColor(ok), okText(ok), cli::kReset);
        printLastStatus();
        return;
    }
    if (strcmp(cmd, "auto") == 0) {
        char* arg = strtok(nullptr, " \t\r\n");
        bool enabled = false;
        if (arg == nullptr) {
            Serial.printf("Auto conversion: %s%s%s\n", onOffColor(rtd.autoConvertEnabled()), rtd.autoConvertEnabled() ? "on" : "off", cli::kReset);
            return;
        }
        if (!parseOnOff(arg, &enabled)) {
            printUsage("auto [on|off]");
            return;
        }
        const bool ok = rtd.setAutoConvert(enabled);
        Serial.printf("Auto conversion: %s%s%s\n", cli::okColor(ok), okText(ok), cli::kReset);
        printLastStatus();
        return;
    }
    if (strcmp(cmd, "rtd") == 0) {
        handleRtdCommand(strtok(nullptr, " \t\r\n"));
        return;
    }
    if (strcmp(cmd, "scale") == 0) {
        handleScaleCommand(strtok(nullptr, " \t\r\n"));
        return;
    }

    cli::printUnknownCommand(cmd);
}

void setup() {
    board::initSerial();
    log_begin(board::SERIAL_BAUD);
    LOGI("=== MAX31865 Unified Bringup Example ===");
    printVersion();

    MAX31865Pins pins = board::rtdPins();
    MAX31865BeginConfig config{};
    config.spi = &SPI;
    config.pins = pins;
    config.spiHz = g_spi_hz;
    config.verifyProbe = false;
    config.referenceResistorOhms = 400.0f;
    config.rtdNominalOhms = 100.0f;
    config.inputFilterTimeConstantUs = 1000U;
    config.wireMode = MAX31865WireMode::FourWire;
    config.filter = MAX31865Filter::Hz60;

    if (!rtd.begin(config)) {
        LOGE("MAX31865 begin failed: %s", rtd.lastErrorName());
        printDetailedHealth();
    } else {
        Serial.printf("Begin: %sOK%s\n", cli::kGreen, cli::kReset);
        printSettings();
    }
    printHelp();
    cli::printPrompt();
}

void loop() {
    while (Serial.available() > 0) {
        const char c = static_cast<char>(Serial.read());
        if (c == '\r') continue;
        if (c == '\n') {
            g_line[g_line_len] = '\0';
            handleCommand(g_line);
            g_line_len = 0;
            cli::printPrompt();
            continue;
        }
        if (g_line_len + 1U < sizeof(g_line)) {
            g_line[g_line_len++] = c;
        }
    }

    if (g_stream_enabled) {
        static uint32_t lastMs = 0;
        if ((millis() - lastMs) >= g_stream_interval_ms) {
            lastMs = millis();
            (void)readOne();
        }
    }
}
