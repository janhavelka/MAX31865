"""Canonical framework-neutral ESP-IDF component build contract."""

from __future__ import annotations


CORE_COMPONENT_DESCRIPTION = (
    "Framework-neutral MAX31865 RTD-to-digital converter core with "
    "application-supplied transport callbacks"
)
CORE_COMPONENT_CMAKE = """idf_component_register(
    SRCS
        "src/MAX31865.cpp"
        "src/MAX31865_Protocol.cpp"
    INCLUDE_DIRS
        "include"
)

target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_17)
"""
