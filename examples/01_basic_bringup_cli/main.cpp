#include <Arduino.h>
#include <SPI.h>

#include <MAX31865/MAX31865.h>

#include "examples/common/BoardConfig.h"
#include "examples/common/BuildConfig.h"
#include "examples/common/CliStyle.h"

namespace {

struct SpiContext {
  SPIClass* spi = &SPI;
  SPISettings settings{1000000UL, MSBFIRST, SPI_MODE1};
  int csPin = -1;
};

SpiContext g_spi;
MAX31865::MAX31865 g_rtd;
MAX31865::Config g_cfg;
char g_line[96] = {};
size_t g_lineLen = 0;

MAX31865::Status spiTransfer(const uint8_t* tx, uint8_t* rx, size_t len,
                             uint32_t, void* user) {
  SpiContext* ctx = static_cast<SpiContext*>(user);
  if (ctx == nullptr || ctx->spi == nullptr || ctx->csPin < 0 ||
      tx == nullptr || rx == nullptr || len == 0) {
    return MAX31865::Status::Error(MAX31865::Err::INVALID_PARAM, "Bad SPI adapter args");
  }

  ctx->spi->beginTransaction(ctx->settings);
  digitalWrite(ctx->csPin, LOW);
  for (size_t i = 0; i < len; ++i) {
    rx[i] = ctx->spi->transfer(tx[i]);
  }
  digitalWrite(ctx->csPin, HIGH);
  ctx->spi->endTransaction();
  return MAX31865::Status::Ok();
}

uint32_t nowMs(void*) {
  return millis();
}

void delayMs(uint32_t ms, void*) {
  delay(ms);
}

void delayUs(uint32_t us, void*) {
  delayMicroseconds(us);
}

void yieldHook(void*) {
  yield();
}

bool readGpio(int pin, void*) {
  return digitalRead(pin) != LOW;
}

const char* statusName(MAX31865::Err err) {
  switch (err) {
    case MAX31865::Err::OK: return "OK";
    case MAX31865::Err::NOT_INITIALIZED: return "NOT_INITIALIZED";
    case MAX31865::Err::INVALID_CONFIG: return "INVALID_CONFIG";
    case MAX31865::Err::SPI_ERROR: return "SPI_ERROR";
    case MAX31865::Err::SPI_TIMEOUT: return "SPI_TIMEOUT";
    case MAX31865::Err::TIMEOUT: return "TIMEOUT";
    case MAX31865::Err::INVALID_PARAM: return "INVALID_PARAM";
    case MAX31865::Err::DEVICE_NOT_FOUND: return "DEVICE_NOT_FOUND";
    case MAX31865::Err::CONVERSION_NOT_READY: return "CONVERSION_NOT_READY";
    case MAX31865::Err::BUSY: return "BUSY";
    case MAX31865::Err::IN_PROGRESS: return "IN_PROGRESS";
    case MAX31865::Err::FAULT_PRESENT: return "FAULT_PRESENT";
    default: return "UNKNOWN";
  }
}

void printStatus(const MAX31865::Status& st) {
  Serial.printf("%s%s%s detail=%ld %s\n",
                st.ok() ? cli::kGreen : cli::kRed,
                statusName(st.code),
                cli::kReset,
                static_cast<long>(st.detail),
                st.msg);
}

void printHelp() {
  Serial.println("Commands:");
  Serial.println("  help                 Show this help");
  Serial.println("  probe                Read config register and check response");
  Serial.println("  recover              Re-apply cached configuration");
  Serial.println("  drv                  Print driver health");
  Serial.println("  cfg                  Print decoded configuration");
  Serial.println("  read                 One-shot temperature read");
  Serial.println("  fault                Read decoded fault status");
  Serial.println("  clear                Clear fault latch");
  Serial.println("  threshold            Print raw threshold codes");
  Serial.println("  stress <n>           Run n one-shot reads");
  Serial.println("  filter 50|60         Set notch filter");
  Serial.println("  wires 2|3|4          Set RTD wire mode");
  Serial.println("  auto 0|1             Disable/enable continuous conversion");
}

void printDriver() {
  MAX31865::SettingsSnapshot snap;
  printStatus(g_rtd.getSettings(snap));
  Serial.printf("state=%u online=%u ok=%lu fail=%lu consecutive=%u last=%s\n",
                static_cast<unsigned>(snap.state),
                g_rtd.isOnline() ? 1U : 0U,
                static_cast<unsigned long>(snap.totalSuccess),
                static_cast<unsigned long>(snap.totalFailures),
                snap.consecutiveFailures,
                statusName(snap.lastError.code));
  Serial.printf("rref=%.3f r0=%.3f wire=%u filter=%s bias=%u auto=%u\n",
                snap.referenceResistorOhms,
                snap.rtdNominalOhms,
                static_cast<unsigned>(snap.wireMode),
                snap.filter == MAX31865::Filter::HZ_50 ? "50Hz" : "60Hz",
                snap.enableBias ? 1U : 0U,
                snap.autoConvert ? 1U : 0U);
}

void printConfig() {
  MAX31865::ConfigurationInfo info;
  MAX31865::Status st = g_rtd.readConfiguration(info);
  printStatus(st);
  if (!st.ok()) {
    return;
  }
  Serial.printf("CONFIG=0x%02X bias=%u auto=%u oneshot=%u wire=%u faultCycle=%u filter=%s\n",
                info.raw,
                info.biasEnabled ? 1U : 0U,
                info.autoConvert ? 1U : 0U,
                info.oneShot ? 1U : 0U,
                static_cast<unsigned>(info.wireMode),
                static_cast<unsigned>(info.faultCycle),
                info.filter == MAX31865::Filter::HZ_50 ? "50Hz" : "60Hz");
}

void printFault() {
  MAX31865::FaultStatus fault;
  MAX31865::Status st = g_rtd.readFaultStatus(fault);
  printStatus(st);
  if (!st.ok()) {
    return;
  }
  Serial.printf("FAULT=0x%02X high=%u low=%u refinHigh=%u refinLow=%u rtdinLow=%u ovuv=%u\n",
                fault.raw,
                fault.highThreshold ? 1U : 0U,
                fault.lowThreshold ? 1U : 0U,
                fault.refInHigh ? 1U : 0U,
                fault.refInLow ? 1U : 0U,
                fault.rtdInLow ? 1U : 0U,
                fault.overUnderVoltage ? 1U : 0U);
}

void readOnce() {
  MAX31865::Reading sample;
  MAX31865::Status st = g_rtd.readBlocking(sample);
  printStatus(st);
  if (!st.ok()) {
    return;
  }
  Serial.printf("code=%u raw=0x%04X R=%.5f ohm T=%.4f C fault=%u\n",
                sample.raw.code,
                sample.raw.rawRegister,
                sample.resistanceOhms,
                sample.temperatureC,
                sample.raw.fault ? 1U : 0U);
  if (sample.hasFaultStatus) {
    printFault();
  }
}

void printThresholds() {
  MAX31865::FaultThresholds thresholds;
  MAX31865::Status st = g_rtd.getFaultThresholdsRaw(thresholds);
  printStatus(st);
  if (st.ok()) {
    Serial.printf("low=%u high=%u\n", thresholds.lowCode, thresholds.highCode);
  }
}

void runStress(uint32_t count) {
  for (uint32_t i = 0; i < count; ++i) {
    Serial.printf("[%lu/%lu] ", static_cast<unsigned long>(i + 1),
                  static_cast<unsigned long>(count));
    readOnce();
  }
}

void handleLine(char* line) {
  char* cmd = strtok(line, " \t\r\n");
  if (cmd == nullptr) {
    return;
  }

  if (strcmp(cmd, "help") == 0) {
    printHelp();
  } else if (strcmp(cmd, "probe") == 0) {
    printStatus(g_rtd.probe());
  } else if (strcmp(cmd, "recover") == 0) {
    printStatus(g_rtd.recover());
  } else if (strcmp(cmd, "drv") == 0) {
    printDriver();
  } else if (strcmp(cmd, "cfg") == 0) {
    printConfig();
  } else if (strcmp(cmd, "read") == 0) {
    readOnce();
  } else if (strcmp(cmd, "fault") == 0) {
    printFault();
  } else if (strcmp(cmd, "clear") == 0) {
    printStatus(g_rtd.clearFaults());
  } else if (strcmp(cmd, "threshold") == 0) {
    printThresholds();
  } else if (strcmp(cmd, "stress") == 0) {
    char* arg = strtok(nullptr, " \t\r\n");
    runStress(arg ? strtoul(arg, nullptr, 10) : 10UL);
  } else if (strcmp(cmd, "filter") == 0) {
    char* arg = strtok(nullptr, " \t\r\n");
    if (arg && strcmp(arg, "50") == 0) {
      printStatus(g_rtd.setFilter(MAX31865::Filter::HZ_50));
    } else if (arg && strcmp(arg, "60") == 0) {
      printStatus(g_rtd.setFilter(MAX31865::Filter::HZ_60));
    } else {
      Serial.println("Usage: filter 50|60");
    }
  } else if (strcmp(cmd, "wires") == 0) {
    char* arg = strtok(nullptr, " \t\r\n");
    int wires = arg ? atoi(arg) : 0;
    if (wires == 2) {
      printStatus(g_rtd.setWireMode(MAX31865::WireMode::TWO_WIRE));
    } else if (wires == 3) {
      printStatus(g_rtd.setWireMode(MAX31865::WireMode::THREE_WIRE));
    } else if (wires == 4) {
      printStatus(g_rtd.setWireMode(MAX31865::WireMode::FOUR_WIRE));
    } else {
      Serial.println("Usage: wires 2|3|4");
    }
  } else if (strcmp(cmd, "auto") == 0) {
    char* arg = strtok(nullptr, " \t\r\n");
    printStatus(g_rtd.setAutoConvert(arg && atoi(arg) != 0));
  } else {
    Serial.println("Unknown command. Type help.");
  }
}

}  // namespace

void setup() {
  Serial.begin(115200);
  delay(250);

  build_config::printBanner("MAX31865 bringup CLI");
  board::SpiPins pins = board::max31865Pins();
  g_spi.csPin = pins.cs;

  pinMode(pins.cs, OUTPUT);
  digitalWrite(pins.cs, HIGH);
  if (pins.drdy >= 0) {
    pinMode(pins.drdy, INPUT);
  }
  SPI.begin(pins.sck, pins.miso, pins.mosi, pins.cs);

  g_cfg.spiTransfer = spiTransfer;
  g_cfg.spiUser = &g_spi;
  g_cfg.nowMs = nowMs;
  g_cfg.delayMs = delayMs;
  g_cfg.delayUs = delayUs;
  g_cfg.cooperativeYield = yieldHook;
  g_cfg.referenceResistorOhms = 400.0f;
  g_cfg.rtdNominalOhms = 100.0f;
  g_cfg.wireMode = MAX31865::WireMode::FOUR_WIRE;
  g_cfg.filter = MAX31865::Filter::HZ_60;
  g_cfg.inputFilterTimeConstantUs = 1000;
  if (pins.drdy >= 0) {
    g_cfg.drdyPin = pins.drdy;
    g_cfg.gpioRead = readGpio;
  }

  printStatus(g_rtd.begin(g_cfg));
  printHelp();
}

void loop() {
  while (Serial.available() > 0) {
    char c = static_cast<char>(Serial.read());
    if (c == '\r') {
      continue;
    }
    if (c == '\n') {
      g_line[g_lineLen] = '\0';
      handleLine(g_line);
      g_lineLen = 0;
      Serial.print("> ");
      continue;
    }
    if (g_lineLen + 1 < sizeof(g_line)) {
      g_line[g_lineLen++] = c;
    }
  }
}
