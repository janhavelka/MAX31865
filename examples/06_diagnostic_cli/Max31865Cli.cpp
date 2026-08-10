#include "Max31865Cli.h"

#include <errno.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if !defined(MAX31865_CLI_GIT_COMMIT)
#define MAX31865_CLI_GIT_COMMIT "unknown"
#endif
#if !defined(MAX31865_CLI_GIT_STATUS)
#define MAX31865_CLI_GIT_STATUS "unknown"
#endif
#if !defined(MAX31865_CLI_TARGET)
#define MAX31865_CLI_TARGET "unknown"
#endif
#if !defined(MAX31865_CLI_FRAMEWORK)
#define MAX31865_CLI_FRAMEWORK "unknown"
#endif
#if !defined(MAX31865_CLI_BUILD_KIND)
#define MAX31865_CLI_BUILD_KIND "unknown"
#endif

namespace max31865_cli
{
namespace
{
enum class OutputFormat : uint8_t
{
    Human = 0,
    Machine
};

enum class CommandSafety : uint8_t
{
    Safe = 0,
    ConfirmMutation,
    Mixed
};

enum class ExecutionKind : uint8_t
{
    CacheOnly = 0,
    LocalMath,
    DeviceIo,
    CliJob
};

enum class HelpSection : uint8_t
{
    General = 0,
    Lifecycle,
    Configuration,
    Acquisition,
    Faults,
    Diagnostics
};

struct CommandSpec
{
    const char *name;
    const char *arguments;
    const char *summary;
    HelpSection section;
    CommandSafety safety;
    ExecutionKind execution;
    bool allowedDuringJob;
};

static constexpr CommandSpec COMMANDS[] = {
    {"help", "[command]", "Show the command catalog or one command", HelpSection::General, CommandSafety::Safe, ExecutionKind::CacheOnly, true},
    {"format", "human|machine", "Select interactive or REC record output", HelpSection::General, CommandSafety::Safe, ExecutionKind::CacheOnly, true},
    {"version", "", "Show library and diagnostic build identity", HelpSection::General, CommandSafety::Safe, ExecutionKind::CacheOnly, true},
    {"wiring", "", "Show application-owned SPI/control wiring", HelpSection::General, CommandSafety::Safe, ExecutionKind::CacheOnly, true},
    {"timeout", "[milliseconds]", "Show or set the default CLI operation timeout", HelpSection::General, CommandSafety::Safe, ExecutionKind::CacheOnly, true},
    {"telemetry", "", "Show uptime and optional free-heap telemetry", HelpSection::General, CommandSafety::Safe, ExecutionKind::CacheOnly, true},
    {"state", "", "Show compact lifecycle and active-job state", HelpSection::Lifecycle, CommandSafety::Safe, ExecutionKind::CacheOnly, true},
    {"health", "", "Show passive health and all driver counters", HelpSection::Lifecycle, CommandSafety::Safe, ExecutionKind::CacheOnly, true},
    {"counters", "show|clear confirm", "Show or clear transport/driver counters", HelpSection::Lifecycle, CommandSafety::Mixed, ExecutionKind::CacheOnly, true},
    {"begin", "confirm", "Bind and initialize using the stored configuration", HelpSection::Lifecycle, CommandSafety::ConfirmMutation, ExecutionKind::DeviceIo, false},
    {"end", "confirm", "Zero-I/O unbind and local-state reset", HelpSection::Lifecycle, CommandSafety::ConfirmMutation, ExecutionKind::CacheOnly, false},
    {"recover", "[timeout_ms] confirm", "Reapply and verify the desired image", HelpSection::Lifecycle, CommandSafety::ConfirmMutation, ExecutionKind::DeviceIo, false},
    {"probe", "", "Read-only raw writable-image probe", HelpSection::Lifecycle, CommandSafety::Safe, ExecutionKind::DeviceIo, false},
    {"settings", "", "Read typed live configuration and thresholds", HelpSection::Configuration, CommandSafety::Safe, ExecutionKind::DeviceIo, false},
    {"offline", "threshold confirm", "Set passive-health offline threshold", HelpSection::Configuration, CommandSafety::ConfirmMutation, ExecutionKind::CacheOnly, false},
    {"bias", "on|off confirm", "Set the configured idle VBIAS state", HelpSection::Configuration, CommandSafety::ConfirmMutation, ExecutionKind::DeviceIo, false},
    {"wire", "2|3|4 confirm", "Set RTD lead-compensation mode", HelpSection::Configuration, CommandSafety::ConfirmMutation, ExecutionKind::DeviceIo, false},
    {"filter", "50|60 confirm", "Set digital notch frequency", HelpSection::Configuration, CommandSafety::ConfirmMutation, ExecutionKind::DeviceIo, false},
    {"rtd", "get | set rref r0 tau_us min_c max_c confirm", "Read/set local RTD scaling and domain", HelpSection::Configuration, CommandSafety::Mixed, ExecutionKind::LocalMath, false},
    {"cvd", "set a b c confirm", "Set custom CVD coefficients", HelpSection::Configuration, CommandSafety::ConfirmMutation, ExecutionKind::LocalMath, false},
    {"start", "confirm", "Start continuous conversion", HelpSection::Acquisition, CommandSafety::ConfirmMutation, ExecutionKind::DeviceIo, false},
    {"trigger", "confirm", "Arm one split one-shot conversion", HelpSection::Acquisition, CommandSafety::ConfirmMutation, ExecutionKind::DeviceIo, false},
    {"stop", "confirm", "Stop conversion and restore idle VBIAS", HelpSection::Acquisition, CommandSafety::ConfirmMutation, ExecutionKind::DeviceIo, true},
    {"ready", "", "Check the selected readiness source once", HelpSection::Acquisition, CommandSafety::Safe, ExecutionKind::DeviceIo, true},
    {"poll", "", "Poll and consume one fresh armed sample", HelpSection::Acquisition, CommandSafety::Safe, ExecutionKind::DeviceIo, false},
    {"read", "", "Read buffered RTD registers immediately", HelpSection::Acquisition, CommandSafety::Safe, ExecutionKind::DeviceIo, false},
    {"wait", "[timeout_ms]", "Wait for and consume an armed result", HelpSection::Acquisition, CommandSafety::Safe, ExecutionKind::DeviceIo, false},
    {"one", "[timeout_ms] confirm", "Run one complete bounded one-shot", HelpSection::Acquisition, CommandSafety::ConfirmMutation, ExecutionKind::DeviceIo, false},
    {"stream", "count interval_ms confirm", "Start a cooperative continuous-sample job", HelpSection::Acquisition, CommandSafety::ConfirmMutation, ExecutionKind::CliJob, false},
    {"stress", "count confirm", "Start a cooperative one-shot stress job", HelpSection::Acquisition, CommandSafety::ConfirmMutation, ExecutionKind::CliJob, false},
    {"job", "", "Show cooperative job progress", HelpSection::Acquisition, CommandSafety::Safe, ExecutionKind::CacheOnly, true},
    {"cancel", "confirm", "Cancel the active job and run bounded cleanup", HelpSection::Acquisition, CommandSafety::ConfirmMutation, ExecutionKind::CliJob, true},
    {"faults", "", "Read and decode latched fault status", HelpSection::Faults, CommandSafety::Safe, ExecutionKind::DeviceIo, false},
    {"fault-clear", "confirm", "Clear latched faults", HelpSection::Faults, CommandSafety::ConfirmMutation, ExecutionKind::DeviceIo, false},
    {"fault-auto", "confirm", "Run a fresh automatic fault cycle (RC <= 100 us)", HelpSection::Faults, CommandSafety::ConfirmMutation, ExecutionKind::DeviceIo, false},
    {"fault-manual", "confirm", "Run a fresh correctly timed manual fault cycle", HelpSection::Faults, CommandSafety::ConfirmMutation, ExecutionKind::DeviceIo, false},
    {"thresholds", "get raw|ohms|c | set raw|ohms|c low high confirm", "Read/set typed inclusive thresholds", HelpSection::Faults, CommandSafety::Mixed, ExecutionKind::DeviceIo, false},
    {"reg", "read addr [length] | dump | write addr value confirm | verify addr value confirm | test confirm | defaults confirm", "Validated diagnostics; RTD reads acknowledge DRDY", HelpSection::Diagnostics, CommandSafety::Mixed, ExecutionKind::DeviceIo, false},
    {"convert", "code n | resistance ohms | temperature c", "Run local status-returning conversion helpers", HelpSection::Diagnostics, CommandSafety::Safe, ExecutionKind::LocalMath, true}};

enum class JobKind : uint8_t
{
    None = 0,
    Stream,
    Stress
};

struct JobState
{
    JobKind kind;
    uint32_t commandId;
    uint32_t target;
    uint32_t attemptLimit;
    uint32_t attempts;
    uint32_t successes;
    uint32_t failures;
    uint32_t noData;
    uint32_t intervalMs;
    uint32_t operationTimeoutMs;
    uint32_t nextDueMs;
    uint32_t startedMs;
    bool ownsContinuous;
    bool hasFirstFailure;
    MAX31865Status firstFailure;
};

Context gContext{};
bool gBound = false;
OutputFormat gFormat = OutputFormat::Human;
uint32_t gCommandId = 0U;
JobState gJob{};
uint32_t gOperationTimeoutMs = 250U;
bool gDispatchActive = false;
bool gDispatchOk = true;
bool gDispatchResultEmitted = false;
MAX31865Status gDispatchStatus = MAX31865Status::Ok();

void saturatingIncrement(uint32_t &value)
{
    if (value != UINT32_MAX)
    {
        ++value;
    }
}

void saturatingAdd(uint32_t &value, uint32_t amount)
{
    const uint32_t room = UINT32_MAX - value;
    value += amount > room ? room : amount;
}

bool due(uint32_t nowMs, uint32_t deadlineMs)
{
    return static_cast<int32_t>(nowMs - deadlineMs) >= 0;
}

uint32_t nowMs()
{
    return gBound && gContext.platform.nowMs != nullptr
        ? gContext.platform.nowMs(gContext.platform.user)
        : 0U;
}

void writeBytes(const char *data, size_t length)
{
    if (gBound && gContext.platform.write != nullptr && data != nullptr &&
        length > 0U)
    {
        gContext.platform.write(gContext.platform.user, data, length);
    }
}

void emit(const char *text)
{
    if (text != nullptr)
    {
        writeBytes(text, strlen(text));
    }
}

void emitf(const char *format, ...)
{
    char buffer[512]{};
    va_list args;
    va_start(args, format);
    const int result = vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    if (result <= 0)
    {
        return;
    }
    const size_t length = static_cast<size_t>(result) < sizeof(buffer)
        ? static_cast<size_t>(result)
        : sizeof(buffer) - 1U;
    writeBytes(buffer, length);
}

const char *safetyName(CommandSafety safety)
{
    switch (safety)
    {
        case CommandSafety::Safe: return "safe";
        case CommandSafety::ConfirmMutation: return "confirm_mutation";
        case CommandSafety::Mixed: return "mixed_subcommands";
        default: return "unknown";
    }
}

const char *executionName(ExecutionKind execution)
{
    switch (execution)
    {
        case ExecutionKind::CacheOnly: return "cache_only";
        case ExecutionKind::LocalMath: return "local_math";
        case ExecutionKind::DeviceIo: return "device_io";
        case ExecutionKind::CliJob: return "cli_job";
        default: return "unknown";
    }
}

const char *sectionName(HelpSection section)
{
    switch (section)
    {
        case HelpSection::General: return "general";
        case HelpSection::Lifecycle: return "lifecycle";
        case HelpSection::Configuration: return "configuration";
        case HelpSection::Acquisition: return "acquisition";
        case HelpSection::Faults: return "faults";
        case HelpSection::Diagnostics: return "diagnostics";
        default: return "unknown";
    }
}

const char *jobName(JobKind kind)
{
    switch (kind)
    {
        case JobKind::None: return "none";
        case JobKind::Stream: return "stream";
        case JobKind::Stress: return "stress";
        default: return "unknown";
    }
}

const CommandSpec *findCommand(const char *name)
{
    if (name == nullptr)
    {
        return nullptr;
    }
    for (const CommandSpec &spec : COMMANDS)
    {
        if (strcmp(name, spec.name) == 0)
        {
            return &spec;
        }
    }
    return nullptr;
}

bool jobActive()
{
    return gJob.kind != JobKind::None;
}

MAX31865Status invalid(const char *message)
{
    return MAX31865Status::Error(MAX31865Error::InvalidArgument, message);
}

bool reportStatusForCommand(
    const char *operation,
    const MAX31865Status &status,
    uint32_t commandId)
{
    if (gDispatchActive && commandId == gCommandId)
    {
        gDispatchResultEmitted = true;
        if (!status.ok())
        {
            gDispatchOk = false;
            if (gDispatchStatus.ok())
            {
                gDispatchStatus = status;
            }
        }
    }
    if (gFormat == OutputFormat::Machine)
    {
        emitf(
            "REC result id=%lu op=%s ok=%u code=%u name=%s detail=%ld\n",
            static_cast<unsigned long>(commandId),
            operation,
            status.ok() ? 1U : 0U,
            static_cast<unsigned>(status.code),
            max31865ErrorName(status.code),
            static_cast<long>(status.detail));
    }
    else
    {
        emitf(
            "[%s] %s: %s (%u), detail=%ld, %s\n",
            status.ok() ? "OK" : "ERR",
            operation,
            max31865ErrorName(status.code),
            static_cast<unsigned>(status.code),
            static_cast<long>(status.detail),
            status.msg == nullptr ? "" : status.msg);
    }
    return status.ok();
}

bool reportStatus(const char *operation, const MAX31865Status &status)
{
    return reportStatusForCommand(operation, status, gCommandId);
}

void reject(const char *message)
{
    (void)reportStatus("parse", invalid(message));
}

bool parseUnsigned(
    const char *text,
    uint32_t minimum,
    uint32_t maximum,
    uint32_t &out)
{
    if (text == nullptr || text[0] == '\0' || text[0] == '-')
    {
        return false;
    }
    errno = 0;
    char *end = nullptr;
    const unsigned long parsed = strtoul(text, &end, 0);
    if (errno != 0 || end == text || *end != '\0' ||
        parsed > static_cast<unsigned long>(UINT32_MAX))
    {
        return false;
    }
    const uint32_t value = static_cast<uint32_t>(parsed);
    if (value < minimum || value > maximum)
    {
        return false;
    }
    out = value;
    return true;
}

bool parseFloat(const char *text, float &out)
{
    if (text == nullptr || text[0] == '\0')
    {
        return false;
    }
    errno = 0;
    char *end = nullptr;
    const float value = strtof(text, &end);
    if (errno != 0 || end == text || *end != '\0' ||
        isfinite(static_cast<double>(value)) == 0)
    {
        return false;
    }
    out = value;
    return true;
}

bool finalConfirm(size_t argc, char *const argv[])
{
    return argc > 1U && strcmp(argv[argc - 1U], "confirm") == 0;
}

void printHelp(const CommandSpec *only)
{
    if (only != nullptr)
    {
        if (gFormat == OutputFormat::Machine)
        {
            emitf(
                "REC help command=%s args=\"%s\" section=%s safety=%s execution=%s during_job=%u\n",
                only->name,
                only->arguments,
                sectionName(only->section),
                safetyName(only->safety),
                executionName(only->execution),
                only->allowedDuringJob ? 1U : 0U);
        }
        else
        {
            emitf(
                "%-13s %-52s %s [section=%s, safety=%s, execution=%s]\n",
                only->name,
                only->arguments,
                only->summary,
                sectionName(only->section),
                safetyName(only->safety),
                executionName(only->execution));
        }
        return;
    }

    if (gFormat == OutputFormat::Human)
    {
        emit("MAX31865 bounded diagnostic CLI. Mutations require `confirm`.\n");
    }
    for (const CommandSpec &spec : COMMANDS)
    {
        printHelp(&spec);
    }
}

MAX31865Status observerLock(void *user, uint32_t timeoutMs)
{
    TransportObserver *observer = static_cast<TransportObserver *>(user);
    saturatingIncrement(observer->counters.lockCalls);
    const MAX31865Status status = observer->downstream.lockBus(
        observer->downstream.user, timeoutMs);
    if (!status.ok())
    {
        saturatingIncrement(observer->counters.callbackFailures);
    }
    return status;
}

void observerUnlock(void *user)
{
    TransportObserver *observer = static_cast<TransportObserver *>(user);
    saturatingIncrement(observer->counters.unlockCalls);
    observer->downstream.unlockBus(observer->downstream.user);
}

MAX31865Status observerChipSelect(void *user, bool asserted)
{
    TransportObserver *observer = static_cast<TransportObserver *>(user);
    saturatingIncrement(
        asserted
            ? observer->counters.chipSelectAssertCalls
            : observer->counters.chipSelectDeassertCalls);
    const MAX31865Status status = observer->downstream.setChipSelect(
        observer->downstream.user, asserted);
    if (!status.ok())
    {
        saturatingIncrement(observer->counters.callbackFailures);
    }
    return status;
}

MAX31865Status observerTransfer(
    void *user,
    const uint8_t *tx,
    uint8_t *rx,
    size_t length,
    uint32_t timeoutMs)
{
    TransportObserver *observer = static_cast<TransportObserver *>(user);
    saturatingIncrement(observer->counters.transferCalls);
    const size_t bounded = length > UINT32_MAX
        ? static_cast<size_t>(UINT32_MAX)
        : length;
    saturatingAdd(
        observer->counters.transferBytes,
        static_cast<uint32_t>(bounded));
    const MAX31865Status status = observer->downstream.transfer(
        observer->downstream.user, tx, rx, length, timeoutMs);
    if (!status.ok())
    {
        saturatingIncrement(observer->counters.transferFailures);
        saturatingIncrement(observer->counters.callbackFailures);
    }
    return status;
}

MAX31865Status observerReadPin(
    void *user,
    MAX31865Pin pin,
    bool *level)
{
    TransportObserver *observer = static_cast<TransportObserver *>(user);
    saturatingIncrement(observer->counters.drdyReadCalls);
    const MAX31865Status status = observer->downstream.readPin(
        observer->downstream.user, pin, level);
    if (!status.ok())
    {
        saturatingIncrement(observer->counters.callbackFailures);
    }
    return status;
}

uint32_t observerNowMs(void *user)
{
    TransportObserver *observer = static_cast<TransportObserver *>(user);
    return observer->downstream.nowMs(observer->downstream.user);
}

uint32_t observerNowUs(void *user)
{
    TransportObserver *observer = static_cast<TransportObserver *>(user);
    return observer->downstream.nowUs(observer->downstream.user);
}

void observerSleepMs(void *user, uint32_t delayMs)
{
    TransportObserver *observer = static_cast<TransportObserver *>(user);
    observer->downstream.sleepMs(observer->downstream.user, delayMs);
}

void observerDelayUs(void *user, uint32_t delayUs)
{
    TransportObserver *observer = static_cast<TransportObserver *>(user);
    observer->downstream.delayUs(observer->downstream.user, delayUs);
}

void printFault(const MAX31865FaultStatus &fault)
{
    const char *prefix = gFormat == OutputFormat::Machine
        ? "REC fault"
        : "fault";
    emitf(
        "%s raw=0x%02X high=%u low=%u refin_high=%u refin_low=%u rtdin_low=%u over_under_voltage=%u\n",
        prefix,
        static_cast<unsigned>(fault.raw),
        fault.highThreshold ? 1U : 0U,
        fault.lowThreshold ? 1U : 0U,
        fault.refinHigh ? 1U : 0U,
        fault.refinLow ? 1U : 0U,
        fault.rtdinLow ? 1U : 0U,
        fault.overUnderVoltage ? 1U : 0U);
}

void printSample(const MAX31865Sample &sample, const char *source)
{
    const char *prefix = gFormat == OutputFormat::Machine
        ? "REC sample"
        : "sample";
    emitf(
        "%s source=%s count=%lu raw_reg=0x%04X code=%u resistance_ohms=%.6f temperature_c=%.6f flags=0x%02X channel=%u read_us=%lu ready_us=%lu\n",
        prefix,
        source,
        static_cast<unsigned long>(sample.sampleCounter),
        static_cast<unsigned>(sample.rawRegister),
        static_cast<unsigned>(sample.rawCode),
        static_cast<double>(sample.resistanceOhms),
        static_cast<double>(sample.temperatureC),
        static_cast<unsigned>(sample.flags),
        static_cast<unsigned>(sample.channelId),
        static_cast<unsigned long>(sample.readTimestampUs),
        static_cast<unsigned long>(sample.readyTimestampUs));
    if ((sample.flags & MAX31865_SAMPLE_FLAG_FAULT_STATUS) != 0U)
    {
        printFault(sample.faultStatus);
    }
}

void printJob()
{
    const bool active = jobActive();
    const uint32_t elapsedMs = active ? nowMs() - gJob.startedMs : 0U;
    const char *prefix = gFormat == OutputFormat::Machine ? "REC job" : "job";
    emitf(
        "%s active=%u kind=%s command_id=%lu target=%lu attempt_limit=%lu attempts=%lu successes=%lu failures=%lu no_data=%lu interval_ms=%lu operation_timeout_ms=%lu elapsed_ms=%lu\n",
        prefix,
        active ? 1U : 0U,
        jobName(gJob.kind),
        static_cast<unsigned long>(gJob.commandId),
        static_cast<unsigned long>(gJob.target),
        static_cast<unsigned long>(gJob.attemptLimit),
        static_cast<unsigned long>(gJob.attempts),
        static_cast<unsigned long>(gJob.successes),
        static_cast<unsigned long>(gJob.failures),
        static_cast<unsigned long>(gJob.noData),
        static_cast<unsigned long>(gJob.intervalMs),
        static_cast<unsigned long>(gJob.operationTimeoutMs),
        static_cast<unsigned long>(elapsedMs));
}

void printHealth()
{
    const MAX31865Health health = gContext.device->health();
    const char *prefix = gFormat == OutputFormat::Machine
        ? "REC health"
        : "health";
    emitf(
        "%s state=%s driver=%s online=%u config_known=%u consecutive=%u offline_threshold=%u last_code=%u last_name=%s last_detail=%ld\n",
        prefix,
        max31865StateName(health.state),
        max31865DriverStateName(health.driverState),
        health.online ? 1U : 0U,
        health.configurationKnown ? 1U : 0U,
        static_cast<unsigned>(health.consecutiveFailures),
        static_cast<unsigned>(health.offlineThreshold),
        static_cast<unsigned>(health.lastOperation.code),
        max31865ErrorName(health.lastOperation.code),
        static_cast<long>(health.lastOperation.detail));
    emitf(
        "%s_counts tracked_ok=%lu tracked_fail=%lu frame_attempt=%lu frame_ok=%lu frame_fail=%lu no_data=%lu dropped=%lu overruns=%lu faults=%lu threshold_faults=%lu reference_faults=%lu voltage_faults=%lu\n",
        prefix,
        static_cast<unsigned long>(health.trackedSuccessCount),
        static_cast<unsigned long>(health.trackedFailureCount),
        static_cast<unsigned long>(health.sampleFrameAttemptCount),
        static_cast<unsigned long>(health.sampleFrameSuccessCount),
        static_cast<unsigned long>(health.sampleFrameFailureCount),
        static_cast<unsigned long>(health.noDataCount),
        static_cast<unsigned long>(health.droppedSampleCount),
        static_cast<unsigned long>(health.overrunCount),
        static_cast<unsigned long>(health.faultObservationCount),
        static_cast<unsigned long>(health.thresholdFaultObservationCount),
        static_cast<unsigned long>(health.referenceFaultObservationCount),
        static_cast<unsigned long>(health.voltageFaultObservationCount));
    emitf(
        "%s_io lock_timeout=%lu lock_fail=%lu transfer_fail=%lu cs_fail=%lu gpio_fail=%lu drdy_timeout=%lu op_timeout=%lu last_fault_valid=%u last_fault=0x%02X last_ok_valid=%u last_ok_ms=%lu last_error_valid=%u last_error_ms=%lu\n",
        prefix,
        static_cast<unsigned long>(health.busLockTimeoutCount),
        static_cast<unsigned long>(health.busLockFailureCount),
        static_cast<unsigned long>(health.spiTransferFailureCount),
        static_cast<unsigned long>(health.chipSelectFailureCount),
        static_cast<unsigned long>(health.gpioFailureCount),
        static_cast<unsigned long>(health.drdyTimeoutCount),
        static_cast<unsigned long>(health.operationTimeoutCount),
        health.hasLastFaultStatus ? 1U : 0U,
        static_cast<unsigned>(health.lastFaultStatus),
        health.hasLastOkMs ? 1U : 0U,
        static_cast<unsigned long>(health.lastOkMs),
        health.hasLastErrorMs ? 1U : 0U,
        static_cast<unsigned long>(health.lastErrorMs));
}

void printTransportCounters()
{
    const TransportCounters &counters = gContext.observer->counters;
    const char *prefix = gFormat == OutputFormat::Machine
        ? "REC transport"
        : "transport";
    emitf(
        "%s lock=%lu unlock=%lu cs_assert=%lu cs_deassert=%lu transfers=%lu bytes=%lu transfer_fail=%lu drdy_reads=%lu callback_fail=%lu\n",
        prefix,
        static_cast<unsigned long>(counters.lockCalls),
        static_cast<unsigned long>(counters.unlockCalls),
        static_cast<unsigned long>(counters.chipSelectAssertCalls),
        static_cast<unsigned long>(counters.chipSelectDeassertCalls),
        static_cast<unsigned long>(counters.transferCalls),
        static_cast<unsigned long>(counters.transferBytes),
        static_cast<unsigned long>(counters.transferFailures),
        static_cast<unsigned long>(counters.drdyReadCalls),
        static_cast<unsigned long>(counters.callbackFailures));
}

MAX31865Status finishJob(const char *reason)
{
    if (!jobActive())
    {
        return MAX31865Status::Error(
            MAX31865Error::InvalidState,
            "no CLI job is active");
    }
    const JobState completed = gJob;
    MAX31865Status cleanup = MAX31865Status::Ok();
    const MAX31865State deviceState = gContext.device->state();
    if (deviceState != MAX31865State::Uninitialized &&
        (completed.ownsContinuous || deviceState != MAX31865State::Ready))
    {
        const uint32_t minimumOneShotCleanupMs =
            gContext.device->singleConversionTimeMs() + 2U;
        const uint32_t cleanupTimeoutMs =
            completed.operationTimeoutMs > minimumOneShotCleanupMs
            ? completed.operationTimeoutMs
            : minimumOneShotCleanupMs;
        cleanup = gContext.device->stop(cleanupTimeoutMs);
    }
    gJob = JobState{};
    const char *prefix = gFormat == OutputFormat::Machine
        ? "REC job_end"
        : "job_end";
    emitf(
        "%s kind=%s reason=%s target=%lu attempt_limit=%lu attempts=%lu successes=%lu failures=%lu no_data=%lu elapsed_ms=%lu cleanup_code=%u cleanup_name=%s\n",
        prefix,
        jobName(completed.kind),
        reason,
        static_cast<unsigned long>(completed.target),
        static_cast<unsigned long>(completed.attemptLimit),
        static_cast<unsigned long>(completed.attempts),
        static_cast<unsigned long>(completed.successes),
        static_cast<unsigned long>(completed.failures),
        static_cast<unsigned long>(completed.noData),
        static_cast<unsigned long>(nowMs() - completed.startedMs),
        static_cast<unsigned>(cleanup.code),
        max31865ErrorName(cleanup.code));
    const bool cancelled = strcmp(reason, "cancelled") == 0 ||
                           strcmp(reason, "stopped") == 0;
    const bool targetReached = completed.kind == JobKind::Stream
        ? completed.successes >= completed.target
        : completed.attempts >= completed.target;
    if (completed.hasFirstFailure)
    {
        (void)reportStatusForCommand(
            "job_first_failure", completed.firstFailure, completed.commandId);
    }
    if (!cleanup.ok())
    {
        (void)reportStatusForCommand(
            "job_cleanup", cleanup, completed.commandId);
    }
    emitf(
        "JOB_END command_id=%lu kind=%s ok=%u failures=%lu reason=%s\n",
        static_cast<unsigned long>(completed.commandId),
        jobName(completed.kind),
        targetReached && completed.failures == 0U && cleanup.ok() &&
                !cancelled
            ? 1U
            : 0U,
        static_cast<unsigned long>(completed.failures),
        reason);
    return cleanup;
}
} // namespace

MAX31865Transport observeTransport(
    TransportObserver &observer,
    const MAX31865Transport &downstream)
{
    observer.downstream = downstream;
    observer.counters = TransportCounters{};
    MAX31865Transport transport = downstream;
    transport.user = &observer;
    transport.lockBus = downstream.lockBus != nullptr ? observerLock : nullptr;
    transport.unlockBus = downstream.unlockBus != nullptr
        ? observerUnlock
        : nullptr;
    transport.setChipSelect = downstream.setChipSelect != nullptr
        ? observerChipSelect
        : nullptr;
    transport.transfer = downstream.transfer != nullptr
        ? observerTransfer
        : nullptr;
    transport.readPin = downstream.readPin != nullptr ? observerReadPin : nullptr;
    transport.nowMs = downstream.nowMs != nullptr ? observerNowMs : nullptr;
    transport.nowUs = downstream.nowUs != nullptr ? observerNowUs : nullptr;
    transport.sleepMs = downstream.sleepMs != nullptr
        ? observerSleepMs
        : nullptr;
    transport.delayUs = downstream.delayUs != nullptr
        ? observerDelayUs
        : nullptr;
    return transport;
}

void clearTransportCounters(TransportObserver &observer)
{
    observer.counters = TransportCounters{};
}

void bind(const Context &context)
{
    gContext = context;
    gBound = context.device != nullptr && context.beginConfig != nullptr &&
             context.pins != nullptr && context.observer != nullptr &&
             context.platform.write != nullptr &&
             context.platform.nowMs != nullptr;
    gJob = JobState{};
    gCommandId = 0U;
    gOperationTimeoutMs = context.beginConfig != nullptr
        ? context.beginConfig->defaultOperationTimeoutMs
        : 250U;
}

namespace
{
uint32_t operationTimeout()
{
    return gOperationTimeoutMs;
}

bool parseOptionalTimeout(
    size_t argc,
    char *const argv[],
    size_t valueIndex,
    uint32_t &timeoutMs)
{
    timeoutMs = operationTimeout();
    return argc <= valueIndex ||
           parseUnsigned(argv[valueIndex], 1U, INT32_MAX, timeoutMs);
}

void printVersion()
{
    const char *prefix = gFormat == OutputFormat::Machine
        ? "REC version"
        : "version";
    emitf(
        "%s library=%s code=%lu git_commit=%s git_status=%s target=%s framework=%s build=%s evidence=software_only_unvalidated_hardware\n",
        prefix,
        MAX31865Version::VERSION,
        static_cast<unsigned long>(MAX31865Version::VERSION_CODE),
        MAX31865_CLI_GIT_COMMIT,
        MAX31865_CLI_GIT_STATUS,
        MAX31865_CLI_TARGET,
        MAX31865_CLI_FRAMEWORK,
        MAX31865_CLI_BUILD_KIND);
}

void printWiring()
{
    const MAX31865Pins &pins = *gContext.pins;
    const MAX31865DeviceConfig &deviceConfig =
        gContext.beginConfig->initialDeviceConfig;
    const char *prefix = gFormat == OutputFormat::Machine
        ? "REC wiring"
        : "wiring";
    emitf(
        "%s sck=%ld miso=%ld mosi=%ld cs=%ld drdy=%ld drdy_enabled=%u spi_clock_hz=%lu spi_max_hz=%lu wire=%u filter_hz=%u ownership=application\n",
        prefix,
        static_cast<long>(pins.sck),
        static_cast<long>(pins.miso),
        static_cast<long>(pins.mosi),
        static_cast<long>(pins.chipSelect),
        static_cast<long>(pins.dataReady),
        pins.dataReady != MAX31865_PIN_UNUSED ? 1U : 0U,
        static_cast<unsigned long>(gContext.spiClockHz),
        static_cast<unsigned long>(max31865_cmd::SPI_MAX_HZ),
        static_cast<unsigned>(deviceConfig.wireMode),
        deviceConfig.filter == MAX31865Filter::Hz50 ? 50U : 60U);
}

void printTimeout()
{
    const char *prefix = gFormat == OutputFormat::Machine
        ? "REC timeout"
        : "timeout";
    emitf(
        "%s milliseconds=%lu\n",
        prefix,
        static_cast<unsigned long>(gOperationTimeoutMs));
}

void printTelemetry()
{
    const bool heapValid = gContext.platform.freeHeapBytes != nullptr;
    const uint32_t heapBytes = heapValid
        ? gContext.platform.freeHeapBytes(gContext.platform.user)
        : 0U;
    const char *prefix = gFormat == OutputFormat::Machine
        ? "REC telemetry"
        : "telemetry";
    emitf(
        "%s uptime_ms=%lu free_heap_valid=%u free_heap_bytes=%lu\n",
        prefix,
        static_cast<unsigned long>(nowMs()),
        heapValid ? 1U : 0U,
        static_cast<unsigned long>(heapBytes));
}

void printState()
{
    const MAX31865Health health = gContext.device->health();
    const char *prefix = gFormat == OutputFormat::Machine
        ? "REC state"
        : "state";
    emitf(
        "%s lifecycle=%s driver=%s config_known=%u job=%s job_active=%u last_code=%u last_name=%s\n",
        prefix,
        max31865StateName(health.state),
        max31865DriverStateName(health.driverState),
        health.configurationKnown ? 1U : 0U,
        jobName(gJob.kind),
        jobActive() ? 1U : 0U,
        static_cast<unsigned>(health.lastOperation.code),
        max31865ErrorName(health.lastOperation.code));
}

void printSettings(const MAX31865Settings &settings)
{
    const char *prefix = gFormat == OutputFormat::Machine
        ? "REC settings"
        : "settings";
    emitf(
        "%s raw_config=0x%02X wire=%u filter_hz=%u bias=%u conversion=%s one_shot=%u fault_cycle=0x%02X low_code=%u high_code=%u\n",
        prefix,
        static_cast<unsigned>(settings.rawConfig),
        static_cast<unsigned>(settings.deviceConfig.wireMode),
        settings.deviceConfig.filter == MAX31865Filter::Hz50 ? 50U : 60U,
        settings.deviceConfig.biasEnabled ? 1U : 0U,
        settings.conversionMode == MAX31865ConversionMode::Continuous
            ? "continuous"
            : "normally_off",
        settings.oneShotActive ? 1U : 0U,
        static_cast<unsigned>(settings.faultCycle),
        static_cast<unsigned>(settings.deviceConfig.thresholds.lowCode),
        static_cast<unsigned>(settings.deviceConfig.thresholds.highCode));
    emitf(
        "%s_thresholds low_ohms=%.6f high_ohms=%.6f low_c_valid=%u low_c=%.6f high_c_valid=%u high_c=%.6f\n",
        prefix,
        static_cast<double>(settings.lowThresholdOhms),
        static_cast<double>(settings.highThresholdOhms),
        settings.lowThresholdTemperatureValid ? 1U : 0U,
        static_cast<double>(settings.lowThresholdC),
        settings.highThresholdTemperatureValid ? 1U : 0U,
        static_cast<double>(settings.highThresholdC));
}

void printRtd(const MAX31865RtdConfig &config)
{
    const char *prefix = gFormat == OutputFormat::Machine
        ? "REC rtd"
        : "rtd";
    emitf(
        "%s rref_ohms=%.9g r0_ohms=%.9g tau_us=%lu min_c=%.9g max_c=%.9g a=%.9g b=%.9g c=%.9g bias_settle_us=%lu\n",
        prefix,
        static_cast<double>(config.referenceResistorOhms),
        static_cast<double>(config.nominalResistanceOhms),
        static_cast<unsigned long>(config.inputFilterTimeConstantUs),
        static_cast<double>(config.minimumTemperatureC),
        static_cast<double>(config.maximumTemperatureC),
        static_cast<double>(config.coefficients.a),
        static_cast<double>(config.coefficients.b),
        static_cast<double>(config.coefficients.c),
        static_cast<unsigned long>(gContext.device->biasSettleTimeUs()));
}

void printProbe(const MAX31865DeviceInfo &info)
{
    const char *prefix = gFormat == OutputFormat::Machine
        ? "REC probe"
        : "probe";
    emitf(
        "%s config=0x%02X high_register=0x%04X low_register=0x%04X fault=0x%02X matches=%u\n",
        prefix,
        static_cast<unsigned>(info.rawConfig),
        static_cast<unsigned>(info.rawHighThresholdRegister),
        static_cast<unsigned>(info.rawLowThresholdRegister),
        static_cast<unsigned>(info.rawFaultStatus),
        info.configurationMatches ? 1U : 0U);
}

void recordJobResult(
    const MAX31865Status &status,
    const MAX31865Sample &sample,
    const char *source)
{
    saturatingIncrement(gJob.attempts);
    if (status.ok())
    {
        saturatingIncrement(gJob.successes);
        printSample(sample, source);
        return;
    }
    if (status.code == MAX31865Error::NoData)
    {
        saturatingIncrement(gJob.noData);
        return;
    }
    saturatingIncrement(gJob.failures);
    if (!gJob.hasFirstFailure)
    {
        gJob.hasFirstFailure = true;
        gJob.firstFailure = status;
    }
    if ((sample.flags & MAX31865_SAMPLE_FLAG_FRAME_VALID) != 0U)
    {
        printSample(sample, source);
    }
    (void)reportStatusForCommand("job_step", status, gJob.commandId);
}

uint32_t streamAttemptLimit(uint32_t target, uint32_t intervalMs)
{
    const uint64_t interval = static_cast<uint64_t>(intervalMs);
    const uint64_t elapsedReadinessGuard =
        gContext.beginConfig->transport.capabilities.hasDrdy ? 0ULL : 1ULL;
    const uint64_t firstConversionAttempts =
        1ULL +
        (static_cast<uint64_t>(gContext.device->singleConversionTimeMs()) +
         elapsedReadinessGuard + interval - 1ULL) /
            interval;
    const uint64_t followingConversionAttempts =
        1ULL +
        (static_cast<uint64_t>(
             gContext.device->continuousConversionTimeMs()) +
         interval - 1ULL) /
            interval;
    const uint64_t limit = firstConversionAttempts +
        static_cast<uint64_t>(target - 1U) * followingConversionAttempts;
    return limit > static_cast<uint64_t>(UINT32_MAX)
        ? UINT32_MAX
        : static_cast<uint32_t>(limit);
}

void startJob(JobKind kind, uint32_t count, uint32_t intervalMs)
{
    gJob = JobState{};
    gJob.kind = kind;
    gJob.commandId = gCommandId;
    gJob.target = count;
    gJob.attemptLimit = kind == JobKind::Stream
        ? streamAttemptLimit(count, intervalMs)
        : count;
    gJob.intervalMs = intervalMs;
    gJob.operationTimeoutMs = gOperationTimeoutMs;
    gJob.nextDueMs = nowMs();
    gJob.startedMs = nowMs();
    gJob.firstFailure = MAX31865Status::Ok();
    const char *prefix = gFormat == OutputFormat::Machine
        ? "REC job_start"
        : "job_start";
    emitf(
        "%s kind=%s command_id=%lu target=%lu attempt_limit=%lu interval_ms=%lu operation_timeout_ms=%lu\n",
        prefix,
        jobName(kind),
        static_cast<unsigned long>(gCommandId),
        static_cast<unsigned long>(count),
        static_cast<unsigned long>(gJob.attemptLimit),
        static_cast<unsigned long>(intervalMs),
        static_cast<unsigned long>(gJob.operationTimeoutMs));
    emitf(
        "JOB_BEGIN command_id=%lu kind=%s target=%lu attempt_limit=%lu interval_ms=%lu operation_timeout_ms=%lu\n",
        static_cast<unsigned long>(gCommandId),
        jobName(kind),
        static_cast<unsigned long>(count),
        static_cast<unsigned long>(gJob.attemptLimit),
        static_cast<unsigned long>(intervalMs),
        static_cast<unsigned long>(gJob.operationTimeoutMs));
}

void handleThresholds(size_t argc, char *const argv[])
{
    if (argc == 3U && strcmp(argv[1], "get") == 0)
    {
        if (strcmp(argv[2], "raw") == 0)
        {
            MAX31865FaultThresholds thresholds{};
            const MAX31865Status status =
                gContext.device->readFaultThresholdsRaw(
                    thresholds, operationTimeout());
            if (status.ok())
            {
                emitf(
                    "%s units=raw low=%u high=%u comparison=low_le_high_ge\n",
                    gFormat == OutputFormat::Machine
                        ? "REC thresholds"
                        : "thresholds",
                    static_cast<unsigned>(thresholds.lowCode),
                    static_cast<unsigned>(thresholds.highCode));
            }
            (void)reportStatus("thresholds_get_raw", status);
            return;
        }
        float low = 0.0F;
        float high = 0.0F;
        MAX31865Status status = MAX31865Status::Ok();
        if (strcmp(argv[2], "ohms") == 0)
        {
            status = gContext.device->readFaultThresholdsResistance(
                low, high, operationTimeout());
        }
        else if (strcmp(argv[2], "c") == 0)
        {
            status = gContext.device->readFaultThresholdsTemperature(
                low, high, operationTimeout());
        }
        else
        {
            reject("threshold units must be raw, ohms, or c");
            return;
        }
        if (status.ok())
        {
            emitf(
                "%s units=%s low=%.9g high=%.9g comparison=low_le_high_ge\n",
                gFormat == OutputFormat::Machine
                    ? "REC thresholds"
                    : "thresholds",
                argv[2],
                static_cast<double>(low),
                static_cast<double>(high));
        }
        (void)reportStatus("thresholds_get", status);
        return;
    }

    if (argc != 6U || strcmp(argv[1], "set") != 0 ||
        !finalConfirm(argc, argv))
    {
        reject("usage: thresholds get raw|ohms|c OR thresholds set raw|ohms|c low high confirm");
        return;
    }
    MAX31865Status status = MAX31865Status::Ok();
    if (strcmp(argv[2], "raw") == 0)
    {
        uint32_t low = 0U;
        uint32_t high = 0U;
        if (!parseUnsigned(argv[3], 0U, max31865_cmd::ADC_CODE_MAX, low) ||
            !parseUnsigned(argv[4], 0U, max31865_cmd::ADC_CODE_MAX, high))
        {
            reject("raw thresholds must be 0..32767");
            return;
        }
        const MAX31865FaultThresholds thresholds{
            static_cast<uint16_t>(low), static_cast<uint16_t>(high)};
        status = gContext.device->setFaultThresholdsRaw(
            thresholds, operationTimeout());
    }
    else
    {
        float low = 0.0F;
        float high = 0.0F;
        if (!parseFloat(argv[3], low) || !parseFloat(argv[4], high))
        {
            reject("threshold values must be finite numbers");
            return;
        }
        if (strcmp(argv[2], "ohms") == 0)
        {
            status = gContext.device->setFaultThresholdsResistance(
                low, high, operationTimeout());
        }
        else if (strcmp(argv[2], "c") == 0)
        {
            status = gContext.device->setFaultThresholdsTemperature(
                low, high, operationTimeout());
        }
        else
        {
            reject("threshold units must be raw, ohms, or c");
            return;
        }
    }
    (void)reportStatus("thresholds_set", status);
}

void handleRegister(size_t argc, char *const argv[])
{
    if ((argc == 3U || argc == 4U) && strcmp(argv[1], "read") == 0)
    {
        uint32_t address = 0U;
        if (!parseUnsigned(argv[2], 0U, max31865_cmd::REG_LAST, address))
        {
            reject("register address must be 0..7");
            return;
        }
        uint32_t length = 1U;
        const bool explicitSpan = argc == 4U;
        if (explicitSpan &&
            (!parseUnsigned(
                 argv[3],
                 1U,
                 static_cast<uint32_t>(max31865_cmd::NUM_REGISTERS),
                 length) ||
             length >
                 static_cast<uint32_t>(max31865_cmd::NUM_REGISTERS) - address))
        {
            reject("register span must contain 1..8 bytes within addresses 0..7");
            return;
        }

        uint8_t values[max31865_cmd::NUM_REGISTERS]{};
        const MAX31865Status status = explicitSpan
            ? gContext.device->readRegisters(
                  static_cast<uint8_t>(address),
                  values,
                  static_cast<size_t>(length),
                  operationTimeout())
            : gContext.device->readRegister(
                  static_cast<uint8_t>(address),
                  values[0],
                  operationTimeout());
        if (status.ok())
        {
            for (uint32_t index = 0U; index < length; ++index)
            {
                const uint8_t registerAddress = static_cast<uint8_t>(
                    address + index);
                emitf(
                    "%s address=0x%02X name=%s value=0x%02X\n",
                    gFormat == OutputFormat::Machine
                        ? "REC register"
                        : "register",
                    static_cast<unsigned>(registerAddress),
                    max31865RegisterName(registerAddress),
                    static_cast<unsigned>(values[index]));
            }
            const uint32_t lastAddress = address + length - 1U;
            if (address <= max31865_cmd::REG_RTD_LSB &&
                lastAddress >= max31865_cmd::REG_RTD_MSB)
            {
                emit(
                    gFormat == OutputFormat::Machine
                        ? "REC warning kind=register_read_acknowledges_drdy\n"
                        : "warning: register read included RTD data and "
                          "acknowledged DRDY\n");
            }
        }
        (void)reportStatus("reg_read", status);
        return;
    }
    if (argc == 2U && strcmp(argv[1], "dump") == 0)
    {
        MAX31865RegisterDump dump[max31865_cmd::NUM_REGISTERS]{};
        size_t count = 0U;
        const MAX31865Status status = gContext.device->dumpRegisters(
            dump, max31865_cmd::NUM_REGISTERS, count, operationTimeout());
        if (status.ok())
        {
            for (size_t index = 0U; index < count; ++index)
            {
                emitf(
                    "%s address=0x%02X name=%s value=0x%02X\n",
                    gFormat == OutputFormat::Machine
                        ? "REC register"
                        : "register",
                    static_cast<unsigned>(dump[index].address),
                    dump[index].name,
                    static_cast<unsigned>(dump[index].value));
            }
            emit(
                gFormat == OutputFormat::Machine
                    ? "REC warning kind=register_dump_acknowledges_drdy\n"
                    : "warning: dump read RTD registers and acknowledged DRDY\n");
        }
        (void)reportStatus("reg_dump", status);
        return;
    }
    if (argc == 5U &&
        (strcmp(argv[1], "write") == 0 ||
         strcmp(argv[1], "verify") == 0) &&
        finalConfirm(argc, argv))
    {
        uint32_t address = 0U;
        uint32_t value = 0U;
        if (!parseUnsigned(argv[2], 0U, max31865_cmd::REG_LAST, address) ||
            !parseUnsigned(argv[3], 0U, UINT8_MAX, value))
        {
            reject("register address/value must fit 0..7 and 0..255");
            return;
        }
        MAX31865Status status = MAX31865Status::Ok();
        if (strcmp(argv[1], "verify") == 0)
        {
            uint8_t readBack = 0U;
            status = gContext.device->writeRegisterVerified(
                static_cast<uint8_t>(address),
                static_cast<uint8_t>(value),
                readBack,
                operationTimeout());
            if (status.ok())
            {
                emitf(
                    "%s readback=0x%02X\n",
                    gFormat == OutputFormat::Machine
                        ? "REC register_verify"
                        : "register_verify",
                    readBack);
            }
        }
        else
        {
            status = gContext.device->writeRegister(
                static_cast<uint8_t>(address),
                static_cast<uint8_t>(value),
                operationTimeout());
        }
        (void)reportStatus("reg_write", status);
        return;
    }
    if (argc == 3U && strcmp(argv[1], "test") == 0 &&
        finalConfirm(argc, argv))
    {
        uint8_t observed = 0U;
        const MAX31865Status status = gContext.device->registerReadbackTest(
            observed, operationTimeout());
        if (status.ok())
        {
            emitf(
                "%s observed=0x%02X restored=1\n",
                gFormat == OutputFormat::Machine
                    ? "REC register_test"
                    : "register_test",
                observed);
        }
        (void)reportStatus("reg_test", status);
        return;
    }
    if (argc == 3U && strcmp(argv[1], "defaults") == 0 &&
        finalConfirm(argc, argv))
    {
        (void)reportStatus(
            "restore_writable_defaults",
            gContext.device->restoreWritableDefaults(operationTimeout()));
        return;
    }
    reject("invalid reg command; use help reg");
}

void handleConvert(size_t argc, char *const argv[])
{
    if (argc != 3U)
    {
        reject("usage: convert code n | resistance ohms | temperature c");
        return;
    }
    if (strcmp(argv[1], "code") == 0)
    {
        uint32_t input = 0U;
        if (!parseUnsigned(argv[2], 0U, max31865_cmd::ADC_CODE_MAX, input))
        {
            reject("code must be 0..32767");
            return;
        }
        float ratio = 0.0F;
        float resistance = 0.0F;
        float temperature = 0.0F;
        MAX31865Status status = MAX31865::codeToRatio(
            static_cast<uint16_t>(input), ratio);
        if (status.ok())
        {
            status = gContext.device->codeToResistance(
                static_cast<uint16_t>(input), resistance);
        }
        const MAX31865Status temperatureStatus = status.ok()
            ? gContext.device->resistanceToTemperature(
                  resistance, temperature)
            : status;
        if (status.ok())
        {
            emitf(
                "%s input=code code=%lu ratio=%.9g resistance_ohms=%.9g temperature_valid=%u temperature_c=%.9g\n",
                gFormat == OutputFormat::Machine ? "REC conversion" : "conversion",
                static_cast<unsigned long>(input),
                static_cast<double>(ratio),
                static_cast<double>(resistance),
                temperatureStatus.ok() ? 1U : 0U,
                static_cast<double>(temperature));
        }
        (void)reportStatus("convert_code", status);
        return;
    }

    float input = 0.0F;
    if (!parseFloat(argv[2], input))
    {
        reject("conversion input must be finite");
        return;
    }
    float resistance = 0.0F;
    float temperature = 0.0F;
    uint16_t code = 0U;
    MAX31865Status status = MAX31865Status::Ok();
    if (strcmp(argv[1], "resistance") == 0)
    {
        resistance = input;
        status = gContext.device->resistanceToCode(resistance, code);
        if (status.ok())
        {
            status = gContext.device->resistanceToTemperature(
                resistance, temperature);
        }
    }
    else if (strcmp(argv[1], "temperature") == 0)
    {
        temperature = input;
        status = gContext.device->temperatureToResistance(
            temperature, resistance);
        if (status.ok())
        {
            status = gContext.device->temperatureToCode(temperature, code);
        }
    }
    else
    {
        reject("conversion kind must be code, resistance, or temperature");
        return;
    }
    if (status.ok())
    {
        emitf(
            "%s input=%s code=%u resistance_ohms=%.9g temperature_c=%.9g\n",
            gFormat == OutputFormat::Machine ? "REC conversion" : "conversion",
            argv[1],
            static_cast<unsigned>(code),
            static_cast<double>(resistance),
            static_cast<double>(temperature));
    }
    (void)reportStatus("convert", status);
}

void dispatch(size_t argc, char *const argv[])
{
    const char *command = argv[0];
    if (strcmp(command, "help") == 0)
    {
        if (argc > 2U)
        {
            reject("usage: help [command]");
            return;
        }
        const CommandSpec *requested = argc == 2U
            ? findCommand(argv[1])
            : nullptr;
        if (argc == 2U && requested == nullptr)
        {
            reject("unknown help command");
            return;
        }
        printHelp(requested);
        return;
    }
    if (strcmp(command, "format") == 0)
    {
        if (argc != 2U ||
            (strcmp(argv[1], "human") != 0 &&
             strcmp(argv[1], "machine") != 0))
        {
            reject("usage: format human|machine");
            return;
        }
        gFormat = strcmp(argv[1], "machine") == 0
            ? OutputFormat::Machine
            : OutputFormat::Human;
        (void)reportStatus("format", MAX31865Status::Ok());
        return;
    }
    if (strcmp(command, "version") == 0 && argc == 1U)
    {
        printVersion();
        return;
    }
    if (strcmp(command, "wiring") == 0 && argc == 1U)
    {
        printWiring();
        return;
    }
    if (strcmp(command, "timeout") == 0)
    {
        if (argc == 1U)
        {
            printTimeout();
            return;
        }
        uint32_t timeoutMs = 0U;
        if (argc != 2U ||
            !parseUnsigned(argv[1], 1U, INT32_MAX, timeoutMs))
        {
            reject("usage: timeout [1..INT32_MAX]");
            return;
        }
        gOperationTimeoutMs = timeoutMs;
        printTimeout();
        return;
    }
    if (strcmp(command, "telemetry") == 0 && argc == 1U)
    {
        printTelemetry();
        return;
    }
    if (strcmp(command, "state") == 0 && argc == 1U)
    {
        printState();
        return;
    }
    if (strcmp(command, "health") == 0 && argc == 1U)
    {
        printHealth();
        return;
    }
    if (strcmp(command, "counters") == 0)
    {
        if (argc == 2U && strcmp(argv[1], "show") == 0)
        {
            printTransportCounters();
            printHealth();
            return;
        }
        if (argc == 3U && strcmp(argv[1], "clear") == 0 &&
            finalConfirm(argc, argv))
        {
            if (jobActive())
            {
                reject("counters clear is unavailable while a CLI job is active");
                return;
            }
            clearTransportCounters(*gContext.observer);
            gContext.device->clearLifetimeCounters();
            (void)reportStatus("counters_clear", MAX31865Status::Ok());
            return;
        }
        reject("usage: counters show | counters clear confirm");
        return;
    }
    if (strcmp(command, "begin") == 0 && argc == 2U)
    {
        gContext.beginConfig->defaultOperationTimeoutMs =
            gOperationTimeoutMs;
        const MAX31865Status status = gContext.transportReady
            ? gContext.device->begin(*gContext.beginConfig)
            : MAX31865Status::Error(
                  MAX31865Error::InvalidState,
                  "Arduino transport setup is unavailable");
        (void)reportStatus("begin", status);
        return;
    }
    if (strcmp(command, "end") == 0 && argc == 2U)
    {
        gContext.device->end();
        (void)reportStatus("end", MAX31865Status::Ok());
        return;
    }
    if (strcmp(command, "recover") == 0 && (argc == 2U || argc == 3U))
    {
        uint32_t timeoutMs = operationTimeout();
        if (argc == 3U &&
            !parseUnsigned(argv[1], 1U, INT32_MAX, timeoutMs))
        {
            reject("timeout must be 1..INT32_MAX milliseconds");
            return;
        }
        (void)reportStatus("recover", gContext.device->recover(timeoutMs));
        return;
    }
    if (strcmp(command, "probe") == 0 && argc == 1U)
    {
        MAX31865DeviceInfo info{};
        const MAX31865Status status = gContext.device->probe(
            info, operationTimeout());
        if (status.ok() || status.code == MAX31865Error::ProbeMismatch)
        {
            printProbe(info);
        }
        (void)reportStatus("probe", status);
        return;
    }
    if (strcmp(command, "settings") == 0 && argc == 1U)
    {
        MAX31865Settings settings{};
        const MAX31865Status status = gContext.device->readConfiguration(
            settings, operationTimeout());
        if (status.ok())
        {
            printSettings(settings);
        }
        (void)reportStatus("settings", status);
        return;
    }
    if (strcmp(command, "offline") == 0 && argc == 3U)
    {
        uint32_t threshold = 0U;
        if (!parseUnsigned(argv[1], 1U, UINT8_MAX, threshold))
        {
            reject("offline threshold must be 1..255");
            return;
        }
        (void)reportStatus(
            "offline",
            gContext.device->setOfflineThreshold(
                static_cast<uint8_t>(threshold)));
        return;
    }
    if (strcmp(command, "bias") == 0 && argc == 3U)
    {
        if (strcmp(argv[1], "on") != 0 && strcmp(argv[1], "off") != 0)
        {
            reject("usage: bias on|off confirm");
            return;
        }
        (void)reportStatus(
            "bias",
            gContext.device->setBias(
                strcmp(argv[1], "on") == 0, operationTimeout()));
        return;
    }
    if (strcmp(command, "wire") == 0 && argc == 3U)
    {
        MAX31865WireMode mode = MAX31865WireMode::FourWire;
        if (strcmp(argv[1], "2") == 0)
        {
            mode = MAX31865WireMode::TwoWire;
        }
        else if (strcmp(argv[1], "3") == 0)
        {
            mode = MAX31865WireMode::ThreeWire;
        }
        else if (strcmp(argv[1], "4") != 0)
        {
            reject("wire mode must be 2, 3, or 4");
            return;
        }
        (void)reportStatus(
            "wire", gContext.device->setWireMode(mode, operationTimeout()));
        return;
    }
    if (strcmp(command, "filter") == 0 && argc == 3U)
    {
        if (strcmp(argv[1], "50") != 0 && strcmp(argv[1], "60") != 0)
        {
            reject("filter must be 50 or 60 Hz");
            return;
        }
        const MAX31865Filter filter = strcmp(argv[1], "50") == 0
            ? MAX31865Filter::Hz50
            : MAX31865Filter::Hz60;
        (void)reportStatus(
            "filter", gContext.device->setFilter(filter, operationTimeout()));
        return;
    }
    if (strcmp(command, "rtd") == 0)
    {
        if (argc == 2U && strcmp(argv[1], "get") == 0)
        {
            printRtd(gContext.device->rtdConfig());
            return;
        }
        if (argc != 8U || strcmp(argv[1], "set") != 0 ||
            !finalConfirm(argc, argv))
        {
            reject("usage: rtd get | rtd set rref r0 tau_us min_c max_c confirm");
            return;
        }
        MAX31865RtdConfig config = gContext.device->rtdConfig();
        uint32_t tauUs = 0U;
        if (!parseFloat(argv[2], config.referenceResistorOhms) ||
            !parseFloat(argv[3], config.nominalResistanceOhms) ||
            !parseUnsigned(argv[4], 0U, UINT32_MAX, tauUs) ||
            !parseFloat(argv[5], config.minimumTemperatureC) ||
            !parseFloat(argv[6], config.maximumTemperatureC))
        {
            reject("invalid finite RTD configuration value");
            return;
        }
        config.inputFilterTimeConstantUs = tauUs;
        (void)reportStatus("rtd_set", gContext.device->setRtdConfig(config));
        return;
    }
    if (strcmp(command, "cvd") == 0)
    {
        if (argc != 6U || strcmp(argv[1], "set") != 0 ||
            !finalConfirm(argc, argv))
        {
            reject("usage: cvd set a b c confirm");
            return;
        }
        MAX31865RtdConfig config = gContext.device->rtdConfig();
        if (!parseFloat(argv[2], config.coefficients.a) ||
            !parseFloat(argv[3], config.coefficients.b) ||
            !parseFloat(argv[4], config.coefficients.c))
        {
            reject("CVD coefficients must be finite");
            return;
        }
        (void)reportStatus("cvd_set", gContext.device->setRtdConfig(config));
        return;
    }
    if (strcmp(command, "start") == 0 && argc == 2U)
    {
        (void)reportStatus(
            "start", gContext.device->startContinuous(operationTimeout()));
        return;
    }
    if (strcmp(command, "trigger") == 0 && argc == 2U)
    {
        (void)reportStatus(
            "trigger",
            gContext.device->triggerSingleConversion(operationTimeout()));
        return;
    }
    if (strcmp(command, "stop") == 0 && argc == 2U)
    {
        if (jobActive())
        {
            (void)reportStatus(
                "stop", finishJob("stopped"));
        }
        else
        {
            (void)reportStatus(
                "stop", gContext.device->stop(operationTimeout()));
        }
        return;
    }
    if (strcmp(command, "ready") == 0 && argc == 1U)
    {
        bool ready = false;
        const MAX31865Status status = gContext.device->dataReady(
            ready, operationTimeout());
        if (status.ok())
        {
            emitf(
                "%s ready=%u source=%s\n",
                gFormat == OutputFormat::Machine ? "REC ready" : "ready",
                ready ? 1U : 0U,
                gContext.beginConfig->transport.capabilities.hasDrdy
                    ? "drdy"
                    : "elapsed_maximum");
        }
        (void)reportStatus("ready", status);
        return;
    }
    if ((strcmp(command, "poll") == 0 || strcmp(command, "read") == 0) &&
        argc == 1U)
    {
        MAX31865Sample sample{};
        const MAX31865Status status = strcmp(command, "poll") == 0
            ? gContext.device->poll(sample, operationTimeout())
            : gContext.device->readSample(sample, operationTimeout());
        if ((status.ok() || status.code == MAX31865Error::DeviceFault) &&
            (sample.flags & MAX31865_SAMPLE_FLAG_FRAME_VALID) != 0U)
        {
            printSample(sample, command);
        }
        (void)reportStatus(command, status);
        return;
    }
    if (strcmp(command, "wait") == 0 && (argc == 1U || argc == 2U))
    {
        uint32_t timeoutMs = 0U;
        if (!parseOptionalTimeout(argc, argv, 1U, timeoutMs))
        {
            reject("timeout must be 1..INT32_MAX milliseconds");
            return;
        }
        MAX31865Sample sample{};
        const MAX31865Status status =
            gContext.device->readSingle(sample, timeoutMs);
        if ((status.ok() || status.code == MAX31865Error::DeviceFault) &&
            (sample.flags & MAX31865_SAMPLE_FLAG_FRAME_VALID) != 0U)
        {
            printSample(sample, command);
        }
        (void)reportStatus(command, status);
        return;
    }
    if (strcmp(command, "one") == 0 && (argc == 2U || argc == 3U))
    {
        uint32_t timeoutMs = operationTimeout();
        if (argc == 3U &&
            !parseUnsigned(argv[1], 1U, INT32_MAX, timeoutMs))
        {
            reject("timeout must be 1..INT32_MAX milliseconds");
            return;
        }
        MAX31865Sample sample{};
        const MAX31865Status status =
            gContext.device->readOneShot(sample, timeoutMs);
        if ((status.ok() || status.code == MAX31865Error::DeviceFault) &&
            (sample.flags & MAX31865_SAMPLE_FLAG_FRAME_VALID) != 0U)
        {
            printSample(sample, command);
        }
        (void)reportStatus(command, status);
        return;
    }
    if (strcmp(command, "stream") == 0 && argc == 4U)
    {
        uint32_t count = 0U;
        uint32_t intervalMs = 0U;
        if (!parseUnsigned(argv[1], 1U, 100000U, count) ||
            !parseUnsigned(argv[2], 1U, 60000U, intervalMs))
        {
            reject("stream count must be 1..100000 and interval 1..60000 ms");
            return;
        }
        const MAX31865Status status =
            gContext.device->startContinuous(operationTimeout());
        if (!reportStatus("stream_start", status))
        {
            return;
        }
        startJob(JobKind::Stream, count, intervalMs);
        gJob.ownsContinuous = true;
        return;
    }
    if (strcmp(command, "stress") == 0 && argc == 3U)
    {
        uint32_t count = 0U;
        if (!parseUnsigned(argv[1], 1U, 100000U, count))
        {
            reject("stress count must be 1..100000");
            return;
        }
        const MAX31865Status readyStatus =
            gContext.device->state() == MAX31865State::Ready
            ? MAX31865Status::Ok()
            : MAX31865Status::Error(
                  MAX31865Error::InvalidState,
                  "stress requires the Ready lifecycle state");
        if (!reportStatus("stress_start", readyStatus))
        {
            return;
        }
        startJob(JobKind::Stress, count, 1U);
        return;
    }
    if (strcmp(command, "job") == 0 && argc == 1U)
    {
        printJob();
        return;
    }
    if (strcmp(command, "cancel") == 0 && argc == 2U)
    {
        if (!jobActive())
        {
            reject("no CLI job is active");
            return;
        }
        (void)reportStatus(
            "cancel", finishJob("cancelled"));
        return;
    }
    if (strcmp(command, "faults") == 0 && argc == 1U)
    {
        MAX31865FaultStatus fault{};
        const MAX31865Status status = gContext.device->readFaultStatus(
            fault, operationTimeout());
        if (status.ok() || status.code == MAX31865Error::DeviceFault)
        {
            printFault(fault);
        }
        (void)reportStatus("faults", status);
        return;
    }
    if (strcmp(command, "fault-clear") == 0 && argc == 2U)
    {
        (void)reportStatus(
            "fault_clear", gContext.device->clearFaults(operationTimeout()));
        return;
    }
    if ((strcmp(command, "fault-auto") == 0 ||
         strcmp(command, "fault-manual") == 0) &&
        argc == 2U)
    {
        MAX31865FaultStatus fault{};
        const MAX31865Status status = strcmp(command, "fault-auto") == 0
            ? gContext.device->runAutomaticFaultDetection(
                  fault, operationTimeout())
            : gContext.device->runManualFaultDetection(
                  fault, operationTimeout());
        if (status.ok() || status.code == MAX31865Error::DeviceFault)
        {
            printFault(fault);
        }
        (void)reportStatus(command, status);
        return;
    }
    if (strcmp(command, "thresholds") == 0)
    {
        handleThresholds(argc, argv);
        return;
    }
    if (strcmp(command, "reg") == 0)
    {
        handleRegister(argc, argv);
        return;
    }
    if (strcmp(command, "convert") == 0)
    {
        handleConvert(argc, argv);
        return;
    }
    reject("invalid arguments; use help <command>");
}

uint32_t beginCommandFrame(const char *command, uint32_t &startedMs)
{
    saturatingIncrement(gCommandId);
    const uint32_t commandId = gCommandId;
    startedMs = nowMs();
    gDispatchActive = true;
    gDispatchOk = true;
    gDispatchResultEmitted = false;
    gDispatchStatus = MAX31865Status::Ok();
    emitf(
        "COMMAND_BEGIN id=%lu command=%s\n",
        static_cast<unsigned long>(commandId),
        command);
    return commandId;
}

void endCommandFrame(
    uint32_t commandId,
    uint32_t startedMs,
    const char *command)
{
    if (!gDispatchResultEmitted)
    {
        (void)reportStatusForCommand(command, gDispatchStatus, commandId);
    }
    const MAX31865Status terminal = gDispatchStatus;
    const bool terminalOk = gDispatchOk;
    gDispatchActive = false;
    emitf(
        "COMMAND_END id=%lu ok=%u code=%u name=%s detail=%ld elapsed_ms=%lu\n",
        static_cast<unsigned long>(commandId),
        terminalOk ? 1U : 0U,
        static_cast<unsigned>(terminal.code),
        max31865ErrorName(terminal.code),
        static_cast<long>(terminal.detail),
        static_cast<unsigned long>(nowMs() - startedMs));
}
} // namespace

void start()
{
    if (!gBound)
    {
        return;
    }
    if (gContext.transportReady &&
        gContext.device->state() == MAX31865State::Uninitialized)
    {
        (void)reportStatus(
            "auto_begin", gContext.device->begin(*gContext.beginConfig));
    }
    else if (!gContext.transportReady)
    {
        (void)reportStatus(
            "auto_begin",
            MAX31865Status::Error(
                MAX31865Error::InvalidState,
                "transport setup failed; device I/O unavailable"));
    }
    printHelp(nullptr);
    printPrompt();
}

void processInputOverflow()
{
    if (!gBound)
    {
        return;
    }
    uint32_t startedMs = 0U;
    const uint32_t commandId = beginCommandFrame("<invalid>", startedMs);
    reject("input line exceeds 191 bytes and was discarded");
    endCommandFrame(commandId, startedMs, "<invalid>");
}

void processLine(const char *line)
{
    if (!gBound || line == nullptr)
    {
        return;
    }

    size_t length = 0U;
    bool invalidCharacter = false;
    while (length < 192U && line[length] != '\0')
    {
        const unsigned char value =
            static_cast<unsigned char>(line[length]);
        if ((value < 0x20U && value != static_cast<unsigned char>('\t')) ||
            value > 0x7EU)
        {
            invalidCharacter = true;
        }
        ++length;
    }
    if (length == 0U)
    {
        return;
    }
    const bool overlong = length == 192U;

    char storage[192]{};
    char *argv[12]{};
    size_t argc = 0U;
    bool tooManyTokens = false;
    if (!overlong && !invalidCharacter)
    {
        memcpy(storage, line, length);
        storage[length] = '\0';
        char *cursor = storage;
        while (*cursor != '\0')
        {
            while (*cursor == ' ' || *cursor == '\t')
            {
                ++cursor;
            }
            if (*cursor == '\0')
            {
                break;
            }
            if (argc < sizeof(argv) / sizeof(argv[0]))
            {
                argv[argc++] = cursor;
            }
            else
            {
                tooManyTokens = true;
            }
            while (*cursor != '\0' && *cursor != ' ' && *cursor != '\t')
            {
                ++cursor;
            }
            if (*cursor != '\0')
            {
                *cursor++ = '\0';
            }
        }
    }
    if (!overlong && !invalidCharacter && argc == 0U)
    {
        return;
    }

    const char *command = overlong || invalidCharacter ? "<invalid>" : argv[0];
    const CommandSpec *spec = overlong || invalidCharacter || tooManyTokens
        ? nullptr
        : findCommand(command);
    uint32_t commandStartedMs = 0U;
    const uint32_t commandId =
        beginCommandFrame(command, commandStartedMs);
    if (overlong)
    {
        reject("input line exceeds 191 bytes");
    }
    else if (invalidCharacter)
    {
        reject("input contains a non-printable character");
    }
    else if (tooManyTokens)
    {
        reject("too many command tokens");
    }
    else if (spec == nullptr)
    {
        reject("unknown command");
    }
    else if (jobActive() && !spec->allowedDuringJob)
    {
        reject("a CLI job is active; use job or cancel confirm");
    }
    else if (spec->safety == CommandSafety::ConfirmMutation &&
             !finalConfirm(argc, argv))
    {
        reject("mutation rejected; append the exact token confirm");
    }
    else
    {
        dispatch(argc, argv);
    }
    endCommandFrame(commandId, commandStartedMs, command);
}

void service()
{
    if (!gBound)
    {
        return;
    }
    const uint32_t currentMs = nowMs();
    gContext.device->tick(currentMs);
    if (!jobActive() || !due(currentMs, gJob.nextDueMs))
    {
        return;
    }

    MAX31865Sample sample{};
    MAX31865Status status = MAX31865Status::Ok();
    if (gJob.kind == JobKind::Stream)
    {
        status = gContext.device->poll(
            sample, gJob.operationTimeoutMs);
        recordJobResult(status, sample, "stream");
    }
    else if (gJob.kind == JobKind::Stress)
    {
        status = gContext.device->readOneShot(
            sample, gJob.operationTimeoutMs);
        recordJobResult(status, sample, "stress");
    }

    const bool completedTarget = gJob.kind == JobKind::Stream
        ? gJob.successes >= gJob.target
        : gJob.attempts >= gJob.target;
    if (completedTarget)
    {
        finishJob(gJob.failures == 0U ? "complete" : "completed_with_failures");
        return;
    }
    if (gJob.kind == JobKind::Stream &&
        gJob.attempts >= gJob.attemptLimit)
    {
        finishJob("attempt_limit");
        return;
    }
    gJob.nextDueMs = currentMs + gJob.intervalMs;
}

void printPrompt()
{
    if (gBound && gFormat == OutputFormat::Human)
    {
        emit("max31865> ");
    }
}
} // namespace max31865_cli
