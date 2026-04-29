#include <Arduino.h>
#include <SPI.h>
#include <MAX31865.h>
#include <MAX31865/Version.h>
#include <ctype.h>
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

static char g_line[160];
static size_t g_line_len = 0;
static bool g_stream_enabled = false;
static bool g_verbose = true;
static uint32_t g_spi_hz = DEFAULT_SPI_HZ;

static const char* goodIfZeroColor(uint32_t value) {
    return cli::zeroGoodColor(value);
}

static const char* goodIfNonZeroColor(uint32_t value) {
    return cli::nonZeroGoodColor(value);
}

static const char* successRateColor(float pct) {
    return cli::successRateColor(pct);
}

static uint32_t stressProgressStep(uint32_t total) {
    if (total == 0U) return 0U;
    const uint32_t step = total / STRESS_PROGRESS_UPDATES;
    return (step == 0U) ? 1U : step;
}

static void printStressProgress(uint32_t completed, uint32_t total, uint32_t okCount, uint32_t failCount) {
    if ((completed == 0U) || (total == 0U)) return;
    const uint32_t step = stressProgressStep(total);
    if ((step == 0U) || ((completed != total) && ((completed % step) != 0U))) return;
    const float pct = (100.0f * static_cast<float>(completed)) / static_cast<float>(total);
    Serial.printf("  Progress: %lu/%lu (%s%.0f%%%s, ok=%s%lu%s, fail=%s%lu%s)\n",
                  static_cast<unsigned long>(completed),
                  static_cast<unsigned long>(total),
                  successRateColor(pct),
                  pct,
                  LOG_COLOR_RESET,
                  goodIfNonZeroColor(okCount),
                  static_cast<unsigned long>(okCount),
                  LOG_COLOR_RESET,
                  goodIfZeroColor(failCount),
                  static_cast<unsigned long>(failCount),
                  LOG_COLOR_RESET);
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
    float value = strtof(token, &end);
    if ((end == token) || (*end != '\0')) return false;
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

static void printHelp() {
    Serial.println();
    cli::printHelpHeader("MAX31865 Unified Bringup CLI");
    cli::printHelpSection("Common");
    cli::printHelpItem("help / ?", "Show this help");
    cli::printHelpItem("version / ver", "Print firmware and library version info");
    cli::printHelpItem("scan", "Print SPI wiring, pin levels, and raw probe");
    cli::printHelpItem("probe", "Probe config register without health side effects");
    cli::printHelpItem("recover", "Tracked recover with config restore");
    cli::printHelpItem("drv / health", "Detailed driver health");
    cli::printHelpItem("state", "Compact one-line health summary");
    cli::printHelpItem("read", "Read one one-shot sample");
    cli::printHelpItem("verbose [0|1]", "Get/set verbose mode");
    cli::printHelpItem("stress [N]", "Run N one-shot read iterations");
    cli::printHelpItem("selftest", "Run safe command self-test report");
    cli::printHelpItem("regs", "Dump all registers");
    cli::printHelpItem("rreg <addr>", "Read one register");
    cli::printHelpItem("rregs <addr> <len>", "Read register range");
    cli::printHelpItem("wreg <addr> <value>", "Write one writable register");
    cli::printHelpItem("wregv <addr> <value>", "Write and verify one register");
    cli::printHelpItem("cfg / settings", "Decode key MAX31865 configuration");
    Serial.println();
    cli::printHelpSection("Measurement");
    cli::printHelpItem("start", "Enable continuous conversion");
    cli::printHelpItem("stop", "Stop continuous conversion");
    cli::printHelpItem("stream [on|off]", "Toggle repeated read output in loop");
    cli::printHelpItem("sample [N]", "Read N one-shot samples");
    cli::printHelpItem("stats", "Print read/health counters");
    cli::printHelpItem("fault", "Read decoded fault status");
    cli::printHelpItem("clear", "Clear fault latch");
    cli::printHelpItem("faultauto", "Run automatic fault-detection cycle");
    cli::printHelpItem("faultmanual [us]", "Run manual fault cycle with settle delay");
    cli::printHelpItem("threshold", "Print threshold raw codes and resistance");
    cli::printHelpItem("threshold raw <low> <high>", "Set threshold ADC codes");
    cli::printHelpItem("threshold ohm <low> <high>", "Set threshold resistance");
    cli::printHelpItem("threshold temp <low> <high>", "Set threshold Celsius");
    cli::printHelpSection("Config");
    cli::printHelpItem("spihz [hz]", "Get/set SPI clock");
    cli::printHelpItem("filter [50|60]", "Get/set digital notch filter");
    cli::printHelpItem("wires [2|3|4]", "Get/set RTD wiring mode");
    cli::printHelpItem("bias [on|off]", "Get/set VBIAS");
    cli::printHelpItem("auto [on|off]", "Get/set continuous conversion");
    cli::printHelpItem("scale [code]", "Show code ratio/resistance/temperature");
}

static void printVersion() {
    Serial.printf("MAX31865 library: %s\n", MAX31865Version::VERSION_FULL);
    Serial.printf("Build: %s %s, git=%s %s\n",
                  MAX31865Version::BUILD_DATE,
                  MAX31865Version::BUILD_TIME,
                  MAX31865Version::GIT_COMMIT,
                  MAX31865Version::GIT_STATUS);
}

static void printScan() {
    MAX31865Pins pins = board::rtdPins();
    transport_adapter::SpiPins spiPins{pins.sck, pins.miso, pins.mosi, pins.cs, pins.drdy, -1, -1, -1};
    transport_adapter::printPins(spiPins);
    bus_diag::printDrdyLevel(pins.drdy);
    printStatus(rtd.probe());
}

static void printConfig() {
    uint8_t config = rtd.readReg(max31865_cmd::REG_CONFIG);
    MAX31865Health health = rtd.health();
    Serial.printf("CONFIG=0x%02X bias=%s auto=%s wire=%u filter=%s state=%s err=%s\n",
                  config,
                  (config & max31865_cmd::CONFIG_BIAS) ? "on" : "off",
                  (config & max31865_cmd::CONFIG_AUTO) ? "on" : "off",
                  (config & max31865_cmd::CONFIG_3WIRE) ? 3U : 4U,
                  (config & max31865_cmd::CONFIG_FILTER_50HZ) ? "50Hz" : "60Hz",
                  max31865StateName(health.state),
                  rtd.lastErrorName());
}

static void printSample(const MAX31865Sample& sample) {
    Serial.printf("sample=%lu code=%u raw=0x%04X R=%.5f ohm T=%.4f C fault=%s\n",
                  static_cast<unsigned long>(sample.sample_counter),
                  sample.raw.code,
                  sample.raw.raw_register,
                  sample.resistance_ohms,
                  sample.temperature_c,
                  sample.raw.fault ? "yes" : "no");
    if (sample.has_fault_status) {
        Serial.printf("  fault=0x%02X high=%u low=%u refinHigh=%u refinLow=%u rtdinLow=%u ovuv=%u\n",
                      sample.fault_status.raw,
                      sample.fault_status.high_threshold ? 1U : 0U,
                      sample.fault_status.low_threshold ? 1U : 0U,
                      sample.fault_status.refin_high ? 1U : 0U,
                      sample.fault_status.refin_low ? 1U : 0U,
                      sample.fault_status.rtdin_low ? 1U : 0U,
                      sample.fault_status.over_under_voltage ? 1U : 0U);
    }
}

static bool readOne() {
    HealthSnapshot before;
    before.capture(rtd);
    MAX31865Sample sample{};
    const bool ok = rtd.readSingle(sample);
    if (ok) printSample(sample);
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
    if (!ok) return;
    Serial.printf("FAULT=0x%02X high=%u low=%u refinHigh=%u refinLow=%u rtdinLow=%u ovuv=%u\n",
                  fault.raw,
                  fault.high_threshold ? 1U : 0U,
                  fault.low_threshold ? 1U : 0U,
                  fault.refin_high ? 1U : 0U,
                  fault.refin_low ? 1U : 0U,
                  fault.rtdin_low ? 1U : 0U,
                  fault.over_under_voltage ? 1U : 0U);
}

static void dumpRegisters() {
    MAX31865RegisterDump rows[8];
    const size_t n = rtd.dumpRegisters(rows, 8);
    for (size_t i = 0; i < n; ++i) {
        Serial.printf("0x%02X %-14s 0x%02X\n", rows[i].addr, rows[i].name, rows[i].value);
    }
}

static void printThresholds() {
    MAX31865FaultThresholds thresholds{};
    if (!rtd.getFaultThresholdsRaw(thresholds)) {
        printLastStatus();
        return;
    }
    Serial.printf("low=%u (%.5f ohm) high=%u (%.5f ohm)\n",
                  thresholds.low_code,
                  rtd.codeToResistance(thresholds.low_code),
                  thresholds.high_code,
                  rtd.codeToResistance(thresholds.high_code));
}

static void runStress(uint32_t count) {
    uint32_t okCount = 0;
    uint32_t failCount = 0;
    for (uint32_t i = 0; i < count; ++i) {
        if (readOne()) okCount++; else failCount++;
        printStressProgress(i + 1U, count, okCount, failCount);
    }
    Serial.printf("Stress done: ok=%lu fail=%lu\n",
                  static_cast<unsigned long>(okCount),
                  static_cast<unsigned long>(failCount));
}

static void runSelfTest() {
    Serial.println("Self-test:");
    printStatus(rtd.probe());
    uint8_t config = rtd.readReg(max31865_cmd::REG_CONFIG);
    Serial.printf("  CONFIG read: 0x%02X\n", config);
    MAX31865FaultStatus fault{};
    (void)rtd.readFaultStatus(fault);
    Serial.printf("  Fault register: 0x%02X\n", fault.raw);
    Serial.printf("  lastErrorName: %s\n", rtd.lastErrorName());
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
        Serial.println("Usage: threshold raw|ohm|temp <low> <high>");
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
    }
    printLastStatus();
    if (ok) printThresholds();
}

static void handleCommand(char* line) {
    char* cmd = strtok(line, " \t\r\n");
    if (cmd == nullptr) return;
    toLowerInPlace(cmd);

    if ((strcmp(cmd, "help") == 0) || (strcmp(cmd, "?") == 0)) {
        printHelp();
    } else if ((strcmp(cmd, "version") == 0) || (strcmp(cmd, "ver") == 0)) {
        printVersion();
    } else if (strcmp(cmd, "scan") == 0) {
        printScan();
    } else if (strcmp(cmd, "probe") == 0) {
        printStatus(rtd.probe());
    } else if (strcmp(cmd, "recover") == 0) {
        printStatus(rtd.recover());
    } else if (strcmp(cmd, "state") == 0) {
        health_view::printOneLine(rtd);
    } else if ((strcmp(cmd, "drv") == 0) || (strcmp(cmd, "health") == 0)) {
        printHealthView(rtd);
    } else if (strcmp(cmd, "read") == 0) {
        (void)readOne();
    } else if (strcmp(cmd, "verbose") == 0) {
        char* arg = strtok(nullptr, " \t\r\n");
        bool enabled = false;
        if (arg != nullptr && parseOnOff(arg, &enabled)) {
            g_verbose = enabled;
        }
        Serial.printf("verbose=%s\n", g_verbose ? "on" : "off");
    } else if (strcmp(cmd, "stress") == 0) {
        uint32_t count = 10;
        char* arg = strtok(nullptr, " \t\r\n");
        if (arg != nullptr) (void)parseU32(arg, &count);
        runStress(count);
    } else if (strcmp(cmd, "selftest") == 0) {
        runSelfTest();
    } else if (strcmp(cmd, "regs") == 0) {
        dumpRegisters();
    } else if (strcmp(cmd, "rreg") == 0) {
        uint32_t addr = 0;
        if (parseU32(strtok(nullptr, " \t\r\n"), &addr)) {
            Serial.printf("0x%02lX = 0x%02X\n", static_cast<unsigned long>(addr), rtd.readReg(static_cast<uint8_t>(addr)));
        }
    } else if (strcmp(cmd, "rregs") == 0) {
        uint32_t addr = 0;
        uint32_t len = 0;
        if (parseU32(strtok(nullptr, " \t\r\n"), &addr) && parseU32(strtok(nullptr, " \t\r\n"), &len) && len <= 8U) {
            uint8_t data[8] = {};
            if (rtd.readRegs(static_cast<uint8_t>(addr), data, len)) {
                for (uint32_t i = 0; i < len; ++i) Serial.printf("0x%02lX: 0x%02X\n", static_cast<unsigned long>(addr + i), data[i]);
            }
        }
        printLastStatus();
    } else if ((strcmp(cmd, "wreg") == 0) || (strcmp(cmd, "wregv") == 0)) {
        uint32_t addr = 0;
        uint32_t value = 0;
        if (parseU32(strtok(nullptr, " \t\r\n"), &addr) && parseU32(strtok(nullptr, " \t\r\n"), &value)) {
            uint8_t rb = 0;
            bool ok = (strcmp(cmd, "wregv") == 0)
                          ? rtd.writeRegVerify(static_cast<uint8_t>(addr), static_cast<uint8_t>(value), &rb)
                          : rtd.writeReg(static_cast<uint8_t>(addr), static_cast<uint8_t>(value));
            Serial.printf("write %s readback=0x%02X\n", ok ? "ok" : "failed", rb);
        }
        printLastStatus();
    } else if ((strcmp(cmd, "cfg") == 0) || (strcmp(cmd, "settings") == 0)) {
        printConfig();
    } else if (strcmp(cmd, "spihz") == 0) {
        uint32_t hz = 0;
        char* arg = strtok(nullptr, " \t\r\n");
        if (arg != nullptr && parseU32(arg, &hz)) {
            rtd.setSpiHz(hz);
            g_spi_hz = hz;
        }
        Serial.printf("SPI Hz: %lu\n", static_cast<unsigned long>(g_spi_hz));
    } else if (strcmp(cmd, "start") == 0) {
        Serial.printf("start=%s\n", rtd.startContinuous() ? "ok" : "failed");
        printLastStatus();
    } else if (strcmp(cmd, "stop") == 0) {
        Serial.printf("stop=%s\n", rtd.stop() ? "ok" : "failed");
        printLastStatus();
    } else if (strcmp(cmd, "stream") == 0) {
        char* arg = strtok(nullptr, " \t\r\n");
        bool enabled = false;
        if (arg != nullptr && parseOnOff(arg, &enabled)) g_stream_enabled = enabled;
        Serial.printf("stream=%s\n", g_stream_enabled ? "on" : "off");
    } else if (strcmp(cmd, "sample") == 0) {
        uint32_t count = 1;
        char* arg = strtok(nullptr, " \t\r\n");
        if (arg != nullptr) (void)parseU32(arg, &count);
        for (uint32_t i = 0; i < count; ++i) (void)readOne();
    } else if (strcmp(cmd, "stats") == 0) {
        printHealthView(rtd);
    } else if (strcmp(cmd, "fault") == 0) {
        printFault();
    } else if (strcmp(cmd, "clear") == 0) {
        Serial.printf("clear=%s\n", rtd.clearFaults() ? "ok" : "failed");
        printLastStatus();
    } else if (strcmp(cmd, "faultauto") == 0) {
        MAX31865FaultStatus fault{};
        Serial.printf("faultauto=%s\n", rtd.runAutoFaultDetection(fault) ? "ok" : "failed");
        printFault();
    } else if (strcmp(cmd, "faultmanual") == 0) {
        uint32_t us = 1000;
        char* arg = strtok(nullptr, " \t\r\n");
        if (arg != nullptr) (void)parseU32(arg, &us);
        MAX31865FaultStatus fault{};
        Serial.printf("faultmanual=%s\n", rtd.runManualFaultDetection(fault, us) ? "ok" : "failed");
        printFault();
    } else if (strcmp(cmd, "threshold") == 0) {
        handleThresholdCommand(strtok(nullptr, " \t\r\n"));
    } else if (strcmp(cmd, "filter") == 0) {
        char* arg = strtok(nullptr, " \t\r\n");
        if (arg == nullptr) {
            printConfig();
        } else if (strcmp(arg, "50") == 0) {
            Serial.printf("filter=%s\n", rtd.setFilter(MAX31865Filter::Hz50) ? "ok" : "failed");
        } else if (strcmp(arg, "60") == 0) {
            Serial.printf("filter=%s\n", rtd.setFilter(MAX31865Filter::Hz60) ? "ok" : "failed");
        }
        printLastStatus();
    } else if (strcmp(cmd, "wires") == 0) {
        uint32_t wires = 0;
        char* arg = strtok(nullptr, " \t\r\n");
        if (arg != nullptr && parseU32(arg, &wires)) {
            MAX31865WireMode mode = (wires == 3U) ? MAX31865WireMode::ThreeWire :
                                    (wires == 2U) ? MAX31865WireMode::TwoWire :
                                                     MAX31865WireMode::FourWire;
            Serial.printf("wires=%s\n", rtd.setWireMode(mode) ? "ok" : "failed");
        } else {
            printConfig();
        }
        printLastStatus();
    } else if (strcmp(cmd, "bias") == 0) {
        bool enabled = false;
        char* arg = strtok(nullptr, " \t\r\n");
        if (arg != nullptr && parseOnOff(arg, &enabled)) {
            Serial.printf("bias=%s\n", rtd.setBias(enabled) ? "ok" : "failed");
        } else {
            printConfig();
        }
        printLastStatus();
    } else if (strcmp(cmd, "auto") == 0) {
        bool enabled = false;
        char* arg = strtok(nullptr, " \t\r\n");
        if (arg != nullptr && parseOnOff(arg, &enabled)) {
            Serial.printf("auto=%s\n", rtd.setAutoConvert(enabled) ? "ok" : "failed");
        } else {
            printConfig();
        }
        printLastStatus();
    } else if (strcmp(cmd, "scale") == 0) {
        uint32_t code = 8192;
        char* arg = strtok(nullptr, " \t\r\n");
        if (arg != nullptr) (void)parseU32(arg, &code);
        Serial.printf("code=%lu ratio=%.8f R=%.5f ohm T=%.4f C\n",
                      static_cast<unsigned long>(code),
                      MAX31865::codeToRatio(static_cast<uint16_t>(code)),
                      rtd.codeToResistance(static_cast<uint16_t>(code)),
                      rtd.resistanceToTemperature(rtd.codeToResistance(static_cast<uint16_t>(code))));
    } else {
        cli::printUnknownCommand(cmd);
    }
}

void setup() {
    board::initSerial();
    log_begin(board::SERIAL_BAUD);
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
        if ((millis() - lastMs) >= 1000U) {
            lastMs = millis();
            (void)readOne();
        }
    }
}
