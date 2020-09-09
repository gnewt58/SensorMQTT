#ifndef _SERIAL_DEBUG_H_
#define _SERIAL_DEBUG_H_
# ifdef SERIAL_DEBUG
#  if SERIAL_DEBUG // is true
#   define SDEBUG_PRINT(...) { Serial.print(__VA_ARGS__); }
#   define SDEBUG_PRINTLN(...) { Serial.println(__VA_ARGS__); }
#   define SDEBUG_PRINTF(...) { Serial.printf(__VA_ARGS__); }
#  else
#   define SDEBUG_PRINT(...) {}
#   define SDEBUG_PRINTLN(...) {}
#   define SDEBUG_PRINTF(...) {}
#  endif
# else
#  define SDEBUG_PRINT(...) {}
#  define SDEBUG_PRINTLN(...) {}
#  define SDEBUG_PRINTF(...) {}
# endif
#endif // _SERIAL_DEBUG_H_