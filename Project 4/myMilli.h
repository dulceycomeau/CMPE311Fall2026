/*
 * myMilli.h : Company-owned replacement for the Arduino millis() time base
 * CMPE 311 - UMBC - PROJECT-MYMILLI (ECO 0.3)
 *
 * Replaces, with code written by The Developer:
 *   - millis()                -> mymilli()          (EC.1.0)
 *   - the Timer0 overflow ISR -> MM_TIMER1_ISR      (EC.1.0.1)
 *   - avr/io.h register names -> MM_* constants     (EC.1.0.2)
 *
 * Extensions beyond millis() (permitted by EC.1.0):
 *   - mymicros()   : 4 usec resolution time stamp from the same counter
 *   - myMilliInit(): optionally stops the Arduino Timer0 interrupt so that
 *                    nothing in The System can depend on Arduino timing
 *
 * Time base
 *   Timer/Counter1 (the 16-bit PWM counter of pins 9 and 10) is run in
 *   Fast PWM mode 14 with TOP = ICR1 = 249 and a clock prescaler of 64:
 *       16 MHz / 64 = 250 kHz  ->  4 usec per count
 *       250 counts per PWM period  ->  exactly 1.000 msec per period
 *   The Timer1 overflow interrupt (TOV1, set each time the counter reaches
 *   TOP) runs MM_TIMER1_ISR once per PWM period, which adds 1 to the
 *   millisecond counter. No fractional correction is needed, unlike the
 *   Arduino Timer0 ISR (1.024 msec per overflow). The output-compare pins
 *   stay disconnected, so pins 9 and 10 remain ordinary digital pins.
 *
 * Usage
 *   Include this file in exactly ONE source file (the sketch), because it
 *   defines the ISR and the functions, not just their prototypes.
 *   Call myMilliInit() once at the start of setup().
 *   Do not call delay(), millis() or micros() from arduino.h after
 *   myMilliInit(): with MM_STOP_ARDUINO_TIMER0 set to 1 they stop advancing.
 *
 * This file includes only <stdint.h>. No register name, bit name or ISR
 * macro from avr/io.h or avr/interrupt.h is used.
 */
#ifndef MY_MILLI_H
#define MY_MILLI_H

#include <stdint.h>

// ===========================================================================
// Configuration
// ===========================================================================
#ifndef MM_STOP_ARDUINO_TIMER0
#define MM_STOP_ARDUINO_TIMER0 1   // 1 = disable the Arduino millis() ISR
#endif

#define MM_CPU_HZ          16000000UL   // ELEGOO Uno R3 system clock
#define MM_PRESCALE        64UL         // Timer1 clock = CPU clock / 64
#define MM_US_PER_COUNT    4UL          // 1e6 / (16e6 / 64)
#define MM_TOP             249U         // counts 0..249 = 250 counts = 1 msec
#define MM_US_PER_MS       1000UL

// ===========================================================================
// Register access (ATmega328P data-sheet addresses, data space)
// ===========================================================================
// Each register is reached through a volatile pointer to its fixed data-space
// address. The host test harness may redefine MM_REG8 to point at an array.
#ifndef MM_REG8
#define MM_REG8(addr)  (*(volatile uint8_t *)(uintptr_t)(addr))
#endif

#define MM_SREG        MM_REG8(0x5F)   // status register (bit 7 = global I)
#define MM_TIFR1       MM_REG8(0x36)   // Timer1 interrupt flag register
#define MM_TIMSK0      MM_REG8(0x6E)   // Timer0 interrupt mask (Arduino millis)
#define MM_TIMSK1      MM_REG8(0x6F)   // Timer1 interrupt mask
#define MM_TCCR1A      MM_REG8(0x80)   // Timer1 control A
#define MM_TCCR1B      MM_REG8(0x81)   // Timer1 control B
#define MM_TCCR1C      MM_REG8(0x82)   // Timer1 control C
#define MM_TCNT1L      MM_REG8(0x84)   // Timer1 counter, low byte
#define MM_TCNT1H      MM_REG8(0x85)   // Timer1 counter, high byte
#define MM_ICR1L       MM_REG8(0x86)   // Timer1 input capture (TOP), low
#define MM_ICR1H       MM_REG8(0x87)   // Timer1 input capture (TOP), high
#define MM_OCR1AL      MM_REG8(0x88)   // Timer1 compare A, low
#define MM_OCR1AH      MM_REG8(0x89)   // Timer1 compare A, high
#define MM_OCR1BL      MM_REG8(0x8A)   // Timer1 compare B, low
#define MM_OCR1BH      MM_REG8(0x8B)   // Timer1 compare B, high

// Bit masks
#define MM_SREG_I      0x80   // SREG bit 7: global interrupt enable
#define MM_TOIE0_BIT   0x01   // TIMSK0 bit 0: Timer0 overflow interrupt enable
#define MM_TOIE1_BIT   0x01   // TIMSK1 bit 0: Timer1 overflow interrupt enable
#define MM_TOV1_BIT    0x01   // TIFR1  bit 0: Timer1 overflow flag
#define MM_WGM11_BIT   0x02   // TCCR1A bit 1
#define MM_WGM12_BIT   0x08   // TCCR1B bit 3
#define MM_WGM13_BIT   0x10   // TCCR1B bit 4
#define MM_CS11_BIT    0x02   // TCCR1B bit 1
#define MM_CS10_BIT    0x01   // TCCR1B bit 0

// Fast PWM mode 14 (WGM13:0 = 1110, TOP = ICR1); COM1A/COM1B = 00 (pins
// disconnected); clock select CS12:0 = 011 (clk/64).
#define MM_TCCR1A_VALUE  (MM_WGM11_BIT)
#define MM_TCCR1B_VALUE  (MM_WGM13_BIT | MM_WGM12_BIT | MM_CS11_BIT | MM_CS10_BIT)

// Interrupt vector number of TIMER1_OVF on the ATmega328P (data sheet
// Table 12-6, vector 14 counted from 1 = RESET, so __vector_13).
#define MM_TIMER1_OVF_VECTOR_NUM 13

// ===========================================================================
// Interrupt control (own definitions, no avr/interrupt.h)
// ===========================================================================
#ifndef MM_CLI
#define MM_CLI()  __asm__ __volatile__ ("cli" ::: "memory")
#endif
#ifndef MM_SEI
#define MM_SEI()  __asm__ __volatile__ ("sei" ::: "memory")
#endif

// ISR declaration. The "signal" attribute makes avr-gcc save and restore
// every register the handler uses, plus SREG, and end with RETI. The name
// __vector_N is what the AVR start-up code places in interrupt vector N.
#define MM_VECTOR_NAME_(n)  __vector_ ## n
#define MM_VECTOR_NAME(n)   MM_VECTOR_NAME_(n)
#ifndef MM_ISR
#define MM_ISR(n) \
  extern "C" void MM_VECTOR_NAME(n)(void) \
      __attribute__((signal, used, externally_visible)); \
  void MM_VECTOR_NAME(n)(void)
#endif

// ===========================================================================
// State
// ===========================================================================
// Written only by the ISR (and zeroed by myMilliInit with interrupts off).
// Volatile: the main program must re-read it every time.
static volatile uint32_t mmMillis = 0;

// ===========================================================================
// ISR: Timer1 overflow, once per 1.000 msec PWM period (EC.1.0.1)
// ===========================================================================
MM_ISR(MM_TIMER1_OVF_VECTOR_NUM) {
  mmMillis++;              // hardware clears TOV1 on entry to this ISR
}

// ===========================================================================
// API
// ===========================================================================

// Configure Timer/Counter1 and start the millisecond counter at 0.
// Must be called once, before any task reads the time.
void myMilliInit(void) {
  uint8_t oldSreg = MM_SREG;
  MM_CLI();                                   // configure atomically

#if MM_STOP_ARDUINO_TIMER0
  MM_TIMSK0 &= (uint8_t)~MM_TOIE0_BIT;        // stop the Arduino millis() ISR
#endif

  MM_TCCR1B = 0;                              // stop Timer1 while configuring
  MM_TCCR1A = MM_TCCR1A_VALUE;                // Arduino init() left it in mode 1
  MM_TCCR1C = 0;
  MM_ICR1H  = (uint8_t)(MM_TOP >> 8);         // 16-bit write: high byte first
  MM_ICR1L  = (uint8_t)(MM_TOP & 0xFF);
  MM_TCNT1H = 0;                              // restart the PWM period
  MM_TCNT1L = 0;
  MM_TIFR1  = MM_TOV1_BIT;                    // writing 1 clears a stale flag
  MM_TIMSK1 = MM_TOIE1_BIT;                   // overflow interrupt only
  mmMillis  = 0;
  MM_TCCR1B = MM_TCCR1B_VALUE;                // start: Fast PWM 14, clk/64

  MM_SREG = oldSreg;                          // restore interrupt state
  MM_SEI();                                   // the time base needs interrupts
}

// Milliseconds since myMilliInit(), modulo 2^32 (wraps after ~49.7 days).
// Replaces millis(). The 32-bit copy is made with interrupts off so the ISR
// cannot change the value half-way through the 4-byte read.
uint32_t mymilli(void) {
  uint8_t oldSreg = MM_SREG;
  MM_CLI();
  uint32_t m = mmMillis;
  MM_SREG = oldSreg;
  return m;
}

// Microseconds since myMilliInit(), 4 usec resolution, modulo 2^32.
// Extension: replaces micros() for the T.13 instrumentation.
uint32_t mymicros(void) {
  uint8_t oldSreg = MM_SREG;
  MM_CLI();
  uint32_t m  = mmMillis;
  uint8_t  lo = MM_TCNT1L;                    // 16-bit read: low byte first
  uint8_t  hi = MM_TCNT1H;
  uint16_t t  = (uint16_t)((hi << 8) | lo);
  // An overflow that happened while interrupts were off has not been counted
  // yet. If the counter is at TOP or has already wrapped past it, count it.
  if ((MM_TIFR1 & MM_TOV1_BIT) && (t == MM_TOP || t < (MM_TOP / 2))) m++;
  MM_SREG = oldSreg;
  // The ISR fires at TOP, so TOP is the first count of a new millisecond.
  uint32_t partUs = (t >= MM_TOP) ? 0UL : ((uint32_t)t + 1UL) * MM_US_PER_COUNT;
  return m * MM_US_PER_MS + partUs;
}

#endif // MY_MILLI_H
