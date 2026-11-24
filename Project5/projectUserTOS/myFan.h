/*
 * myFan.h : PWM fan motor driver and push-button input as a FreeRTOS task
 * CMPE 311 - UMBC - PROJECT-USERTOS (ECO 0.4), from PROJECT-ADDMOTOR
 *
 *   - Fan PWM : Timer/Counter2, phase-correct PWM, 31.4 kHz, OC2A on pin 11
 *               -> IRFZ34N gate (HL.10). Duty = OCR2A / 255.
 *   - Button  : pin 4 to GND, internal pull-up, pin-change interrupt
 *               PCINT20. The developer-written ISR counts the edge and wakes
 *               the fan task with a FreeRTOS task notification (HL.12).
 *   - Speeds  : OFF-LOW-MED-HIGH-MED-LOW-OFF, one step per press (HL.14, 15).
 *
 * The fan task sleeps (uses no CPU) until the button ISR wakes it. While the
 * button is active it samples the pin every 2 ms into an up/down counter
 * (same noise-tolerant debounce as PROJECT-ADDMOTOR). It also ends the
 * 200 ms start-up kick.
 *
 * Requires myRegs.h and the FreeRTOS headers to be included first.
 * Include in exactly ONE source file (the sketch).
 */
#ifndef MY_FAN_H
#define MY_FAN_H

#define MF_SAMPLE_MS        2U    // button sampled every 2 ms while active
#define MF_DEBOUNCE_SAMPLES 10U   // 10 samples (20 ms) to accept a change
#define MF_KICK_MS          200U  // full power for this long when starting

#define MF_DUTY_OFF   0U          //   0 %
#define MF_DUTY_LOW   128U        //  50 %
#define MF_DUTY_MED   181U        //  71 %
#define MF_DUTY_HIGH  255U        // 100 %

enum FanSpeed { FAN_OFF = 0, FAN_LOW, FAN_MED, FAN_HIGH };
static const uint8_t mfSequence[6] = { FAN_OFF, FAN_LOW, FAN_MED, FAN_HIGH, FAN_MED, FAN_LOW };
static const uint8_t mfDuty[4]     = { MF_DUTY_OFF, MF_DUTY_LOW, MF_DUTY_MED, MF_DUTY_HIGH };

static uint8_t  mfStep        = 0;     // index into mfSequence (0 = OFF)
static uint8_t  mfStablePress = 0;     // debounced level: 1 = pressed
static uint8_t  mfCount       = 0;     // debounce counter 0..MF_DEBOUNCE_SAMPLES
static uint8_t  mfKicking     = 0;
static TickType_t mfKickStart = 0;
static uint32_t mfPresses     = 0;     // accepted presses (for testing)
static volatile uint32_t mfEdges = 0;  // pin-4 edges seen by the ISR
static TaskHandle_t mfTask    = NULL;  // fan task, woken by the ISR

static inline uint8_t mfButtonPressed(void) { return (MF_PIND & MF_PD4_BIT) ? 0 : 1; }
static inline void mfSetDuty(uint8_t duty) { MF_OCR2A = duty; }
uint8_t  fanSpeed(void)   { return mfSequence[mfStep]; }
uint32_t fanPresses(void) { return mfPresses; }

// ---------------------------------------------------------------------------
// ISR: pin change on PD4 (button), every edge including bounce and noise.
// ---------------------------------------------------------------------------
MM_ISR(MF_PCINT2_VECTOR_NUM) {
  mfEdges++;
  if (mfTask != NULL) {
    BaseType_t woken = pdFALSE;
    vTaskNotifyGiveFromISR(mfTask, &woken);   // fan task runs within 1 tick
  }
}

// Configure fan PWM (fan OFF) and the button input + interrupt.
// Call in setup(), before the scheduler starts.
void fanInit(void) {
  uint8_t oldSreg = MM_SREG;
  asm volatile ("cli" ::: "memory");
  MF_PORTB &= (uint8_t)~MF_PB3_BIT;           // fan PWM: pin 11 output, low
  MF_DDRB  |= MF_PB3_BIT;
  MF_TCCR2B = 0;
  MF_TIMSK2 = 0;
  MF_OCR2A  = MF_DUTY_OFF;
  MF_TCNT2  = 0;
  MF_TCCR2A = MF_TCCR2A_VALUE;                // phase-correct PWM on OC2A
  MF_TCCR2B = MF_TCCR2B_VALUE;                // clock / 1 -> 31.4 kHz
  MF_DDRD  &= (uint8_t)~MF_PD4_BIT;           // button: pin 4 input
  MF_PORTD |= MF_PD4_BIT;                     // pull-up on
  MF_PCMSK2 |= MF_PCINT20_BIT;
  MF_PCIFR  = MF_PCIF2_BIT;
  MF_PCICR |= MF_PCIE2_BIT;
  mfStablePress = mfButtonPressed();          // a held button is not a press
  mfCount = mfStablePress ? MF_DEBOUNCE_SAMPLES : 0;
  MM_SREG = oldSreg;
}

// One debounce sample; steps the fan on an accepted press.
static void mfSample(TickType_t now) {
  if (mfButtonPressed()) { if (mfCount < MF_DEBOUNCE_SAMPLES) mfCount++; }
  else                   { if (mfCount > 0) mfCount--; }
  if (mfStablePress) {                        // waiting for release
    if (mfCount == 0) mfStablePress = 0;
    return;
  }
  if (mfCount < MF_DEBOUNCE_SAMPLES) return;
  mfStablePress = 1;
  mfPresses++;                                // accepted press: one step
  uint8_t was = mfSequence[mfStep];
  mfStep = (uint8_t)((mfStep + 1) % 6);
  uint8_t speed = mfSequence[mfStep];
  if (was == FAN_OFF && speed != FAN_OFF) {   // start from rest: full power
    mfKicking = 1; mfKickStart = now;         // briefly to overcome friction
    mfSetDuty(MF_DUTY_HIGH);
  } else {
    mfKicking = 0;
    mfSetDuty(mfDuty[speed]);
  }
}

// Fan task (priority 2).
void taskFan(void *pv) {
  (void)pv;
  for (;;) {
    TickType_t now = xTaskGetTickCount();
    if (mfKicking && now - mfKickStart >= MF_KICK_MS) {   // end of kick
      mfKicking = 0;
      mfSetDuty(mfDuty[mfSequence[mfStep]]);
    }
    // Idle only if nothing is in progress AND the pin reads released now.
    // (An edge wake-up dropped while sampling must not leave a held button
    // unnoticed.) An edge after this check leaves a pending notification,
    // so ulTaskNotifyTake() below returns at once and nothing is missed.
    bool idle = (mfStablePress == 0 && mfCount == 0 && !mfButtonPressed());
    if (idle) {
      // Sleep until the button ISR notifies, or until the kick must end.
      TickType_t wait = portMAX_DELAY;
      if (mfKicking) wait = (TickType_t)(MF_KICK_MS - (now - mfKickStart));
      if (ulTaskNotifyTake(pdTRUE, wait) == 0) continue;   // timeout: kick end
    } else {
      vTaskDelay(pdMS_TO_TICKS(MF_SAMPLE_MS));             // fixed 2 ms sampling
      (void)ulTaskNotifyTake(pdTRUE, 0);                    // drop edge wake-ups
    }
    mfSample(xTaskGetTickCount());
  }
}

#endif // MY_FAN_H
