#pragma once

#if defined(EXEC_SYSTEM_NATIVE)

// EXEC_SYSTEM_NATIVE

#elif defined(ARDUINO_AVR_NANO)

#define EXEC_SYSTEM_ARDUINO_NANO

#elif defined(ARDUINO_ARCH_ESP8266)

#define EXEC_SYSTEM_ARDUINO_ESP8266

#endif
