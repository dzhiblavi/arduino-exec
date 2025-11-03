#pragma once

#if defined(ARDUINO_AVR_NANO)

#define EXEC_ARDUINO
#define EXEC_ARDUINO_AVR

#define EXEC_RAM

#elif defined(ARDUINO_ARCH_ESP8266)

#define EXEC_ARDUINO
#define EXEC_ARDUINO_ESP8266

#define EXEC_RAM IRAM_ATTR

#else

#define EXEC_NATIVE
#define EXEC_RAM

#endif
