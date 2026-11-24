/*
 * PROJECT-USERTOS : Two independently blinking LEDs set over the serial
 *                   monitor, plus a push-button controlled PWM fan, running
 *                   on the FreeRTOS real-time operating system
 * CMPE 311 - UMBC - Dulcey Comeau
 *
 * Same behaviour as PROJECT-ADDMOTOR. Per ECO 0.4 the bare-metal cyclic
 * executive is replaced by FreeRTOS (HL.19): each activity is a FreeRTOS
 * task, created with xTaskCreateStatic() and scheduled by the FreeRTOS
 * preemptive priority scheduler (HL.20).
 *
 *   FreeRTOS     : FreeRTOS 202604.01 LTS, kernel V11.3.1 (E.6), in
 *                  src/FreeRTOS. ATmega328P port in src/FreeRTOS/port.c.
 *   RTOS tick    : 1.000 ms from the PROJECT-MYMILLI time base (Timer1 Fast
 *                  PWM counter) with the developer-written tick ISR.
 *   Tasks        : LED1 and LED2 (priority 3), fan/button (2), serial (1),
 *                  FreeRTOS idle (0).
 *   Registers    : all register constants in myRegs.h; nothing from avr.h.
 *
 * Hardware is unchanged from PROJECT-ADDMOTOR: LEDs on pins 2 and 3, fan
 * PWM on pin 11 (IRFZ34N MOSFET), button on pin 4.
 */

#include "myRegs.h"
#include "src/FreeRTOS/FreeRTOS.h"
#include "src/FreeRTOS/task.h"
#include "myFan.h"

// ---------------------------------------------------------------------------
// Time: one RTOS tick is exactly 1 ms, so the tick count replaces mymilli().
// ---------------------------------------------------------------------------
static inline uint32_t mymilli(void) { return (uint32_t)xTaskGetTickCount(); }

// ---------------------------------------------------------------------------
// Hardware
// ---------------------------------------------------------------------------
const uint8_t LED1_PIN = 2;          // EC.3: LED1 on digital pin 2
const uint8_t LED2_PIN = 3;          // EC.3: LED2 on digital pin 3

// ---------------------------------------------------------------------------
// Task priorities, stacks and control blocks (static allocation, no heap)
// ---------------------------------------------------------------------------
#define PRIO_LED     3               // highest: blink timing (HL.6)
#define PRIO_FAN     2
#define PRIO_SERIAL  1
#define STACK_LED    100             // bytes
#define STACK_FAN    110
#define STACK_SERIAL 220             // Serial.print() needs the most stack

static StaticTask_t tcbLed1, tcbLed2, tcbFan, tcbSerial;
static StackType_t  stkLed1[STACK_LED], stkLed2[STACK_LED];
static StackType_t  stkFan[STACK_FAN], stkSerial[STACK_SERIAL];
static TaskHandle_t hLed1, hLed2;

// ---------------------------------------------------------------------------
// LED tasks (one function, two tasks). Each task sleeps until its next
// toggle time or until the serial task sends it a new half-period with a
// task notification. Toggle times are computed from fixed deadlines, so the
// blink rate does not drift (HL.6). Before an interval is set, the task
// sleeps with its LED off.
// ---------------------------------------------------------------------------
void taskLed(void *pv) {
  const uint8_t pin = (uint8_t)(uintptr_t)pv;
  uint32_t half = 0;                 // half-period in ms; 0 = not set yet
  TickType_t next = 0;               // tick of the next toggle
  bool on = false;
  for (;;) {
    TickType_t wait = portMAX_DELAY;
    if (half != 0) {
      TickType_t now = xTaskGetTickCount();
      wait = ((int32_t)(next - now) > 0) ? (TickType_t)(next - now) : 0;
    }
    uint32_t newHalf;
    if (xTaskNotifyWait(0, 0xFFFFFFFFUL, &newHalf, wait) == pdTRUE) {
      half = newHalf;                // new interval: start on-phase now
      on = true;
      digitalWrite(pin, HIGH);
      next = xTaskGetTickCount() + half;
    } else {
      on = !on;                      // deadline reached: toggle
      digitalWrite(pin, on ? HIGH : LOW);
      next += half;                  // fixed rate: no cumulative drift
      TickType_t now = xTaskGetTickCount();
      if ((int32_t)(now - next) >= 0) next = now + half;   // overrun: resync
    }
  }
}

// Called by the serial task to set one LED's half-period. Only that LED's
// task is notified, so the other LED is unaffected (Table 5 of [1]).
void setLedInterval(uint8_t ledNumber, uint32_t halfPeriod) {
  xTaskNotify(ledNumber == 1 ? hLed1 : hLed2, halfPeriod, eSetValueWithOverwrite);
}

// ---------------------------------------------------------------------------
// Serial task: prompt/read dialog, unchanged from PROJECT-ADDMOTOR except
// that it sleeps for 10 ms when no complete line has arrived.
// ---------------------------------------------------------------------------
enum InputState { ASK_LED, ASK_INTERVAL };
InputState inputState  = ASK_LED;
uint8_t    selectedLed = 0;
char       lineBuf[16];
uint8_t    lineLen  = 0;
bool       prompted = false;

bool readLineNonBlocking(void) {
  while (Serial.available() > 0) {
    char c = Serial.read();
    if (c == '\n' || c == '\r') {
      if (lineLen == 0) continue;             // ignore stray CR/LF
      lineBuf[lineLen] = '\0';
      lineLen = 0;
      return true;
    }
    if (lineLen < sizeof(lineBuf) - 1) lineBuf[lineLen++] = c;
  }
  return false;
}

void taskSerial(void *pv) {
  (void)pv;
  for (;;) {
    if (!prompted) {                          // print each prompt once
      if (inputState == ASK_LED) Serial.print(F("What LED? (1 or 2) "));
      else                       Serial.print(F("What interval (in msec)? "));
      prompted = true;
    }
    if (!readLineNonBlocking()) {             // no full line: sleep 10 ms
      vTaskDelay(pdMS_TO_TICKS(10));
      continue;
    }
    long value = atol(lineBuf);
    Serial.println(value);                    // echo the accepted number
    if (inputState == ASK_LED) {
      if (value == 1 || value == 2) { selectedLed = (uint8_t)value; inputState = ASK_INTERVAL; }
      else                          { Serial.println(F("  Please enter 1 or 2.")); }
    } else {
      if (value > 1) {
        uint32_t half = (uint32_t)value / 2UL;  // 600 -> 300 ms on / 300 ms off
        setLedInterval(selectedLed, half);
        Serial.print(F("  LED")); Serial.print(selectedLed);
        Serial.print(F(" -> ")); Serial.print(half);
        Serial.print(F(" ms on / ")); Serial.print(half);
        Serial.println(F(" ms off."));
      } else {
        Serial.println(F("  Interval must be 2 msec or more."));
      }
      inputState = ASK_LED;
    }
    prompted = false;
  }
}

// ---------------------------------------------------------------------------
// FreeRTOS hook: a task overflowed its stack. Both LEDs on, then halt.
// ---------------------------------------------------------------------------
extern "C" void vApplicationStackOverflowHook(TaskHandle_t t, char *name) {
  (void)t; (void)name;
  asm volatile ("cli");
  digitalWrite(LED1_PIN, HIGH);
  digitalWrite(LED2_PIN, HIGH);
  for (;;) { }
}

// ---------------------------------------------------------------------------
// Arduino entry points
// ---------------------------------------------------------------------------
void setup() {
  pinMode(LED1_PIN, OUTPUT);
  pinMode(LED2_PIN, OUTPUT);
  digitalWrite(LED1_PIN, LOW);
  digitalWrite(LED2_PIN, LOW);
  Serial.begin(9600);
  fanInit();                                   // fan PWM + button ISR

  hLed1 = xTaskCreateStatic(taskLed, "LED1", STACK_LED, (void *)(uintptr_t)LED1_PIN,
                            PRIO_LED, stkLed1, &tcbLed1);
  hLed2 = xTaskCreateStatic(taskLed, "LED2", STACK_LED, (void *)(uintptr_t)LED2_PIN,
                            PRIO_LED, stkLed2, &tcbLed2);
  mfTask = xTaskCreateStatic(taskFan, "FAN", STACK_FAN, NULL, PRIO_FAN, stkFan, &tcbFan);
  xTaskCreateStatic(taskSerial, "SER", STACK_SERIAL, NULL, PRIO_SERIAL, stkSerial, &tcbSerial);

  vTaskStartScheduler();                       // starts the 1 ms tick; never returns
}

void loop() {
  // Not used: FreeRTOS runs the tasks; vTaskStartScheduler() does not return.
}
