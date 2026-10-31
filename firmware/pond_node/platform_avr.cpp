#include <Arduino.h>

#include "platform.h"

#if defined(__AVR_ATmega328P__)

#include <avr/interrupt.h>
#include <avr/sleep.h>
#include <avr/wdt.h>

ISR(WDT_vect) {}  // only wakes the CPU

void platformInit() {
  MCUSR = 0;
  wdt_disable();
}

float platformReadBandgapCounts() {
  const uint8_t savedAdmux = ADMUX;
  ADMUX = _BV(REFS0) | _BV(MUX3) | _BV(MUX2) | _BV(MUX1);  // AVcc reference, 1.1 V bandgap input
  delay(2);                                                // let the bandgap settle
  ADCSRA |= _BV(ADSC);                                     // discard the first conversion
  while (bit_is_set(ADCSRA, ADSC)) {
  }
  uint32_t sum = 0;
  for (uint8_t i = 0; i < 8; ++i) {
    ADCSRA |= _BV(ADSC);
    while (bit_is_set(ADCSRA, ADSC)) {
    }
    sum += ADC;
  }
  ADMUX = savedAdmux;
  return sum / 8.0f;
}

static void watchdogSleep(uint8_t prescalerBits) {
  cli();
  MCUSR &= ~_BV(WDRF);
  WDTCSR = _BV(WDCE) | _BV(WDE);        // timed change sequence
  WDTCSR = _BV(WDIE) | prescalerBits;  // interrupt mode only, never reset
  wdt_reset();
  set_sleep_mode(SLEEP_MODE_PWR_DOWN);
  sleep_enable();
  sei();
  sleep_cpu();
  sleep_disable();
  wdt_disable();
}

uint32_t platformSleep(uint32_t ms) {
  struct Step {
    uint16_t ms;
    uint8_t bits;
  };
  static const Step steps[] = {
      {8000, _BV(WDP3) | _BV(WDP0)}, {4000, _BV(WDP3)},           {2000, _BV(WDP2) | _BV(WDP1) | _BV(WDP0)},
      {1000, _BV(WDP2) | _BV(WDP1)}, {500, _BV(WDP2) | _BV(WDP0)}, {250, _BV(WDP2)},
      {125, _BV(WDP1) | _BV(WDP0)},  {64, _BV(WDP1)},              {32, _BV(WDP0)},
      {16, 0}};
  const uint8_t savedAdcsra = ADCSRA;
  ADCSRA &= ~_BV(ADEN);  // ADC off while sleeping
  uint32_t slept = 0;
  for (uint8_t i = 0; i < sizeof(steps) / sizeof(steps[0]); ++i) {
    while (ms - slept >= steps[i].ms) {
      watchdogSleep(steps[i].bits);
      slept += steps[i].ms;
    }
  }
  ADCSRA = savedAdcsra;
  return slept;
}

#else  // portable fallbacks (other boards, host simulation)

void platformInit() {}

float platformReadBandgapCounts() { return 0.0f; }

uint32_t platformSleep(uint32_t ms) {
  delay(ms);
  return 0;
}

#endif
