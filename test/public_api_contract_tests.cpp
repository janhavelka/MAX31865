#include "MAX31865/MAX31865.h"
#include "support/TestHarness.h"

#include <type_traits>

using namespace max31865_test;

namespace {

bool classOwnershipAndMethodSignaturesAreStable()
{
    static_assert(!std::is_copy_constructible<MAX31865>::value,
                  "driver must not copy borrowed ownership state");
    static_assert(!std::is_copy_assignable<MAX31865>::value,
                  "driver must not copy borrowed ownership state");
    static_assert(!std::is_move_constructible<MAX31865>::value,
                  "driver move would invalidate callbacks/state");
    static_assert(!std::is_move_assignable<MAX31865>::value,
                  "driver move would invalidate callbacks/state");

    using Begin = MAX31865Status (MAX31865::*)(const MAX31865BeginConfig &);
    using Recover = MAX31865Status (MAX31865::*)(uint32_t);
    using Apply = MAX31865Status (MAX31865::*)(
        const MAX31865DeviceConfig &,
        uint32_t);
    using ReadConfiguration = MAX31865Status (MAX31865::*)(
        MAX31865Settings &,
        uint32_t);
    using ProbeDefault = MAX31865Status (MAX31865::*)(MAX31865DeviceInfo &);
    using ProbeTimed = MAX31865Status (MAX31865::*)(
        MAX31865DeviceInfo &,
        uint32_t);
    using DataReadyDefault = MAX31865Status (MAX31865::*)(bool &);
    using DataReadyTimed = MAX31865Status (MAX31865::*)(bool &, uint32_t);
    using Poll = MAX31865Status (MAX31865::*)(
        MAX31865Sample &,
        const MAX31865ReadOptions *);
    using PollTimed = MAX31865Status (MAX31865::*)(
        MAX31865Sample &,
        uint32_t,
        const MAX31865ReadOptions *);
    using ReadSample = MAX31865Status (MAX31865::*)(
        MAX31865Sample &,
        const MAX31865ReadOptions *);
    using ReadSampleTimed = MAX31865Status (MAX31865::*)(
        MAX31865Sample &,
        uint32_t,
        const MAX31865ReadOptions *);
    using ReadSingle = MAX31865Status (MAX31865::*)(
        MAX31865Sample &,
        uint32_t,
        const MAX31865ReadOptions *);
    using ReadFault = MAX31865Status (MAX31865::*)(
        MAX31865FaultStatus &,
        uint32_t);
    using SetThresholds = MAX31865Status (MAX31865::*)(
        const MAX31865FaultThresholds &,
        uint32_t);
    using Dump = MAX31865Status (MAX31865::*)(
        MAX31865RegisterDump *,
        size_t,
        size_t &,
        uint32_t);
    using CodeToRatio = MAX31865Status (*)(uint16_t, float &);
    using DecodeFault = MAX31865FaultStatus (*)(uint8_t);

    static_assert(std::is_same<decltype(&MAX31865::begin), Begin>::value,
                  "begin signature");
    static_assert(std::is_same<decltype(&MAX31865::recover), Recover>::value,
                  "recover signature");
    static_assert(std::is_same<decltype(&MAX31865::applyConfiguration),
                               Apply>::value,
                  "applyConfiguration signature");
    static_assert(std::is_same<decltype(&MAX31865::readConfiguration),
                               ReadConfiguration>::value,
                  "readConfiguration signature");
    static_assert(std::is_same<
                      decltype(static_cast<ProbeDefault>(&MAX31865::probe)),
                      ProbeDefault>::value,
                  "default probe signature");
    static_assert(std::is_same<
                      decltype(static_cast<ProbeTimed>(&MAX31865::probe)),
                      ProbeTimed>::value,
                  "timed probe signature");
    static_assert(std::is_same<
                      decltype(static_cast<DataReadyDefault>(
                          &MAX31865::dataReady)),
                      DataReadyDefault>::value,
                  "default dataReady signature");
    static_assert(std::is_same<
                      decltype(static_cast<DataReadyTimed>(
                          &MAX31865::dataReady)),
                      DataReadyTimed>::value,
                  "timed dataReady signature");
    static_assert(std::is_same<
                      decltype(static_cast<Poll>(&MAX31865::poll)),
                      Poll>::value,
                  "poll signature");
    static_assert(std::is_same<
                      decltype(static_cast<PollTimed>(&MAX31865::poll)),
                      PollTimed>::value,
                  "timed poll signature");
    static_assert(std::is_same<
                      decltype(static_cast<ReadSample>(&MAX31865::readSample)),
                      ReadSample>::value,
                  "readSample signature");
    static_assert(std::is_same<
                      decltype(static_cast<ReadSampleTimed>(
                          &MAX31865::readSample)),
                      ReadSampleTimed>::value,
                  "timed readSample signature");
    static_assert(std::is_same<decltype(&MAX31865::readSingle),
                               ReadSingle>::value,
                  "readSingle signature");
    static_assert(std::is_same<decltype(&MAX31865::readOneShot),
                               ReadSingle>::value,
                  "readOneShot signature");
    static_assert(std::is_same<decltype(&MAX31865::readFaultStatus),
                               ReadFault>::value,
                  "readFaultStatus signature");
    static_assert(std::is_same<decltype(&MAX31865::setFaultThresholdsRaw),
                               SetThresholds>::value,
                  "raw threshold signature");
    static_assert(std::is_same<decltype(&MAX31865::dumpRegisters), Dump>::value,
                  "dump signature");
    static_assert(std::is_same<decltype(&MAX31865::codeToRatio),
                               CodeToRatio>::value,
                  "codeToRatio signature");
    static_assert(std::is_same<decltype(&MAX31865::decodeFaultStatus),
                               DecodeFault>::value,
                  "decodeFaultStatus signature");

    static_assert(static_cast<uint8_t>(MAX31865WireMode::TwoWire) == 2U,
                  "two-wire mapping");
    static_assert(static_cast<uint8_t>(MAX31865WireMode::ThreeWire) == 3U,
                  "three-wire mapping");
    static_assert(static_cast<uint8_t>(MAX31865WireMode::FourWire) == 4U,
                  "four-wire mapping");
    static_assert(static_cast<uint8_t>(MAX31865Filter::Hz50) ==
                      max31865_cmd::CONFIG_FILTER_50HZ,
                  "50 Hz mapping");
    static_assert(static_cast<uint8_t>(MAX31865ConversionMode::Continuous) ==
                      max31865_cmd::CONFIG_AUTO,
                  "continuous mapping");
    static_assert(static_cast<uint8_t>(MAX31865FaultCycle::ManualStep2) ==
                      max31865_cmd::CONFIG_FAULT_CYCLE_MANUAL_2,
                  "fault-cycle mapping");
    return true;
}

bool statusFactoriesAndNamesCoverEveryCode()
{
    const MAX31865Status ok = MAX31865Status::Ok();
    CHECK(ok.ok());
    CHECK_EQ(MAX31865Error::Ok, ok.code);
    CHECK_CSTR_EQ("OK", ok.msg);
    CHECK_EQ(0, ok.detail);

    const MAX31865Status error = MAX31865Status::Error(
        MAX31865Error::SpiTransferFailed,
        "backend",
        -123);
    CHECK(!error.ok());
    CHECK_EQ(MAX31865Error::SpiTransferFailed, error.code);
    CHECK_CSTR_EQ("backend", error.msg);
    CHECK_EQ(-123, error.detail);

    const MAX31865Error errors[] = {
        MAX31865Error::Ok,
        MAX31865Error::NotInitialized,
        MAX31865Error::InvalidArgument,
        MAX31865Error::InvalidState,
        MAX31865Error::Busy,
        MAX31865Error::UnsupportedCommand,
        MAX31865Error::NoData,
        MAX31865Error::TimingUnavailable,
        MAX31865Error::RegisterAddressInvalid,
        MAX31865Error::BusLockTimeout,
        MAX31865Error::BusLockFailed,
        MAX31865Error::SpiTransferFailed,
        MAX31865Error::ChipSelectFailed,
        MAX31865Error::GpioFailed,
        MAX31865Error::OperationTimeout,
        MAX31865Error::DrdyTimeout,
        MAX31865Error::RegisterVerifyFailed,
        MAX31865Error::ProbeMismatch,
        MAX31865Error::ConfigurationUnknown,
        MAX31865Error::DeviceFault,
        MAX31865Error::RestoreFailed,
        MAX31865Error::ConversionOutOfRange};
    const char *const names[] = {
        "Ok",
        "NotInitialized",
        "InvalidArgument",
        "InvalidState",
        "Busy",
        "UnsupportedCommand",
        "NoData",
        "TimingUnavailable",
        "RegisterAddressInvalid",
        "BusLockTimeout",
        "BusLockFailed",
        "SpiTransferFailed",
        "ChipSelectFailed",
        "GpioFailed",
        "OperationTimeout",
        "DrdyTimeout",
        "RegisterVerifyFailed",
        "ProbeMismatch",
        "ConfigurationUnknown",
        "DeviceFault",
        "RestoreFailed",
        "ConversionOutOfRange"};
    static_assert(sizeof(errors) / sizeof(errors[0]) ==
                      sizeof(names) / sizeof(names[0]),
                  "error-name fixture size");
    for (size_t index = 0U; index < sizeof(errors) / sizeof(errors[0]); ++index) {
        CHECK_CSTR_EQ(names[index], max31865ErrorName(errors[index]));
    }
    CHECK_CSTR_EQ("Unknown", max31865ErrorName(
                                 static_cast<MAX31865Error>(0xFFU)));
    return true;
}

bool defaultRtdAndDeviceConfigurationIsFullyInitialized()
{
    const MAX31865RtdCoefficients coefficients =
        max31865Iec60751Coefficients();
    CHECK_NEAR(3.90830e-3F, coefficients.a, 1.0e-9F);
    CHECK_NEAR(-5.77500e-7F, coefficients.b, 1.0e-12F);
    CHECK_NEAR(-4.18301e-12F, coefficients.c, 1.0e-16F);

    const MAX31865RtdConfig rtd = max31865DefaultRtdConfig();
    CHECK_NEAR(400.0F, rtd.referenceResistorOhms, 0.0F);
    CHECK_NEAR(100.0F, rtd.nominalResistanceOhms, 0.0F);
    CHECK_NEAR(coefficients.a, rtd.coefficients.a, 0.0F);
    CHECK_NEAR(coefficients.b, rtd.coefficients.b, 0.0F);
    CHECK_NEAR(coefficients.c, rtd.coefficients.c, 0.0F);
    CHECK_EQ(100U, rtd.inputFilterTimeConstantUs);
    CHECK_NEAR(-200.0F, rtd.minimumTemperatureC, 0.0F);
    CHECK_NEAR(850.0F, rtd.maximumTemperatureC, 0.0F);

    const MAX31865DeviceConfig device = max31865DefaultDeviceConfig();
    CHECK_EQ(MAX31865WireMode::FourWire, device.wireMode);
    CHECK_EQ(MAX31865Filter::Hz60, device.filter);
    CHECK(!device.biasEnabled);
    CHECK_EQ(0U, device.thresholds.lowCode);
    CHECK_EQ(max31865_cmd::ADC_CODE_MAX, device.thresholds.highCode);
    return true;
}

bool defaultBeginConfigurationIsUnboundAndFinite()
{
    const MAX31865BeginConfig config = max31865DefaultBeginConfig();
    CHECK(config.transport.user == nullptr);
    CHECK(!config.transport.capabilities.hasDrdy);
    CHECK(config.transport.lockBus == nullptr);
    CHECK(config.transport.unlockBus == nullptr);
    CHECK(config.transport.setChipSelect == nullptr);
    CHECK(config.transport.transfer == nullptr);
    CHECK(config.transport.readPin == nullptr);
    CHECK(config.transport.nowMs == nullptr);
    CHECK(config.transport.nowUs == nullptr);
    CHECK(config.transport.sleepMs == nullptr);
    CHECK(config.transport.delayUs == nullptr);
    CHECK_EQ(0U, config.powerReadyDelayMs);
    CHECK_EQ(250U, config.defaultOperationTimeoutMs);
    CHECK_EQ(5U, config.offlineThreshold);
    CHECK_EQ(MAX31865WireMode::FourWire,
             config.initialDeviceConfig.wireMode);
    CHECK_NEAR(400.0F, config.rtd.referenceResistorOhms, 0.0F);
    return true;
}

bool publicDataTypesExposeValidityAndDiagnosticFields()
{
    MAX31865Pins pins;
    CHECK_EQ(MAX31865_PIN_UNUSED, pins.sck);
    CHECK_EQ(MAX31865_PIN_UNUSED, pins.miso);
    CHECK_EQ(MAX31865_PIN_UNUSED, pins.mosi);
    CHECK_EQ(MAX31865_PIN_UNUSED, pins.chipSelect);
    CHECK_EQ(MAX31865_PIN_UNUSED, pins.dataReady);

    MAX31865Sample sample{};
    sample.rawRegister = 0x1234U;
    sample.rawCode = 0x091AU;
    sample.sampleCounter = 7U;
    sample.readTimestampUs = 100U;
    sample.readyTimestampUs = 90U;
    sample.channelId = 3U;
    sample.resistanceOhms = 100.0F;
    sample.temperatureC = 0.0F;
    sample.flags = static_cast<uint8_t>(
        MAX31865_SAMPLE_FLAG_FRAME_VALID |
        MAX31865_SAMPLE_FLAG_DATA_VALID |
        MAX31865_SAMPLE_FLAG_READ_TIMESTAMP |
        MAX31865_SAMPLE_FLAG_READY_TIMESTAMP);
    CHECK_EQ(0x091AU, sample.rawCode);
    CHECK((sample.flags & MAX31865_SAMPLE_FLAG_DATA_VALID) != 0U);

    MAX31865ReadOptions options{};
    options.hasReadyTimestamp = true;
    options.readyTimestampUs = 1234U;
    options.channelId = 9U;
    CHECK(options.hasReadyTimestamp);
    CHECK_EQ(1234U, options.readyTimestampUs);
    CHECK_EQ(9U, options.channelId);

    MAX31865RegisterDump dump{};
    dump.address = max31865_cmd::REG_CONFIG;
    dump.name = "CONFIG";
    dump.value = max31865_cmd::CONFIG_BIAS;
    CHECK_CSTR_EQ("CONFIG", dump.name);
    return true;
}

bool lifecycleAndDriverStateNamesAreStable()
{
    CHECK_CSTR_EQ("Uninitialized",
                  max31865StateName(MAX31865State::Uninitialized));
    CHECK_CSTR_EQ("Ready", max31865StateName(MAX31865State::Ready));
    CHECK_CSTR_EQ("Converting", max31865StateName(MAX31865State::Converting));
    CHECK_CSTR_EQ("Fault", max31865StateName(MAX31865State::Fault));
    CHECK_CSTR_EQ("Unknown",
                  max31865StateName(static_cast<MAX31865State>(0xFFU)));
    CHECK_CSTR_EQ("UNINIT",
                  max31865DriverStateName(MAX31865DriverState::UNINIT));
    CHECK_CSTR_EQ("READY",
                  max31865DriverStateName(MAX31865DriverState::READY));
    CHECK_CSTR_EQ("DEGRADED",
                  max31865DriverStateName(MAX31865DriverState::DEGRADED));
    CHECK_CSTR_EQ("OFFLINE",
                  max31865DriverStateName(MAX31865DriverState::OFFLINE));
    CHECK_CSTR_EQ("UNKNOWN", max31865DriverStateName(
                                 static_cast<MAX31865DriverState>(0xFFU)));
    return true;
}

} // namespace

int main()
{
    const TestCase tests[] = {
        {"ownership and method signatures",
         classOwnershipAndMethodSignaturesAreStable},
        {"status factories and names", statusFactoriesAndNamesCoverEveryCode},
        {"RTD and device defaults",
         defaultRtdAndDeviceConfigurationIsFullyInitialized},
        {"begin defaults", defaultBeginConfigurationIsUnboundAndFinite},
        {"public data fields", publicDataTypesExposeValidityAndDiagnosticFields},
        {"state names", lifecycleAndDriverStateNamesAreStable}};
    return runTests(tests, sizeof(tests) / sizeof(tests[0]));
}
