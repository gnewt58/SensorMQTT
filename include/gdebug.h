#ifndef _G_DEBUG_H_
#define _G_DEBUG_H_
# ifdef G_DEBUG
#  define GDEBUG_PRINT(...) { Serial.print(__VA_ARGS__); }
#  define GDEBUG_PRINTLN(...) { Serial.println(__VA_ARGS__); }
#  define GDEBUG_PRINTF(...) { Serial.printf(__VA_ARGS__); }
# else
#  define GDEBUG_PRINT(...) {}
#  define GDEBUG_PRINTLN(...) {}
#  define GDEBUG_PRINTF(...) {}
# endif
#endif // _G_DEBUG_H_