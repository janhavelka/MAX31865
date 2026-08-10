#!/usr/bin/env python3
"""Freeze the borrowed transport and Arduino callback guarantees."""

from __future__ import annotations

from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
TRANSPORT = ROOT / "include" / "MAX31865" / "Transport.h"
ARDUINO = ROOT / "src" / "platform" / "arduino" / "MAX31865ArduinoBackend.cpp"
ARDUINO_HEADER = ROOT / "include" / "MAX31865" / "ArduinoBackend.h"

REQUIRED_TRANSPORT_TEXT = (
    "borrows `user` and every resource reachable through it",
    "Callbacks make exactly one attempt",
    "do not retry or recover",
    "do not re-enter the same driver",
    "externally serialized per instance",
    "initializes the complete RX buffer",
    "It neither controls CS nor locks the bus",
    "`lockBus`/`unlockBus` are either both null",
    "one finite shared-bus acquisition attempt",
    "zero means no waiting",
    "zero means one immediate attempt",
    "Any other callback error is normalized to the role-specific",
)

REQUIRED_TRANSPORT_FIELDS = (
    "void *user;",
    "MAX31865TransportCapabilities capabilities;",
    "MAX31865Status (*lockBus)",
    "void (*unlockBus)",
    "MAX31865Status (*setChipSelect)",
    "MAX31865Status (*transfer)",
    "MAX31865Status (*readPin)",
    "uint32_t (*nowMs)",
    "uint32_t (*nowUs)",
    "void (*sleepMs)",
    "void (*delayUs)",
)


def normalized(text: str) -> str:
    return " ".join(text.replace("*", " ").split())


def function_body(source: str, signature: str) -> str:
    start = source.find(signature)
    if start < 0:
        raise ValueError(f"missing function: {signature}")
    brace = source.find("{", start)
    if brace < 0:
        raise ValueError(f"missing function body: {signature}")
    depth = 0
    for index in range(brace, len(source)):
        if source[index] == "{":
            depth += 1
        elif source[index] == "}":
            depth -= 1
            if depth == 0:
                return source[brace + 1:index]
    raise ValueError(f"unterminated function body: {signature}")


def require(failures: list[str], condition: bool, label: str) -> None:
    if not condition:
        failures.append(label)


def ordered(body: str, first: str, second: str) -> bool:
    first_index = body.find(first)
    second_index = body.find(second)
    return first_index >= 0 and second_index > first_index


def main() -> int:
    failures: list[str] = []
    for path in (TRANSPORT, ARDUINO, ARDUINO_HEADER):
        if not path.is_file():
            failures.append(f"required transport boundary file is missing: {path}")
    if failures:
        for failure in failures:
            print(f" - {failure}")
        return 1

    transport = TRANSPORT.read_text(encoding="utf-8")
    arduino = ARDUINO.read_text(encoding="utf-8")
    arduino_header = ARDUINO_HEADER.read_text(encoding="utf-8")
    transport_normalized = normalized(transport)
    for phrase in REQUIRED_TRANSPORT_TEXT:
        require(
            failures,
            normalized(phrase) in transport_normalized,
            f"transport documentation is missing: {phrase}",
        )
    for field in REQUIRED_TRANSPORT_FIELDS:
        require(failures, field in transport, f"transport field is missing: {field}")

    try:
        transfer = function_body(arduino, "MAX31865Status transfer(")
        chip_select = function_body(arduino, "MAX31865Status setChipSelect(")
        gpio_read = function_body(arduino, "MAX31865Status readPin(")
        lock = function_body(arduino, "MAX31865Status lockBus(")
        unlock = function_body(arduino, "void unlockBus(")
        bind = function_body(arduino, "MAX31865Status max31865ArduinoBackendBind(")
        configure = function_body(
            arduino,
            "MAX31865Status max31865ArduinoConfigureControlPins(",
        )
        make_transport = function_body(
            arduino,
            "MAX31865Transport max31865ArduinoTransport(",
        )

        for token, description in (
            ("tx == nullptr", "rejects null TX"),
            ("rx == nullptr", "rejects null RX"),
            ("length == 0U", "rejects zero length"),
            ("length > max31865_cmd::MAX_FRAME_BYTES", "enforces maximum frame"),
            ("validateConfigured", "rejects unconfigured use"),
        ):
            require(failures, token in transfer, f"Arduino transfer {description}")
        require(
            failures,
            transfer.count("transferBytes(") == 1,
            "Arduino transfer performs exactly one framework transfer call",
        )
        require(
            failures,
            ordered(lock, "arbiter.lock", "beginTransaction("),
            "Arduino arbiter lock precedes beginTransaction",
        )
        require(
            failures,
            ordered(unlock, "endTransaction(", "arbiter.unlock"),
            "Arduino endTransaction precedes arbiter unlock",
        )
        require(
            failures,
            "digitalWrite" in chip_select and "setChipSelect" not in chip_select,
            "Arduino CS callback performs one direct GPIO action without reentry",
        )
        require(
            failures,
            ordered(gpio_read, "digitalRead(", "*level ="),
            "Arduino GPIO callback commits output only after reading",
        )
        require(
            failures,
            all(token not in bind for token in ("digitalWrite(", "pinMode(", "beginTransaction(")),
            "Arduino bind is zero-I/O",
        )
        require(
            failures,
            "backend.pinsConfigured = false" in bind,
            "every successful Arduino bind requires explicit pin configuration",
        )
        require(
            failures,
            "spiClockHz == 0U" in bind
            and "spiClockHz > max31865_cmd::SPI_MAX_HZ" in bind
            and "backend.spiClockHz = spiClockHz" in bind,
            "Arduino bind does not validate/store explicit SPI clock metadata",
        )
        require(
            failures,
            ordered(configure, "digitalWrite(", "pinMode("),
            "Arduino control setup preloads CS before enabling the output",
        )
        for callback in (
            "lockBus",
            "unlockBus",
            "setChipSelect",
            "transfer",
            "readPin",
            "nowMs",
            "nowUs",
            "sleepMs",
            "delayUs",
        ):
            require(
                failures,
                f"transport.{callback} = {callback};" in make_transport,
                f"Arduino transport does not publish {callback}",
            )
        for body, label in (
            (transfer, "transfer"),
            (chip_select, "chip select"),
            (gpio_read, "GPIO read"),
            (lock, "lock"),
            (unlock, "unlock"),
        ):
            require(
                failures,
                all(token not in body for token in ("while (", "for (", "retry(", "recover(")),
                f"Arduino {label} callback contains hidden retry/recovery",
            )
    except ValueError as error:
        failures.append(str(error))

    require(failures, "SPI.begin(" not in arduino, "Arduino backend initializes the bus")
    require(
        failures,
        all(
            token not in arduino
            for token in ("MAX31865::", ".probe(", ".recover(", ".readSample(")
        ),
        "Arduino callbacks re-enter the driver",
    )
    require(
        failures,
        "new " not in arduino and "malloc(" not in arduino and "free(" not in arduino,
        "Arduino backend performs heap allocation",
    )
    require(
        failures,
        arduino_header.count("borrowed") >= 3,
        "Arduino public contract does not document borrowed lifetimes",
    )

    if failures:
        print("Transport behavioral contract check FAILED")
        for failure in failures:
            print(f" - {failure}")
        return 1
    print("Transport behavioral contract check PASSED")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
