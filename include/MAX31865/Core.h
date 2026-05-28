/**
 * @file Core.h
 * @brief Clean framework-neutral include path for the MAX31865 core driver.
 */

#pragma once

#if defined(MAX31865_HAS_ARDUINO_BACKEND) && MAX31865_HAS_ARDUINO_BACKEND
#error "MAX31865/Core.h is the framework-neutral driver include; use MAX31865/MAX31865.h for the Arduino compatibility backend."
#endif

#ifndef MAX31865_HAS_ARDUINO_BACKEND
#define MAX31865_HAS_ARDUINO_BACKEND 0
#endif

#include "MAX31865/CommandTable.h"
#include "MAX31865/Config.h"
#include "MAX31865/MAX31865.h"
#include "MAX31865/Status.h"
#include "MAX31865/Transport.h"

/**
 * @brief Framework-neutral MAX31865 driver entry point.
 *
 * The current implementation keeps one driver implementation and exposes this
 * alias from the clean include path to avoid duplicate protocol or health
 * state. Include MAX31865/MAX31865.h directly when the guarded compatibility
 * backend is needed.
 */
using MAX31865Core = MAX31865;
