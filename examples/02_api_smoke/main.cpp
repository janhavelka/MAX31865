#include <Arduino.h>
#include <SPI.h>
#include "MAX31865/MAX31865.h"
#include <MAX31865/Version.h>

MAX31865 rtd;
static volatile bool g_touch_hardware = false;

static void exercisePublicApi(MAX31865& device) {
  MAX31865Pins pins = {12, 13, 11, 10, -1};

  MAX31865BeginConfig cfg{};
  cfg.spi = &SPI;
  cfg.pins = pins;
  cfg.spiHz = 1000000U;
  cfg.verifyProbe = false;
  cfg.referenceResistorOhms = 400.0f;
  cfg.rtdNominalOhms = 100.0f;
  cfg.inputFilterTimeConstantUs = 1000U;
  cfg.wireMode = MAX31865WireMode::FourWire;
  cfg.filter = MAX31865Filter::Hz60;
  cfg.useCustomCoefficients = false;
  cfg.coefficients = {3.90830e-3f, -5.77500e-7f, -4.18301e-12f};

  MAX31865Health health = device.health();
  MAX31865Status status = device.lastOperationStatus();
  MAX31865FaultStatus fault{};
  MAX31865Sample sample{};
  MAX31865RawRtd raw{};
  MAX31865FaultThresholds thresholds{};
  MAX31865Settings settings{};
  MAX31865RegisterDump dump[8] = {};
  uint8_t regs[8] = {};
  uint8_t readback = 0;
  float lowOhms = 0.0f;
  float highOhms = 0.0f;
  float value = 0.0f;

  (void)MAX31865Version::VERSION_FULL;
  (void)max31865StateName(device.state());
  (void)max31865DriverStateName(device.driverState());
  (void)max31865ErrorName(device.lastError());
  (void)device.healthState();
  (void)device.isOnline();
  (void)device.lastErrorName();
  (void)device.lastOkMs();
  (void)device.lastErrorMs();
  (void)device.consecutiveFailures();
  (void)device.totalFailures();
  (void)device.totalSuccess();
  (void)device.offlineThreshold();
  device.setOfflineThreshold(5);
  device.setSpiLockTimeoutMs(50U);
  (void)device.spiLockTimeoutMs();
  device.clearHealthCounters();
  device.setSpiHz(1000000U);
  (void)device.spiHz();
  (void)device.wireMode();
  (void)device.filter();
  (void)device.biasEnabled();
  (void)device.autoConvertEnabled();
  (void)device.referenceResistorOhms();
  (void)device.rtdNominalOhms();
  (void)device.inputFilterTimeConstantUs();
  (void)device.rtdCoefficients();
  (void)device.setRtdParameters(400.0f, 100.0f);

  (void)MAX31865::decodeFaultStatus(0xFC, fault);
  (void)MAX31865::codeToRatio(16384U);
  (void)device.codeToResistance(16384U);
  (void)device.resistanceToCode(100.0f);
  (void)device.resistanceToTemperature(100.0f);
  (void)device.temperatureToResistance(25.0f);
  (void)device.temperatureToCode(25.0f);
  (void)device.getSingleConversionTimeMs();
  (void)device.getContinuousConversionTimeMs();
  (void)device.getBiasSettleTimeUs();

  if (g_touch_hardware) {
    (void)device.begin(cfg);
    (void)device.begin(SPI, pins.sck, pins.miso, pins.mosi, pins.cs, pins.drdy, cfg.spiHz);
    (void)device.probe();
    (void)device.recover();
    (void)device.setBias(true);
    (void)device.setAutoConvert(false);
    (void)device.startContinuous();
    (void)device.stop();
    (void)device.setWireMode(MAX31865WireMode::ThreeWire);
    (void)device.setFilter(MAX31865Filter::Hz50);
    (void)device.configureMeasurement(MAX31865WireMode::FourWire, MAX31865Filter::Hz60, false);
    (void)device.readSingle(sample, 200U);
    (void)device.poll(sample);
    (void)device.available();
    (void)device.isDataReady();
    (void)device.readIfReady(sample);
    (void)device.readSample(sample);
    (void)device.droppedCount();
    (void)device.overrunCount();
    (void)device.totalReadCount();
    (void)device.keptSampleCount();
    (void)device.readRawRtd(raw);
    (void)device.readResistance(value);
    (void)device.readTemperature(value);
    (void)device.readFaultStatus(fault);
    (void)device.clearFaults();
    (void)device.runAutoFaultDetection(fault);
    (void)device.runManualFaultDetection(fault, 1000U);
    (void)device.setFaultThresholdsRaw(0U, 32767U);
    (void)device.getFaultThresholdsRaw(thresholds);
    (void)device.setFaultThresholdsResistance(0.0f, 400.0f);
    (void)device.getFaultThresholdsResistance(lowOhms, highOhms);
    (void)device.setFaultThresholdsTemperature(-50.0f, 250.0f);
    (void)device.readReg(max31865_cmd::REG_CONFIG);
    (void)device.readReg(max31865_cmd::REG_CONFIG, readback);
    (void)device.readRegs(max31865_cmd::REG_CONFIG, regs, sizeof(regs));
    (void)device.writeReg(max31865_cmd::REG_CONFIG, max31865_cmd::CONFIG_RESET);
    (void)device.writeRegVerify(max31865_cmd::REG_CONFIG, max31865_cmd::CONFIG_RESET, &readback);
    (void)device.dumpRegisters(dump, 8U);
    (void)device.getSettings(settings);
    (void)device.registerReadbackTest(&readback);
    (void)device.resetRegisters();
    device.tick(millis());
    device.end();
  }

  (void)health;
  (void)status;
}

void setup() {
  Serial.begin(115200);
  exercisePublicApi(rtd);
}

void loop() {}
