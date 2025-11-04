#pragma once

#include "exec/sys/config.h"

#if defined(EXEC_NATIVE)

#include "exec/io/serial/platforms/native.h"  // IWYU pragma: keep

#elif defined(EXEC_ARDUINO)

#include "exec/io/serial/platforms/arduino.h"  // IWYU pragma: keep

#endif
