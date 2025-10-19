#pragma once

#if defined(EXEC_NATIVE)

// EXEC_NATIVE

#define EXEC_RAM

#elif defined(ARDUINO_AVR_NANO)

#define EXEC_ARDUINO
#define EXEC_ARDUINO_NANO

#define EXEC_RAM

#elif defined(ARDUINO_ARCH_ESP8266)

#define EXEC_ARDUINO
#define EXEC_ARDUINO_ESP8266

#define EXEC_RAM IRAM_ATTR

#endif
