/*
 * myRegs.h : ATmega328P register and bit constants defined by The Developer
 * CMPE 311 - UMBC - PROJECT-USERTOS (ECO 0.4)
 *
 * Collects every register constant used by this design (Timer1 RTOS tick,
 * Timer2 fan PWM, push-button pin-change interrupt) in one header that both
 * the C port layer (src/FreeRTOS/port.c) and the C++ sketch include.
 * Values are data-space addresses from the ATmega328P data sheet.
 * Nothing from avr/io.h or avr/interrupt.h is used.
 */
#ifndef MY_REGS_H
#define MY_REGS_H

#include <stdint.h>

#define MM_REG8(addr)  (*(volatile uint8_t *)(uintptr_t)(addr))

// ---- Status register -------------------------------------------------------
#define MM_SREG        MM_REG8(0x5F)   // bit 7 = global interrupt enable
#define MM_SREG_I      0x80

// ---- Timer/Counter0 (Arduino millis) ----------------------------------------
#define MM_TIMSK0      MM_REG8(0x6E)
#define MM_TOIE0_BIT   0x01            // cleared: Arduino Timer0 ISR stopped

// ---- Timer/Counter1 (RTOS tick, 1.000 ms) ----------------------------------
#define MM_TIFR1       MM_REG8(0x36)
#define MM_TIMSK1      MM_REG8(0x6F)
#define MM_TCCR1A      MM_REG8(0x80)
#define MM_TCCR1B      MM_REG8(0x81)
#define MM_TCCR1C      MM_REG8(0x82)
#define MM_TCNT1L      MM_REG8(0x84)
#define MM_TCNT1H      MM_REG8(0x85)
#define MM_ICR1L       MM_REG8(0x86)
#define MM_ICR1H       MM_REG8(0x87)
#define MM_TOIE1_BIT   0x01            // TIMSK1: overflow interrupt enable
#define MM_TOV1_BIT    0x01            // TIFR1: overflow flag (write 1 to clear)
#define MM_WGM11_BIT   0x02            // TCCR1A bit 1
#define MM_WGM12_BIT   0x08            // TCCR1B bit 3
#define MM_WGM13_BIT   0x10            // TCCR1B bit 4
#define MM_CS11_BIT    0x02            // TCCR1B bit 1
#define MM_CS10_BIT    0x01            // TCCR1B bit 0
#define MM_TOP         249U            // 250 counts x 4 us = 1.000 ms
// Fast PWM mode 14 (TOP = ICR1), clock / 64, output pins disconnected.
#define MM_TCCR1A_VALUE  (MM_WGM11_BIT)                                          // 0x02
#define MM_TCCR1B_VALUE  (MM_WGM13_BIT | MM_WGM12_BIT | MM_CS11_BIT | MM_CS10_BIT) // 0x1B
#define MM_TIMER1_OVF_VECTOR_NUM 13    // vector 14 counting RESET as 1

// ---- Ports, pin change, Timer/Counter2 (fan) -------------------------------
#define MF_DDRB        MM_REG8(0x24)
#define MF_PORTB       MM_REG8(0x25)
#define MF_PIND        MM_REG8(0x29)
#define MF_DDRD        MM_REG8(0x2A)
#define MF_PORTD       MM_REG8(0x2B)
#define MF_PCIFR       MM_REG8(0x3B)
#define MF_PCICR       MM_REG8(0x68)
#define MF_PCMSK2      MM_REG8(0x6D)
#define MF_TIMSK2      MM_REG8(0x70)
#define MF_TCCR2A      MM_REG8(0xB0)
#define MF_TCCR2B      MM_REG8(0xB1)
#define MF_TCNT2       MM_REG8(0xB2)
#define MF_OCR2A       MM_REG8(0xB3)
#define MF_PB3_BIT     0x08            // PB3 = pin 11 = OC2A (fan PWM)
#define MF_PD4_BIT     0x10            // PD4 = pin 4 (button)
#define MF_PCIE2_BIT   0x04            // PCICR: enable port D pin changes
#define MF_PCIF2_BIT   0x04            // PCIFR: port D flag (write 1 to clear)
#define MF_PCINT20_BIT 0x10            // PCMSK2: PD4
#define MF_COM2A1_BIT  0x80            // TCCR2A: non-inverting PWM on OC2A
#define MF_WGM20_BIT   0x01            // TCCR2A: phase-correct PWM, TOP 0xFF
#define MF_CS20_BIT    0x01            // TCCR2B: clock / 1
#define MF_TCCR2A_VALUE  (MF_COM2A1_BIT | MF_WGM20_BIT)  // 0x81
#define MF_TCCR2B_VALUE  (MF_CS20_BIT)                   // 0x01 -> 31.4 kHz
#define MF_PCINT2_VECTOR_NUM 5         // vector 6 counting RESET as 1

// ---- Interrupt service routine declarations (no avr/interrupt.h) -----------
// "signal": compiler saves the registers it uses and ends with RETI.
// "naked" : no compiler prologue/epilogue; the code saves everything itself
//           (used by the RTOS tick, which saves the full task context).
#define MM_VECTOR_NAME_(n)  __vector_ ## n
#define MM_VECTOR_NAME(n)   MM_VECTOR_NAME_(n)
#ifdef __cplusplus
#define MM_EXTERN_C extern "C"
#else
#define MM_EXTERN_C
#endif
#define MM_ISR(n) \
  MM_EXTERN_C void MM_VECTOR_NAME(n)(void) __attribute__((signal, used, externally_visible)); \
  void MM_VECTOR_NAME(n)(void)
#define MM_NAKED_ISR(n) \
  MM_EXTERN_C void MM_VECTOR_NAME(n)(void) __attribute__((signal, naked, used, externally_visible)); \
  void MM_VECTOR_NAME(n)(void)

#endif // MY_REGS_H
