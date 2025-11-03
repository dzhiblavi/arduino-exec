#include "exec/sys/config.h"  // IWYU pragma: keep

#if !defined(EXEC_INT_MANUAL)

#if defined(EXEC_NATIVE)

#define EXEC_INT_MANUAL

#elif defined(EXEC_ARDUINO_AVR)

#define EXEC_INT_ARDUINO_AVR

#elif defined(EXEC_ARDUINO_ESP8266)

#define EXEC_INT_ARDUINO_ESP8266

#endif

#endif
