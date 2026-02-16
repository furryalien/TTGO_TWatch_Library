#pragma once

#if defined(ESP32) || defined(ESP8266) || defined(ARDUINO_ARCH_ESP32) || defined(ARDUINO_ARCH_ESP8266)
#include <pgmspace.h>
#else
#include_next <avr/pgmspace.h>
#endif
