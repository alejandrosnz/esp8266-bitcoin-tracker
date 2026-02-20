/**
 * @file debug.h
 * @brief Conditional debug-logging macro.
 *
 * To enable verbose Serial output, either uncomment the #define below or add
 * -DDEBUG to your build flags before flashing.
 *
 * When DEBUG is not defined the macro expands to nothing, so there is zero
 * runtime and flash overhead in release builds.
 */
#pragma once

// #define DEBUG // uncomment to enable debug

#ifdef DEBUG
  #define DEBUG_PRINT(x) Serial.println(x)
#else
  #define DEBUG_PRINT(x)
#endif
