// Board-specific helpers. AVR (Arduino Uno) implementation in platform_avr.cpp;
// other targets get portable fallbacks.
#pragma once

#include <stdint.h>

// Call first in setup(): clears a pending watchdog configuration.
void platformInit();

// Average ADC reading of the internal 1.1 V reference against AVcc (ATmega328P),
// used to measure the supply voltage. Returns 0 when not supported.
float platformReadBandgapCounts();

// Sleeps in power-down mode for about `ms` milliseconds using the watchdog timer
// (+/-10 %). Returns the time slept, which millis() does not count.
// Without sleep support it falls back to delay() and returns 0.
uint32_t platformSleep(uint32_t ms);
