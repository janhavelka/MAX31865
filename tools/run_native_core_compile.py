#!/usr/bin/env python3
"""Compile and link the public framework-neutral MAX31865 core."""

from __future__ import annotations

import pathlib
import tempfile

from native_compile import CORE_DRIVER_SOURCES, build_and_run


SOURCE = r'''#include "MAX31865/MAX31865.h"
#include "MAX31865/CommandTable.h"
#include "MAX31865/Config.h"
#include "MAX31865/Protocol.h"
#include "MAX31865/Status.h"
#include "MAX31865/Transport.h"
#include "MAX31865/Version.h"

#include <cstdint>

static_assert(MAX31865Version::VERSION_MAJOR <= UINT16_MAX, "version major");
static_assert(MAX31865Version::VERSION_MINOR <= UINT16_MAX, "version minor");
static_assert(MAX31865Version::VERSION_PATCH <= UINT16_MAX, "version patch");

int main()
{
    MAX31865 driver;
    MAX31865BeginConfig begin = max31865DefaultBeginConfig();
    MAX31865DeviceConfig deviceConfig = max31865DefaultDeviceConfig();
    MAX31865RtdConfig rtdConfig = max31865DefaultRtdConfig();
    MAX31865DeviceInfo deviceInfo{};
    MAX31865Settings settings{};
    MAX31865Sample sample{};
    MAX31865ReadOptions options{};
    MAX31865FaultStatus fault{};
    MAX31865FaultThresholds thresholds{};
    MAX31865RegisterDump dump[max31865_cmd::NUM_REGISTERS]{};
    uint8_t registers[max31865_cmd::NUM_REGISTERS]{};
    uint8_t byte = 0U;
    uint16_t code = 0U;
    size_t count = 0U;
    float first = 0.0F;
    float second = 0.0F;
    bool ready = false;

    (void)driver.begin(begin);
    driver.end();
    (void)driver.state();
    (void)driver.lastOperationStatus();
    (void)driver.health();
    driver.clearLifetimeCounters();
    (void)driver.setOfflineThreshold(1U);
    (void)driver.defaultOperationTimeoutMs();
    driver.tick(0U);
    (void)driver.probe(deviceInfo);
    (void)driver.probe(deviceInfo, 1U);
    (void)driver.recover(1U);
    (void)driver.applyConfiguration(deviceConfig, 1U);
    (void)driver.readConfiguration(settings, 1U);
    (void)driver.configureMeasurement(
        MAX31865WireMode::FourWire, MAX31865Filter::Hz60, 1U);
    (void)driver.setBias(false, 1U);
    (void)driver.setWireMode(MAX31865WireMode::FourWire, 1U);
    (void)driver.setFilter(MAX31865Filter::Hz60, 1U);
    (void)driver.setRtdConfig(rtdConfig);
    (void)driver.rtdConfig();
    (void)driver.startContinuous(1U);
    (void)driver.triggerSingleConversion(1U);
    (void)driver.stop(1U);
    (void)driver.dataReady(ready);
    (void)driver.dataReady(ready, 1U);
    (void)driver.poll(sample, &options);
    (void)driver.poll(sample, 1U, &options);
    (void)driver.readSample(sample, &options);
    (void)driver.readSample(sample, 1U, &options);
    (void)driver.readSingle(sample, 1U, &options);
    (void)driver.readOneShot(sample, 1U, &options);
    (void)driver.readFaultStatus(fault, 1U);
    (void)driver.clearFaults(1U);
    (void)driver.runAutomaticFaultDetection(fault, 1U);
    (void)driver.runManualFaultDetection(fault, 1U);
    (void)driver.setFaultThresholdsRaw(thresholds, 1U);
    (void)driver.readFaultThresholdsRaw(thresholds, 1U);
    (void)driver.setFaultThresholdsResistance(first, second, 1U);
    (void)driver.readFaultThresholdsResistance(first, second, 1U);
    (void)driver.setFaultThresholdsTemperature(first, second, 1U);
    (void)driver.readFaultThresholdsTemperature(first, second, 1U);
    (void)driver.readRegister(0U, byte, 1U);
    (void)driver.readRegisters(0U, registers, 1U, 1U);
    (void)driver.writeRegister(0U, byte, 1U);
    (void)driver.writeRegisterVerified(0U, byte, byte, 1U);
    (void)driver.dumpRegisters(
        dump, max31865_cmd::NUM_REGISTERS, count, 1U);
    (void)driver.restoreWritableDefaults(1U);
    (void)driver.registerReadbackTest(byte, 1U);
    (void)MAX31865::codeToRatio(code, first);
    (void)driver.codeToResistance(code, first);
    (void)driver.resistanceToCode(first, code);
    (void)driver.resistanceToTemperature(first, second);
    (void)driver.temperatureToResistance(first, second);
    (void)driver.temperatureToCode(first, code);
    (void)driver.singleConversionTimeMs();
    (void)driver.continuousConversionTimeMs();
    (void)driver.biasSettleTimeUs();
    (void)MAX31865::decodeFaultStatus(byte);
    (void)max31865StateName(MAX31865State::Uninitialized);
    (void)max31865DriverStateName(MAX31865DriverState::UNINIT);
    (void)max31865ErrorName(MAX31865Error::Ok);

    return driver.state() == MAX31865State::Uninitialized ? 0 : 1;
}
'''


def main() -> int:
    with tempfile.TemporaryDirectory(prefix="max31865-native-core-") as name:
        source = pathlib.Path(name) / "public_core_consumer.cpp"
        source.write_text(SOURCE, encoding="utf-8", newline="\n")
        build_and_run(
            "native_core_compile",
            (*CORE_DRIVER_SOURCES, str(source)),
        )
    print("Native core compile check PASSED")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
