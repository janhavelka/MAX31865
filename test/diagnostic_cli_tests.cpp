#include "Max31865Cli.h"
#include "Max31865CliShell.h"
#include "support/ScriptedMax31865Transport.h"
#include "support/TestHarness.h"

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

using namespace max31865_test;

namespace {

struct FixedOutput final {
    static constexpr size_t CAPACITY = 65536U;

    char bytes[CAPACITY]{};
    size_t length = 0U;
    bool overflow = false;

    void clear()
    {
        length = 0U;
        overflow = false;
        bytes[0] = '\0';
    }

    void append(const char *data, size_t dataLength)
    {
        if (data == nullptr || dataLength == 0U) {
            return;
        }
        const size_t available = CAPACITY - 1U - length;
        const size_t copied = dataLength < available ? dataLength : available;
        if (copied > 0U) {
            memcpy(bytes + length, data, copied);
            length += copied;
            bytes[length] = '\0';
        }
        if (copied != dataLength) {
            overflow = true;
        }
    }

    bool contains(const char *needle) const
    {
        return needle != nullptr && strstr(bytes, needle) != nullptr;
    }

    size_t occurrences(const char *needle) const
    {
        if (needle == nullptr || needle[0] == '\0') {
            return 0U;
        }
        size_t count = 0U;
        const size_t needleLength = strlen(needle);
        const char *cursor = bytes;
        while ((cursor = strstr(cursor, needle)) != nullptr) {
            ++count;
            cursor += needleLength;
        }
        return count;
    }
};

struct CliFixture final {
    ScriptedMax31865Transport scripted;
    MAX31865 device;
    MAX31865BeginConfig config;
    max31865_cli::TransportObserver observer;
    MAX31865Pins pins;
    FixedOutput output;
    max31865_cli::Context context;

    explicit CliFixture(bool hasDrdy = true)
        : scripted(),
          device(),
          config(max31865DefaultBeginConfig()),
          observer{},
          pins{1, 2, 3, 4, hasDrdy ? 5 : MAX31865_PIN_UNUSED},
          output(),
          context{}
    {
        const MAX31865Transport downstream = scripted.makeTransport(hasDrdy);
        config.transport = max31865_cli::observeTransport(observer, downstream);
        context = max31865_cli::Context{
            &device,
            &config,
            &pins,
            &observer,
            1000000U,
            {this, writeThunk, nowMsThunk, freeHeapThunk},
            true};

        // Output format is process-global in the example. Normalize it before
        // each independently bound fixture, then reset the command sequence.
        max31865_cli::bind(context);
        max31865_cli::processLine("format human");
        max31865_cli::bind(context);
        clearOutput();
        clearIo();
    }

    static void writeThunk(void *user, const char *data, size_t length)
    {
        static_cast<CliFixture *>(user)->output.append(data, length);
    }

    static uint32_t nowMsThunk(void *user)
    {
        return static_cast<CliFixture *>(user)->scripted.nowMs();
    }

    static uint32_t freeHeapThunk(void *)
    {
        return 123456U;
    }

    bool beginDirect()
    {
        return device.begin(config).ok();
    }

    void clearOutput()
    {
        output.clear();
    }

    void clearIo()
    {
        scripted.resetLog();
        max31865_cli::clearTransportCounters(observer);
    }

    void run(const char *line)
    {
        clearOutput();
        max31865_cli::processLine(line);
    }
};

struct FixedInputStream final {
    const char *bytes;
    size_t length;
    size_t position;

    int available() const
    {
        return position < length ? 1 : 0;
    }

    int read()
    {
        if (position >= length) {
            return -1;
        }
        const unsigned char value =
            static_cast<unsigned char>(bytes[position]);
        ++position;
        return static_cast<int>(value);
    }
};

bool outputHasCommandFrame(
    const FixedOutput &output,
    uint32_t id,
    const char *command,
    bool ok)
{
    char begin[96]{};
    char end[96]{};
    const int beginLength = snprintf(
        begin,
        sizeof(begin),
        "COMMAND_BEGIN id=%lu command=%s",
        static_cast<unsigned long>(id),
        command);
    const int endLength = snprintf(
        end,
        sizeof(end),
        "COMMAND_END id=%lu ok=%u",
        static_cast<unsigned long>(id),
        ok ? 1U : 0U);
    return beginLength > 0 && endLength > 0 && output.contains(begin) &&
        output.contains(end) && output.occurrences("COMMAND_BEGIN") == 1U &&
        output.occurrences("COMMAND_END") == 1U;
}

bool tableHelpAndCommandCorrelationAreComplete()
{
    CliFixture fixture;
    max31865_cli::start();
    CHECK_EQ(MAX31865State::Ready, fixture.device.state());
    CHECK(!fixture.output.overflow);
    CHECK(fixture.output.contains("MAX31865 bounded diagnostic CLI"));

    const char *commands[] = {
        "help", "format", "version", "wiring", "timeout", "telemetry",
        "state", "health",
        "counters", "begin", "end", "recover", "probe", "settings",
        "offline", "bias", "wire", "filter", "rtd", "cvd", "start",
        "trigger", "stop", "ready", "poll", "read", "wait", "one",
        "stream", "stress", "job", "cancel", "faults", "fault-clear",
        "fault-auto", "fault-manual", "thresholds", "reg", "convert"};
    for (const char *command : commands) {
        CHECK(fixture.output.contains(command));
    }

    fixture.run("help fault-manual");
    CHECK(outputHasCommandFrame(fixture.output, 1U, "help", true));
    CHECK(fixture.output.contains("fault-manual"));
    CHECK(fixture.output.contains("safety=confirm_mutation"));
    CHECK(fixture.output.contains("execution=device_io"));

    fixture.run("not-a-command");
    CHECK(outputHasCommandFrame(
        fixture.output, 2U, "not-a-command", false));
    CHECK(fixture.output.contains("unknown command"));
    CHECK(fixture.output.contains("name=InvalidArgument"));
    return true;
}

bool metadataTelemetryAndTimeoutAreExplicit()
{
    CliFixture fixture;
    fixture.run("version");
    CHECK(fixture.output.contains("target=unknown"));
    CHECK(fixture.output.contains("framework=unknown"));
    CHECK(fixture.output.contains("build=unknown"));
    CHECK(fixture.output.contains("evidence=software_only_unvalidated_hardware"));

    fixture.run("wiring");
    CHECK(fixture.output.contains("spi_clock_hz=1000000"));
    CHECK(fixture.output.contains("spi_max_hz=5000000"));
    CHECK(fixture.output.contains("wire=4"));
    CHECK(fixture.output.contains("filter_hz=60"));

    fixture.run("telemetry");
    CHECK(fixture.output.contains("free_heap_valid=1"));
    CHECK(fixture.output.contains("free_heap_bytes=123456"));

    fixture.run("timeout");
    CHECK(fixture.output.contains("milliseconds=250"));
    fixture.run("timeout 20");
    CHECK(fixture.output.contains("milliseconds=20"));
    fixture.run("begin confirm");
    CHECK_EQ(20U, fixture.device.defaultOperationTimeoutMs());
    return true;
}

bool confirmationRejectionIsExactAndPerformsNoIo()
{
    CliFixture fixture;
    CHECK(fixture.beginDirect());
    fixture.clearIo();
    const MAX31865Health beforeHealth = fixture.device.health();
    const MAX31865RtdConfig beforeRtd = fixture.device.rtdConfig();
    const uint8_t beforeConfig = fixture.scripted.device().registerValue(
        max31865_cmd::REG_CONFIG);
    const uint8_t beforeHigh = fixture.scripted.device().registerValue(
        max31865_cmd::REG_HIGH_FAULT_MSB);

    const char *rejected[] = {
        "begin", "end", "end CONFIRM", "recover", "offline 9",
        "bias on", "wire 3", "filter 50", "cvd set 1 2 3", "start",
        "trigger", "one", "one 100", "stop", "stream 5 10",
        "stress 5", "cancel",
        "fault-clear", "fault-auto", "fault-manual",
        "counters clear", "rtd set 400 100 0 -200 850",
        "thresholds set raw 10 20", "reg write 3 2", "reg verify 3 2",
        "reg test", "reg defaults"};
    for (const char *line : rejected) {
        fixture.run(line);
        CHECK(fixture.output.contains("[ERR] parse"));
        CHECK(fixture.output.contains("COMMAND_BEGIN"));
        CHECK(fixture.output.contains("COMMAND_END"));
        CHECK_EQ(0U, fixture.scripted.eventCount());
        CHECK_EQ(0U, fixture.observer.counters.lockCalls);
        CHECK_EQ(0U, fixture.observer.counters.transferCalls);
        CHECK_EQ(MAX31865State::Ready, fixture.device.state());
    }

    const MAX31865Health afterHealth = fixture.device.health();
    const MAX31865RtdConfig afterRtd = fixture.device.rtdConfig();
    CHECK_EQ(beforeHealth.trackedSuccessCount, afterHealth.trackedSuccessCount);
    CHECK_EQ(beforeHealth.trackedFailureCount, afterHealth.trackedFailureCount);
    CHECK_EQ(beforeHealth.offlineThreshold, afterHealth.offlineThreshold);
    CHECK_NEAR(
        beforeRtd.referenceResistorOhms,
        afterRtd.referenceResistorOhms,
        0.0F);
    CHECK_NEAR(
        beforeRtd.coefficients.a,
        afterRtd.coefficients.a,
        0.0F);
    CHECK_EQ(
        beforeConfig,
        fixture.scripted.device().registerValue(max31865_cmd::REG_CONFIG));
    CHECK_EQ(
        beforeHigh,
        fixture.scripted.device().registerValue(
            max31865_cmd::REG_HIGH_FAULT_MSB));
    return true;
}

bool parserAndSerialShellEnforceFiniteBounds()
{
    CliFixture fixture;
    CHECK(fixture.beginDirect());
    fixture.clearIo();

    char maximumLine[192]{};
    memset(maximumLine, 'x', sizeof(maximumLine) - 1U);
    fixture.run(maximumLine);
    CHECK(fixture.output.contains("unknown command"));
    CHECK(fixture.output.contains("COMMAND_END"));

    char overlongLine[193]{};
    memset(overlongLine, 'y', sizeof(overlongLine) - 1U);
    fixture.run(overlongLine);
    CHECK(fixture.output.contains("input line exceeds 191 bytes"));
    CHECK(fixture.output.contains("COMMAND_BEGIN"));
    CHECK(fixture.output.contains("command=<invalid>"));
    CHECK(fixture.output.contains("COMMAND_END"));

    char unterminatedLine[192]{};
    memset(unterminatedLine, 'u', sizeof(unterminatedLine));
    fixture.clearOutput();
    max31865_cli::processLine(unterminatedLine);
    CHECK(fixture.output.contains("input line exceeds 191 bytes"));
    CHECK(fixture.output.contains("COMMAND_BEGIN"));
    CHECK(fixture.output.contains("COMMAND_END"));

    const char nonPrintableLine[] = {'h', 'e', 'l', 'p', '\x01', 'x', '\0'};
    fixture.clearOutput();
    max31865_cli::processLine(nonPrintableLine);
    CHECK(fixture.output.contains("input contains a non-printable character"));
    CHECK(fixture.output.contains("command=<invalid>"));
    CHECK(fixture.output.contains("COMMAND_END"));

    fixture.run("help a b c d e f g h i j k l");
    CHECK(fixture.output.contains("too many command tokens"));
    CHECK(fixture.output.contains("COMMAND_BEGIN"));
    CHECK(fixture.output.contains("command=help"));
    CHECK(fixture.output.contains("COMMAND_END"));
    fixture.run("offline -1 confirm");
    CHECK(fixture.output.contains("offline threshold must be 1..255"));
    fixture.run("recover 2147483648 confirm");
    CHECK(fixture.output.contains("timeout must be 1..INT32_MAX"));
    fixture.run("convert code 32768");
    CHECK(fixture.output.contains("code must be 0..32767"));
    fixture.run("convert temperature nan");
    CHECK(fixture.output.contains("conversion input must be finite"));
    fixture.run("stream 100001 1 confirm");
    CHECK(fixture.output.contains("stream count must be 1..100000"));
    CHECK_EQ(0U, fixture.scripted.eventCount());

    max31865_cli::BoundedSerialShell shell;
    char shellLine[192]{};
    char validInput[193]{};
    memset(validInput, 'v', 191U);
    validInput[191] = '\n';
    FixedInputStream valid{validInput, 192U, 0U};
    max31865_cli::BoundedSerialShell::Result result =
        max31865_cli::BoundedSerialShell::Result::None;
    size_t polls = 0U;
    while (result == max31865_cli::BoundedSerialShell::Result::None &&
           polls < 8U) {
        const size_t before = valid.position;
        result = shell.poll(valid, shellLine, sizeof(shellLine));
        CHECK(valid.position - before <= 32U);
        ++polls;
    }
    CHECK_EQ(max31865_cli::BoundedSerialShell::Result::Line, result);
    CHECK_EQ(191U, strlen(shellLine));

    char invalidInput[194]{};
    memset(invalidInput, 'z', 192U);
    invalidInput[192] = '\n';
    FixedInputStream invalid{invalidInput, 193U, 0U};
    result = max31865_cli::BoundedSerialShell::Result::None;
    polls = 0U;
    while (result == max31865_cli::BoundedSerialShell::Result::None &&
           polls < 8U) {
        const size_t before = invalid.position;
        result = shell.poll(invalid, shellLine, sizeof(shellLine));
        CHECK(invalid.position - before <= 32U);
        ++polls;
    }
    CHECK_EQ(max31865_cli::BoundedSerialShell::Result::Overflow, result);
    fixture.clearOutput();
    max31865_cli::processInputOverflow();
    CHECK(fixture.output.contains("COMMAND_BEGIN"));
    CHECK(fixture.output.contains("command=<invalid>"));
    CHECK(fixture.output.contains("input line exceeds 191 bytes"));
    CHECK(fixture.output.contains("COMMAND_END"));

    const char editedInput[] = {'a', 'b', 'c', '\b', 'D', '\n'};
    FixedInputStream edited{editedInput, sizeof(editedInput), 0U};
    result = shell.poll(edited, shellLine, sizeof(shellLine));
    CHECK_EQ(max31865_cli::BoundedSerialShell::Result::Line, result);
    CHECK_CSTR_EQ("abD", shellLine);
    return true;
}

bool machineModeProducesStableRecordsAndSuppressesPrompt()
{
    CliFixture fixture;
    CHECK(fixture.beginDirect());

    fixture.run("format machine");
    CHECK(outputHasCommandFrame(fixture.output, 1U, "format", true));
    CHECK(fixture.output.contains("REC result id=1 op=format ok=1"));

    fixture.run("state");
    CHECK(outputHasCommandFrame(fixture.output, 2U, "state", true));
    CHECK(fixture.output.contains("REC state lifecycle=Ready"));
    CHECK(fixture.output.contains("driver=READY"));
    const size_t beforePrompt = fixture.output.length;
    max31865_cli::printPrompt();
    CHECK_EQ(beforePrompt, fixture.output.length);

    fixture.clearOutput();
    max31865_cli::processInputOverflow();
    CHECK(fixture.output.contains(
        "COMMAND_BEGIN id=3 command=<invalid>"));
    CHECK(fixture.output.contains("REC result id=3 op=parse ok=0"));
    CHECK(fixture.output.contains("COMMAND_END id=3 ok=0"));
    CHECK(!fixture.output.contains("[ERR]"));

    fixture.run("help reg");
    CHECK(fixture.output.contains("REC help command=reg"));
    CHECK(fixture.output.contains("args=\"read addr [length] | dump"));
    CHECK(fixture.output.contains("safety=mixed_subcommands"));
    fixture.run("reg verify 4 0x20 confirm");
    CHECK(fixture.output.contains("REC register_verify readback=0x20"));
    CHECK(!fixture.output.contains("\nregister_verify"));
    fixture.run("reg dump");
    CHECK(fixture.output.contains(
        "REC warning kind=register_dump_acknowledges_drdy"));
    CHECK(!fixture.output.contains("\nwarning:"));
    fixture.run("reg test confirm");
    CHECK(fixture.output.contains("REC register_test observed="));
    CHECK(!fixture.output.contains("\nregister_test"));
    fixture.run("format human");
    CHECK(fixture.output.contains("[OK] format"));
    fixture.clearOutput();
    max31865_cli::printPrompt();
    CHECK_CSTR_EQ("max31865> ", fixture.output.bytes);
    return true;
}

bool successfulMachineCommandsEmitOneCorrelatedResult()
{
    CliFixture fixture;
    CHECK(fixture.beginDirect());

    fixture.run("format machine");
    CHECK(fixture.output.contains("REC result id=1 op=format ok=1"));
    CHECK_EQ(1U, fixture.output.occurrences("REC result"));

    struct CommandResult
    {
        const char *line;
        const char *operation;
    };
    const CommandResult commands[] = {
        {"help version", "help"},
        {"version", "version"},
        {"wiring", "wiring"},
        {"timeout", "timeout"},
        {"telemetry", "telemetry"},
        {"state", "state"},
        {"health", "health"},
        {"counters show", "counters"},
        {"rtd get", "rtd"},
        {"job", "job"},
        {"probe", "probe"},
        {"settings", "settings"},
        {"faults", "faults"}};

    uint32_t commandId = 1U;
    for (const CommandResult &command : commands) {
        ++commandId;
        fixture.run(command.line);
        char expected[96]{};
        const int length = snprintf(
            expected,
            sizeof(expected),
            "REC result id=%lu op=%s ok=1",
            static_cast<unsigned long>(commandId),
            command.operation);
        CHECK(length > 0);
        CHECK(fixture.output.contains(expected));
        CHECK_EQ(1U, fixture.output.occurrences("REC result"));
        if (strcmp(command.line, "job") == 0) {
            CHECK(fixture.output.contains(
                "REC job active=0 kind=none command_id=0 target=0 "
                "attempt_limit=0 attempts=0 successes=0 failures=0 "
                "no_data=0 interval_ms=0 operation_timeout_ms=0 "
                "elapsed_ms=0"));
        }
    }
    return true;
}

bool lifecycleCommandsBeginProbeRecoverAndEndCleanly()
{
    CliFixture fixture;
    fixture.run("stress 1 confirm");
    CHECK(fixture.output.contains("[ERR] stress_start"));
    CHECK(fixture.output.contains("name=InvalidState"));
    CHECK(!fixture.output.contains("JOB_BEGIN"));
    CHECK_EQ(0U, fixture.scripted.eventCount());

    fixture.run("probe");
    CHECK(fixture.output.contains("name=NotInitialized"));
    CHECK_EQ(MAX31865State::Uninitialized, fixture.device.state());

    fixture.run("begin confirm");
    CHECK(fixture.output.contains("[OK] begin"));
    CHECK_EQ(MAX31865State::Ready, fixture.device.state());
    CHECK(fixture.observer.counters.transferCalls > 0U);

    fixture.run("probe");
    CHECK(fixture.output.contains("matches=1"));
    CHECK(fixture.output.contains("[OK] probe"));
    fixture.run("recover 100 confirm");
    CHECK(fixture.output.contains("[OK] recover"));
    CHECK_EQ(MAX31865State::Ready, fixture.device.state());

    fixture.run("start confirm");
    CHECK_EQ(MAX31865State::Converting, fixture.device.state());
    fixture.clearIo();
    fixture.run("stress 1 confirm");
    CHECK(fixture.output.contains("[ERR] stress_start"));
    CHECK(!fixture.output.contains("JOB_BEGIN"));
    CHECK_EQ(0U, fixture.scripted.eventCount());
    fixture.run("stop confirm");
    CHECK_EQ(MAX31865State::Ready, fixture.device.state());

    fixture.clearIo();
    fixture.run("end confirm");
    CHECK(fixture.output.contains("[OK] end"));
    CHECK_EQ(MAX31865State::Uninitialized, fixture.device.state());
    CHECK_EQ(0U, fixture.scripted.eventCount());
    CHECK_EQ(0U, fixture.observer.counters.transferCalls);

    fixture.run("begin confirm");
    CHECK_EQ(MAX31865State::Ready, fixture.device.state());
    return true;
}

bool sampleAndLocalConversionCommandsUseTheExpectedPaths()
{
    CliFixture fixture;
    CHECK(fixture.beginDirect());
    CHECK(fixture.scripted.device().enqueueSample(8192U));
    fixture.clearIo();
    fixture.run("one 100 confirm");
    CHECK(fixture.output.contains("sample source=one"));
    CHECK(fixture.output.contains("code=8192"));
    CHECK(fixture.output.contains("temperature_c=0.000000"));
    CHECK(fixture.output.contains("[OK] one"));
    CHECK_EQ(MAX31865State::Ready, fixture.device.state());
    CHECK(!fixture.scripted.device().conversionRunning());

    CHECK(fixture.scripted.device().enqueueSample(9000U));
    fixture.run("trigger confirm");
    CHECK_EQ(MAX31865State::Converting, fixture.device.state());
    fixture.run("wait 100");
    CHECK(fixture.output.contains("sample source=wait"));
    CHECK(fixture.output.contains("code=9000"));
    CHECK_EQ(MAX31865State::Ready, fixture.device.state());

    fixture.scripted.device().forceReadySample(10000U);
    fixture.run("read");
    CHECK(fixture.output.contains("sample source=read"));
    CHECK(fixture.output.contains("code=10000"));

    fixture.clearIo();
    fixture.run("convert code 8192");
    CHECK(fixture.output.contains("input=code code=8192"));
    CHECK(fixture.output.contains("resistance_ohms=100"));
    CHECK(fixture.output.contains("temperature_valid=1"));
    CHECK_EQ(0U, fixture.scripted.eventCount());
    fixture.run("convert temperature 100");
    CHECK(fixture.output.contains("input=temperature"));
    CHECK(fixture.output.contains("temperature_c=100"));
    CHECK_EQ(0U, fixture.scripted.eventCount());
    return true;
}

bool faultThresholdAndRegisterCommandsReachTypedDriverApis()
{
    CliFixture fixture;
    CHECK(fixture.beginDirect());

    fixture.run("thresholds set raw 100 30000 confirm");
    CHECK(fixture.output.contains("[OK] thresholds_set"));
    fixture.run("thresholds get raw");
    CHECK(fixture.output.contains("units=raw low=100 high=30000"));
    CHECK(fixture.output.contains("comparison=low_le_high_ge"));

    fixture.run("reg verify 4 0x20 confirm");
    CHECK(fixture.output.contains("register_verify readback=0x20"));
    CHECK(fixture.output.contains("[OK] reg_write"));
    fixture.run("reg read 4");
    CHECK(fixture.output.contains("address=0x04"));
    CHECK(fixture.output.contains("value=0x20"));

    fixture.scripted.device().setFaultInputs(
        max31865_cmd::FAULT_REFIN_LOW);
    fixture.run("fault-auto confirm");
    CHECK(fixture.output.contains("fault raw=0x10"));
    CHECK(fixture.output.contains("refin_low=1"));
    CHECK(fixture.output.contains("name=DeviceFault"));
    fixture.run("fault-clear confirm");
    CHECK(fixture.output.contains("[OK] fault_clear"));

    fixture.scripted.device().setFaultInputs(0U);
    fixture.run("fault-manual confirm");
    CHECK(fixture.output.contains("fault raw=0x00"));
    CHECK(fixture.output.contains("[OK] fault-manual"));
    fixture.run("faults");
    CHECK(fixture.output.contains("fault raw=0x00"));
    return true;
}

bool registerSpanReadsAreBoundedAndReportDrdyAcknowledgement()
{
    CliFixture fixture;
    CHECK(fixture.beginDirect());
    fixture.run("format machine");

    fixture.scripted.device().forceReadySample(0x1234U);
    const uint32_t beforeSpanAck =
        fixture.scripted.device().rtdReadAcknowledgeCount();
    fixture.clearIo();
    fixture.run("reg read 0 3");
    CHECK_EQ(3U, fixture.output.occurrences("REC register address="));
    CHECK(fixture.output.contains("REC register address=0x00 name=CONFIG"));
    CHECK(fixture.output.contains("REC register address=0x01 name=RTD_MSB"));
    CHECK(fixture.output.contains("REC register address=0x02 name=RTD_LSB"));
    CHECK(fixture.output.contains(
        "REC warning kind=register_read_acknowledges_drdy"));
    CHECK(fixture.output.contains("REC result id=2 op=reg_read ok=1"));
    CHECK_EQ(1U, fixture.observer.counters.transferCalls);
    CHECK_EQ(4U, fixture.observer.counters.transferBytes);
    CHECK_EQ(
        beforeSpanAck + 1U,
        fixture.scripted.device().rtdReadAcknowledgeCount());

    fixture.clearIo();
    fixture.run("reg read 3 2");
    CHECK_EQ(2U, fixture.output.occurrences("REC register address="));
    CHECK(fixture.output.contains("REC register address=0x03"));
    CHECK(fixture.output.contains("REC register address=0x04"));
    CHECK(!fixture.output.contains("register_read_acknowledges_drdy"));
    CHECK_EQ(1U, fixture.observer.counters.transferCalls);
    CHECK_EQ(3U, fixture.observer.counters.transferBytes);

    fixture.scripted.device().forceReadySample(0x2345U);
    const uint32_t beforeSingleAck =
        fixture.scripted.device().rtdReadAcknowledgeCount();
    fixture.clearIo();
    fixture.run("reg read 1");
    CHECK_EQ(1U, fixture.output.occurrences("REC register address="));
    CHECK(fixture.output.contains(
        "REC warning kind=register_read_acknowledges_drdy"));
    CHECK_EQ(1U, fixture.observer.counters.transferCalls);
    CHECK_EQ(2U, fixture.observer.counters.transferBytes);
    CHECK_EQ(
        beforeSingleAck + 1U,
        fixture.scripted.device().rtdReadAcknowledgeCount());

    fixture.run("format human");
    fixture.clearIo();
    fixture.run("reg read 7 2");
    CHECK(fixture.output.contains(
        "register span must contain 1..8 bytes within addresses 0..7"));
    CHECK_EQ(0U, fixture.scripted.eventCount());
    CHECK_EQ(0U, fixture.observer.counters.transferCalls);

    fixture.run("reg read 0 0");
    CHECK(fixture.output.contains(
        "register span must contain 1..8 bytes within addresses 0..7"));
    CHECK_EQ(0U, fixture.scripted.eventCount());
    return true;
}

bool stressJobAdvancesOneStepAndCanBeCancelled()
{
    CliFixture fixture;
    CHECK(fixture.beginDirect());
    CHECK(fixture.scripted.device().enqueueSample(8192U));
    CHECK(fixture.scripted.device().enqueueSample(8200U));
    CHECK(fixture.scripted.device().enqueueSample(8300U));

    fixture.clearIo();
    fixture.run("stress 3 confirm");
    CHECK(fixture.output.contains("JOB_BEGIN command_id=1 kind=stress"));
    CHECK_EQ(0U, fixture.observer.counters.transferCalls);
    fixture.clearOutput();
    max31865_cli::service();
    CHECK_EQ(1U, fixture.device.health().sampleFrameSuccessCount);
    CHECK(fixture.output.contains("sample source=stress"));
    CHECK(!fixture.output.contains("JOB_END"));

    fixture.run("job");
    CHECK(fixture.output.contains("attempts=1 successes=1"));
    fixture.clearOutput();
    max31865_cli::service();
    CHECK_EQ(2U, fixture.device.health().sampleFrameSuccessCount);
    CHECK(!fixture.output.contains("JOB_END"));
    max31865_cli::service();
    CHECK_EQ(3U, fixture.device.health().sampleFrameSuccessCount);
    CHECK(fixture.output.contains("JOB_END command_id=1 kind=stress ok=1"));
    CHECK(fixture.output.contains("reason=complete"));

    CHECK(fixture.scripted.device().enqueueSample(8400U));
    fixture.run("stress 5 confirm");
    CHECK(fixture.output.contains("JOB_BEGIN"));
    fixture.clearIo();
    fixture.run("settings");
    CHECK(fixture.output.contains("a CLI job is active"));
    CHECK_EQ(0U, fixture.scripted.eventCount());
    fixture.run("poll");
    CHECK(fixture.output.contains("a CLI job is active"));
    CHECK_EQ(0U, fixture.scripted.eventCount());
    const MAX31865Health beforeClear = fixture.device.health();
    fixture.run("counters clear confirm");
    CHECK(fixture.output.contains(
        "counters clear is unavailable while a CLI job is active"));
    CHECK_EQ(beforeClear.trackedSuccessCount,
             fixture.device.health().trackedSuccessCount);
    CHECK_EQ(beforeClear.sampleFrameSuccessCount,
             fixture.device.health().sampleFrameSuccessCount);
    fixture.clearOutput();
    max31865_cli::service();
    CHECK_EQ(4U, fixture.device.health().sampleFrameSuccessCount);
    fixture.run("cancel confirm");
    CHECK(fixture.output.contains("reason=cancelled"));
    CHECK(fixture.output.contains("JOB_END"));

    fixture.clearIo();
    fixture.clearOutput();
    max31865_cli::service();
    CHECK_EQ(0U, fixture.scripted.eventCount());
    CHECK_EQ(0U, fixture.output.length);
    return true;
}

bool stressTimeoutRunsBoundedOneShotCleanup()
{
    CliFixture fixture;
    fixture.run("timeout 20");
    fixture.run("begin confirm");
    CHECK_EQ(MAX31865State::Ready, fixture.device.state());
    fixture.run("stress 1 confirm");
    fixture.run("timeout 250");
    fixture.clearOutput();
    max31865_cli::service();
    CHECK(fixture.output.contains("reason=completed_with_failures"));
    CHECK(fixture.output.contains("cleanup_name=Ok"));
    CHECK_EQ(MAX31865State::Ready, fixture.device.state());
    CHECK(!fixture.scripted.device().conversionRunning());
    return true;
}

bool streamJobIsCooperativeAndCleanupIsBounded()
{
    CliFixture fixture;
    CHECK(fixture.beginDirect());
    CHECK(fixture.scripted.device().enqueueSample(7000U));
    CHECK(fixture.scripted.device().enqueueSample(7100U));
    fixture.run("stream 2 60 confirm");
    CHECK(fixture.output.contains("JOB_BEGIN command_id=1 kind=stream"));
    CHECK(fixture.scripted.device().continuousConversion());

    fixture.clearOutput();
    max31865_cli::service();
    CHECK_EQ(0U, fixture.device.health().sampleFrameSuccessCount);
    CHECK(!fixture.output.contains("JOB_END"));
    fixture.run("job");
    CHECK(fixture.output.contains("attempts=1"));
    CHECK(fixture.output.contains("no_data=1"));

    fixture.scripted.advanceTimeMs(60U);
    fixture.clearOutput();
    max31865_cli::service();
    CHECK_EQ(1U, fixture.device.health().sampleFrameSuccessCount);
    CHECK(fixture.output.contains("sample source=stream"));
    CHECK(!fixture.output.contains("JOB_END"));

    fixture.run("job");
    CHECK(fixture.output.contains("attempts=2 successes=1"));
    CHECK(fixture.output.contains("no_data=1"));
    fixture.scripted.advanceTimeMs(60U);
    fixture.clearOutput();
    max31865_cli::service();
    CHECK_EQ(2U, fixture.device.health().sampleFrameSuccessCount);
    CHECK(fixture.output.contains("attempts=3 successes=2"));
    CHECK(fixture.output.contains("JOB_END command_id=1 kind=stream ok=1"));
    CHECK(!fixture.scripted.device().conversionRunning());

    fixture.run("stream 1 60 confirm");
    fixture.clearIo();
    CHECK(fixture.scripted.addFailure(makeFailure(
        EventKind::ReadDrdy,
        1U,
        MAX31865Error::GpioFailed)));
    CHECK(fixture.scripted.addFailure(makeFailure(
        EventKind::ReadDrdy,
        2U,
        MAX31865Error::GpioFailed)));
    fixture.clearOutput();
    max31865_cli::service();
    CHECK(!fixture.output.contains("JOB_END"));
    fixture.scripted.advanceTimeMs(60U);
    fixture.clearOutput();
    max31865_cli::service();
    CHECK(fixture.output.contains("reason=attempt_limit"));
    CHECK(fixture.output.contains("kind=stream ok=0"));
    CHECK(!fixture.scripted.device().conversionRunning());
    fixture.scripted.clearFailures();

    fixture.run("stream 100 60 confirm");
    CHECK(fixture.scripted.device().continuousConversion());
    fixture.clearIo();
    fixture.run("cancel confirm");
    CHECK(fixture.output.contains("reason=cancelled"));
    CHECK(!fixture.scripted.device().conversionRunning());
    CHECK(fixture.scripted.eventCount() > 0U);
    CHECK(fixture.scripted.eventCount() < 64U);
    CHECK_EQ(
        fixture.observer.counters.lockCalls,
        fixture.observer.counters.unlockCalls);

    // With no DRDY the core deliberately adds a one-millisecond guard to the
    // first elapsed-time horizon. The attempt ceiling must include it so a
    // 1 ms stream does not terminate one poll before its first valid sample.
    CliFixture noDrdy(false);
    CHECK(noDrdy.beginDirect());
    CHECK(noDrdy.scripted.device().enqueueSample(7200U));
    noDrdy.run("stream 1 1 confirm");
    max31865_cli::service(); // t=0, first NoData attempt
    for (uint32_t elapsed = 1U; elapsed <= 55U; ++elapsed)
    {
        noDrdy.scripted.advanceTimeMs(1U);
        max31865_cli::service();
    }
    noDrdy.run("job");
    CHECK(noDrdy.output.contains("active=1"));
    CHECK(noDrdy.output.contains("attempt_limit=57"));
    CHECK(noDrdy.output.contains("attempts=56"));
    noDrdy.scripted.advanceTimeMs(1U);
    noDrdy.clearOutput();
    max31865_cli::service();
    CHECK(noDrdy.output.contains("JOB_END"));
    CHECK(noDrdy.output.contains("reason=complete"));
    CHECK(noDrdy.output.contains("successes=1"));
    return true;
}

bool asynchronousJobResultsRetainTheStartingCommandId()
{
    CliFixture fixture;
    CHECK(fixture.beginDirect());
    fixture.run("format machine");
    fixture.run("stress 2 confirm");
    CHECK(fixture.output.contains("JOB_BEGIN command_id=2 kind=stress"));
    fixture.run("job");
    CHECK(fixture.output.contains("COMMAND_BEGIN id=3 command=job"));

    fixture.clearIo();
    CHECK(fixture.scripted.addFailure(makeFailure(
        EventKind::Transfer,
        1U,
        MAX31865Error::SpiTransferFailed)));
    fixture.clearOutput();
    max31865_cli::service();
    CHECK(fixture.output.contains("REC result id=2 op=job_step ok=0"));
    CHECK(!fixture.output.contains("REC result id=3 op=job_step"));

    fixture.run("cancel confirm");
    CHECK(fixture.output.contains("COMMAND_BEGIN id=4 command=cancel"));
    CHECK(fixture.output.contains(
        "REC result id=2 op=job_first_failure ok=0"));
    const char *firstFailure = strstr(
        fixture.output.bytes, "REC result id=2 op=job_first_failure ok=0");
    const char *jobEnd = strstr(
        fixture.output.bytes, "JOB_END command_id=2 kind=stress");
    CHECK(firstFailure != nullptr && jobEnd != nullptr && firstFailure < jobEnd);
    CHECK(fixture.output.contains("JOB_END command_id=2 kind=stress"));
    CHECK(fixture.output.contains(
        "REC result id=4 op=cancel ok=0"));
    CHECK(fixture.output.contains(
        "COMMAND_END id=4 ok=0 code=18 name=ConfigurationUnknown"));
    return true;
}

bool observerCountersMatchTheRealTransportAndFailures()
{
    max31865_cli::TransportObserver emptyObserver{};
    const MAX31865Transport empty{};
    const MAX31865Transport wrapped =
        max31865_cli::observeTransport(emptyObserver, empty);
    CHECK_EQ(nullptr, wrapped.lockBus);
    CHECK_EQ(nullptr, wrapped.unlockBus);
    CHECK_EQ(nullptr, wrapped.setChipSelect);
    CHECK_EQ(nullptr, wrapped.transfer);
    CHECK_EQ(nullptr, wrapped.readPin);
    CHECK_EQ(nullptr, wrapped.nowMs);
    CHECK_EQ(nullptr, wrapped.nowUs);
    CHECK_EQ(nullptr, wrapped.sleepMs);
    CHECK_EQ(nullptr, wrapped.delayUs);
    MAX31865 unboundDevice;
    MAX31865BeginConfig invalidConfig = max31865DefaultBeginConfig();
    invalidConfig.transport = wrapped;
    CHECK_EQ(
        MAX31865Error::InvalidArgument,
        unboundDevice.begin(invalidConfig).code);
    CHECK_EQ(0U, emptyObserver.counters.callbackFailures);

    CliFixture fixture;
    CHECK(fixture.beginDirect());
    fixture.clearIo();
    fixture.run("reg read 0");
    CHECK(fixture.output.contains("[OK] reg_read"));
    CHECK_EQ(1U, fixture.observer.counters.lockCalls);
    CHECK_EQ(1U, fixture.observer.counters.unlockCalls);
    CHECK_EQ(1U, fixture.observer.counters.chipSelectAssertCalls);
    CHECK_EQ(1U, fixture.observer.counters.chipSelectDeassertCalls);
    CHECK_EQ(1U, fixture.observer.counters.transferCalls);
    CHECK_EQ(2U, fixture.observer.counters.transferBytes);
    CHECK_EQ(0U, fixture.observer.counters.transferFailures);
    CHECK_EQ(0U, fixture.observer.counters.callbackFailures);
    CHECK_EQ(
        fixture.observer.counters.transferCalls,
        fixture.scripted.matchingEventCount(EventKind::Transfer));

    fixture.clearIo();
    CHECK(fixture.scripted.addFailure(makeFailure(
        EventKind::Transfer,
        1U,
        MAX31865Error::SpiTransferFailed)));
    fixture.run("reg read 0");
    CHECK(fixture.output.contains("name=SpiTransferFailed"));
    CHECK_EQ(1U, fixture.observer.counters.transferCalls);
    CHECK_EQ(1U, fixture.observer.counters.transferFailures);
    CHECK_EQ(1U, fixture.observer.counters.callbackFailures);
    CHECK_EQ(1U, fixture.device.health().spiTransferFailureCount);

    fixture.run("counters show");
    CHECK(fixture.output.contains("transfer_fail=1"));
    CHECK(fixture.output.occurrences("transfer_fail=1") >= 2U);
    fixture.run("counters clear confirm");
    CHECK(fixture.output.contains("[OK] counters_clear"));
    CHECK_EQ(0U, fixture.observer.counters.transferCalls);
    CHECK_EQ(0U, fixture.observer.counters.transferFailures);
    CHECK_EQ(0U, fixture.device.health().spiTransferFailureCount);
    CHECK_EQ(0U, fixture.device.health().trackedFailureCount);
    return true;
}

} // namespace

int main()
{
    const TestCase tests[] = {
        {"table help and command correlation are complete",
         tableHelpAndCommandCorrelationAreComplete},
        {"metadata telemetry and timeout are explicit",
         metadataTelemetryAndTimeoutAreExplicit},
        {"confirmation rejection is exact and performs no I/O",
         confirmationRejectionIsExactAndPerformsNoIo},
        {"parser and serial shell enforce finite bounds",
         parserAndSerialShellEnforceFiniteBounds},
        {"machine mode produces stable records and suppresses prompt",
         machineModeProducesStableRecordsAndSuppressesPrompt},
        {"successful machine commands emit one correlated result",
         successfulMachineCommandsEmitOneCorrelatedResult},
        {"lifecycle commands begin probe recover and end cleanly",
         lifecycleCommandsBeginProbeRecoverAndEndCleanly},
        {"sample and local conversion commands use expected paths",
         sampleAndLocalConversionCommandsUseTheExpectedPaths},
        {"fault threshold and register commands reach typed APIs",
         faultThresholdAndRegisterCommandsReachTypedDriverApis},
        {"register span reads are bounded and report DRDY acknowledgement",
         registerSpanReadsAreBoundedAndReportDrdyAcknowledgement},
        {"stress job advances one step and can be cancelled",
         stressJobAdvancesOneStepAndCanBeCancelled},
        {"stress timeout runs bounded one-shot cleanup",
         stressTimeoutRunsBoundedOneShotCleanup},
        {"stream job is cooperative and cleanup is bounded",
         streamJobIsCooperativeAndCleanupIsBounded},
        {"asynchronous job results retain starting command ID",
         asynchronousJobResultsRetainTheStartingCommandId},
        {"observer counters match real transport and failures",
         observerCountersMatchTheRealTransportAndFailures}};
    return runTests(tests, sizeof(tests) / sizeof(tests[0]));
}
