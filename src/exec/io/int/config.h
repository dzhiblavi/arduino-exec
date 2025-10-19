#include "exec/sys/config.h"  // IWYU pragma: keep

#if !defined(EXEC_INT_MANUAL)

#if defined(EXEC_NATIVE)

#define EXEC_INT_MANUAL

#elif defined(EXEC_ARDUINO_NANO)

#define EXEC_INT_ARDUINO_NANO

#elif defined(EXEC_ARDUINO_ESP8266)

#define EXEC_INT_ARDUINO_ESP8266

#endif

#endif
