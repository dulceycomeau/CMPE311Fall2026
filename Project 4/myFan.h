/*
 * myFan.h : PWM fan motor driver and push-button input
 * CMPE 311 - UMBC - PROJECT-ADDMOTOR (ECO 0.3.1)
 *
 * Adds, with code and register definitions written by The Developer:
 *   - Fan PWM   : Timer/Counter2, phase-correct PWM, 31.4 kHz, output OC2A on
 *                 digital pin 11 -> L293D enable (EN1,2) -> DC motor   (HL.10)
 *   - Button    : single-pole momentary push button on digital pin 4 to GND,
 *                 internal pull-up, pin-change interrupt PCINT20 with the
 *                 developer-written ISR MF_BUTTON_ISR (__vector_5)   (HL.12)
 *   - Speeds    : OFF, LOW, MEDIUM, HIGH (HL.14), stepped one per press in
 *                 the order OFF-LOW-MED-HIGH-MED-LOW-OFF ... (HL.15)
 *
 * PWM
 *   Timer2 clock = 16 MHz (no prescale). Phase-correct mode counts 0..255..0,
 *   so one PWM period = 510 clocks: 16 MHz / 510 = 31.37 kHz, above human
 *   hearing, so the motor does not whine at the PWM rate. Duty = OCR2A / 255.
 *   The pin swings 0 V to 5 V (HL.10: maximum 5 VDC).
 *
 * Button
 *   The ISR runs on every edge of pin 4 (presses, contact bounce and any
 *   electrical noise from the motor) and counts the edges. fanTask(),
 *   dispatched by the cyclic executive, samples the pin every MF_SAMPLE_MS
 *   with an up/down counter: a pressed sample counts up, a released sample
 *   counts down. A press is accepted when the counter reaches
 *   MF_DEBOUNCE_SAMPLES, a release when it returns to 0. Short bounces and
 *   noise spikes move the counter by at most a step or two, so they can
 *   neither block a press nor create a false one. One press = one step.
 *
 * Requires myMilli.h to be included first (uses MM_REG8, MM_SREG, mymilli()).
 * Include this file in exactly ONE source file (the sketch).
 * No register name, bit name or ISR macro from avr/io.h or avr/interrupt.h is
 * used.
 */
#ifndef MY_FAN_H
#define MY_FAN_H

#ifndef MY_MILLI_H
#error "Include myMilli.h before myFan.h"
#endif

// ===========================================================================
// Configuration
// ===========================================================================
#define MF_SAMPLE_MS        2U   // button sampled every 2 ms
#define MF_DEBOUNCE_SAMPLES 10U  // 10 samples (20 ms) to accept a change
#define MF_KICK_MS       200U    // full power for this long when starting

// PWM duty for each speed, as OCR2A values out of 255 (see SDD Table 9).
#define MF_DUTY_OFF      0U      //   0 %
#define MF_DUTY_LOW      128U    //  50 %
#define MF_DUTY_MED      181U    //  71 %
#define MF_DUTY_HIGH     255U    // 100 %

// ===========================================================================
// Register constants (ATmega328P data-sheet addresses, data space)
// ===========================================================================
#define MF_PINB        MM_REG8(0x23)   // Port B input
#define MF_DDRB        MM_REG8(0x24)   // Port B direction
#define MF_PORTB       MM_REG8(0x25)   // Port B output
#define MF_PIND        MM_REG8(0x29)   // Port D input
#define MF_DDRD        MM_REG8(0x2A)   // Port D direction
#define MF_PORTD       MM_REG8(0x2B)   // Port D output / pull-up enable
#define MF_PCIFR       MM_REG8(0x3B)   // pin-change interrupt flags
#define MF_PCICR       MM_REG8(0x68)   // pin-change interrupt control
#define MF_PCMSK2      MM_REG8(0x6D)   // pin-change mask, port D
#define MF_TIMSK2      MM_REG8(0x70)   // Timer2 interrupt mask
#define MF_TCCR2A      MM_REG8(0xB0)   // Timer2 control A
#define MF_TCCR2B      MM_REG8(0xB1)   // Timer2 control B
#define MF_TCNT2       MM_REG8(0xB2)   // Timer2 counter
#define MF_OCR2A       MM_REG8(0xB3)   // Timer2 compare A (fan duty)

// Bit masks
#define MF_PB3_BIT     0x08   // PB3 = digital pin 11 = OC2A (fan PWM out)
#define MF_PD4_BIT     0x10   // PD4 = digital pin 4 (button)
#define MF_PCIE2_BIT   0x04   // PCICR bit 2: enable pin-change group 2 (port D)
#define MF_PCIF2_BIT   0x04   // PCIFR bit 2: group 2 flag (write 1 to clear)
#define MF_PCINT20_BIT 0x10   // PCMSK2 bit 4: PCINT20 = PD4
#define MF_COM2A1_BIT  0x80   // TCCR2A bit 7: non-inverting PWM on OC2A
#define MF_WGM20_BIT   0x01   // TCCR2A bit 0: phase-correct PWM, TOP = 0xFF
#define MF_CS20_BIT    0x01   // TCCR2B bit 0: clock / 1

#define MF_TCCR2A_VALUE  (MF_COM2A1_BIT | MF_WGM20_BIT)   // 0x81
#define MF_TCCR2B_VALUE  (MF_CS20_BIT)                    // 0x01

// Interrupt vector number of PCINT2 on the ATmega328P (vector 6 counted from
// 1 = RESET, so __vector_5). MM_ISR() is defined in myMilli.h.
#define MF_PCINT2_VECTOR_NUM 5

// ===========================================================================
// Fan state
// ===========================================================================
enum FanSpeed { FAN_OFF = 0, FAN_LOW, FAN_MED, FAN_HIGH };

// One press advances one step through this 6-step cycle (HL.15).
static const uint8_t mfSequence[6] = { FAN_OFF, FAN_LOW, FAN_MED, FAN_HIGH, FAN_MED, FAN_LOW };
static const uint8_t mfDuty[4]     = { MF_DUTY_OFF, MF_DUTY_LOW, MF_DUTY_MED, MF_DUTY_HIGH };

static uint8_t  mfStep        = 0;       // index into mfSequence (0 = OFF)
static uint8_t  mfStablePress = 0;       // debounced level: 1 = pressed
static uint8_t  mfCount       = 0;       // debounce counter 0..MF_DEBOUNCE_SAMPLES
static uint32_t mfLastSample  = 0;       // mymilli() of the last sample
static uint8_t  mfKicking     = 0;       // 1 while start-up kick is active
static uint32_t mfKickStart   = 0;
static uint32_t mfPresses     = 0;       // accepted presses (for testing)

// Written by the ISR; read with interrupts off.
static volatile uint32_t mfEdges = 0;    // pin-4 edges seen (activity count)

// ===========================================================================
// ISR: pin change on PD4 (button), every edge including bounce (HL.12)
// ===========================================================================
MM_ISR(MF_PCINT2_VECTOR_NUM) {
  mfEdges++;               // interrupts are already off inside an ISR
}

// ===========================================================================
// API
// ===========================================================================
static inline uint8_t mfButtonPressed(void) {
  return (MF_PIND & MF_PD4_BIT) ? 0 : 1;      // active low (pull-up)
}

static inline void mfSetDuty(uint8_t duty) {
  MF_OCR2A = duty;                            // takes effect at next TOP
}

uint8_t fanSpeed(void)   { return mfSequence[mfStep]; }
uint8_t fanDuty(void)    { return MF_OCR2A; }
uint32_t fanPresses(void){ return mfPresses; }
uint32_t fanEdges(void) {
  uint8_t oldSreg = MM_SREG; MM_CLI();
  uint32_t e = mfEdges;
  MM_SREG = oldSreg;
  return e;
}

// Configure the fan PWM output (fan OFF) and the button input + interrupt.
// Call once in setup(), after myMilliInit().
void fanInit(void) {
  uint8_t oldSreg = MM_SREG;
  MM_CLI();

  // Fan PWM: pin 11 output, Timer2 phase-correct PWM at 31.4 kHz, duty 0.
  MF_PORTB &= (uint8_t)~MF_PB3_BIT;
  MF_DDRB  |= MF_PB3_BIT;
  MF_TCCR2B = 0;                              // stop while configuring
  MF_TIMSK2 = 0;                              // Timer2 interrupts not used
  MF_OCR2A  = MF_DUTY_OFF;
  MF_TCNT2  = 0;
  MF_TCCR2A = MF_TCCR2A_VALUE;                // overrides Arduino init()
  MF_TCCR2B = MF_TCCR2B_VALUE;

  // Button: pin 4 input with pull-up, pin-change interrupt on PCINT20.
  MF_DDRD  &= (uint8_t)~MF_PD4_BIT;
  MF_PORTD |= MF_PD4_BIT;
  MF_PCMSK2 |= MF_PCINT20_BIT;
  MF_PCIFR  = MF_PCIF2_BIT;                   // clear any stale flag
  MF_PCICR |= MF_PCIE2_BIT;

  mfStep = 0;
  mfStablePress = mfButtonPressed();          // a held button is not a press
  mfCount = mfStablePress ? MF_DEBOUNCE_SAMPLES : 0;
  mfLastSample = mmMillis;
  MM_SREG = oldSreg;
}

// Fan task, dispatched every cycle by the cyclic executive. Runs to
// completion in a few microseconds and never waits (HL.13).
void fanTask(void) {
  uint32_t now = mymilli();

  // End of the start-up kick: drop from full power to the selected duty.
  if (mfKicking && now - mfKickStart >= MF_KICK_MS) {
    mfKicking = 0;
    mfSetDuty(mfDuty[mfSequence[mfStep]]);
  }

  // Debounce: sample every MF_SAMPLE_MS into an up/down counter.
  if (now - mfLastSample < MF_SAMPLE_MS) return;
  mfLastSample = now;
  if (mfButtonPressed()) { if (mfCount < MF_DEBOUNCE_SAMPLES) mfCount++; }
  else                   { if (mfCount > 0) mfCount--; }
  if (mfStablePress) {                        // waiting for a release
    if (mfCount == 0) mfStablePress = 0;
    return;
  }
  if (mfCount < MF_DEBOUNCE_SAMPLES) return;  // not yet a press
  mfStablePress = 1;

  // Accepted press: one step through OFF-LOW-MED-HIGH-MED-LOW (HL.15).
  mfPresses++;
  uint8_t was = mfSequence[mfStep];
  mfStep = (uint8_t)((mfStep + 1) % 6);
  uint8_t speed = mfSequence[mfStep];
  if (was == FAN_OFF && speed != FAN_OFF) {   // starting from rest:
    mfKicking = 1;                            // full power briefly so the
    mfKickStart = now;                        // motor overcomes static friction
    mfSetDuty(MF_DUTY_HIGH);
  } else {
    mfKicking = 0;
    mfSetDuty(mfDuty[speed]);
  }
}

#endif // MY_FAN_H
