/*
 * PROJECT-CE : Independent asynchronous control of two LEDs using a
 *              polled, round-robin cyclic executive task manager
 * CMPE 311 - UMBC
 *
 * Functionally identical to PROJECT-ASYNC (same customer and high-level
 * requirements). Per ECO 0.1 the application is restructured so that every
 * task is dispatched by a cyclic executive from a function pointer table.
 *
 *   - Two LEDs on digital pins 2 and 3 (EC.3) blink independently.
 *   - The User sets each LED's blink interval (full period, msec) over the
 *     Arduino IDE serial-monitor (HL.3, HL.5); on-time = off-time = interval/2.
 *   - Serial I/O is a non-blocking task, so it never disturbs blinking (HL.4).
 *   - No RTOS or 3rd-party multitasking library is used (EC.5).
 *
 * Adding a future task = write a void(void) run-to-completion function and
 * add one row to taskTable[]. The dispatcher itself never changes.
 */

// ---------------------------------------------------------------------------
// Build options
// ---------------------------------------------------------------------------
// Set to 1 only for test T.13 (timing instrumentation). Leave 0 for delivery.
#define CE_INSTRUMENT 0

// ---------------------------------------------------------------------------
// Hardware
// ---------------------------------------------------------------------------
const uint8_t LED1_PIN = 2;          // EC.3: LED1 on digital pin 2
const uint8_t LED2_PIN = 3;          // EC.3: LED2 on digital pin 3

// ---------------------------------------------------------------------------
// Cyclic executive: task control block and function pointer table
// ---------------------------------------------------------------------------
typedef void (*TaskFunction)(void);  // every task: no args, runs to completion

typedef struct {
  TaskFunction  run;                 // pointer to the task function
  unsigned long period;              // msec between dispatches; 0 = every cycle
  unsigned long lastRun;             // millis() timestamp of last dispatch
  bool          enabled;             // false = skipped by the dispatcher
} Task;

// Task IDs double as indices into taskTable[] (round-robin order).
enum TaskId { TASK_LED1 = 0, TASK_LED2, TASK_SERIAL, NUM_TASKS };

// Task function prototypes
void taskLed1(void);
void taskLed2(void);
void taskSerial(void);

// The task table. LED tasks start disabled (LEDs off) until The User sets
// an interval; the serial task is always enabled and polled every cycle.
Task taskTable[NUM_TASKS] = {
  /* run          period  lastRun  enabled */
  { taskLed1,     0,      0,       false },   // TASK_LED1
  { taskLed2,     0,      0,       false },   // TASK_LED2
  { taskSerial,   0,      0,       true  },   // TASK_SERIAL
};

#if CE_INSTRUMENT
unsigned long ceCycles      = 0;     // completed dispatcher cycles
unsigned long ceMaxCycleUs  = 0;     // worst-case cycle time, microseconds
unsigned long ceMaxTaskUs[NUM_TASKS] = {0};
unsigned long ceLastReport  = 0;
#endif

// Polled, round-robin dispatcher. Visits every table entry once per cycle in
// fixed index order and calls the task through its function pointer when the
// task is enabled and its period has elapsed.
void dispatchTasks(void) {
#if CE_INSTRUMENT
  unsigned long cycleStart = micros();
#endif
  for (uint8_t i = 0; i < NUM_TASKS; i++) {
    Task *t = &taskTable[i];
    if (!t->enabled) continue;
    unsigned long now = millis();
    if (now - t->lastRun >= t->period) {   // unsigned math: rollover safe
      if (t->period != 0 && now - t->lastRun < 2UL * t->period)
        t->lastRun += t->period;           // fixed-rate: no cumulative drift
      else
        t->lastRun = now;                  // period 0, or overrun: resync, no burst
#if CE_INSTRUMENT
      unsigned long ts = micros();
#endif
      t->run();                            // dispatch via function pointer
#if CE_INSTRUMENT
      unsigned long dt = micros() - ts;
      if (dt > ceMaxTaskUs[i]) ceMaxTaskUs[i] = dt;
#endif
    }
  }
#if CE_INSTRUMENT
  unsigned long cycleUs = micros() - cycleStart;
  if (cycleUs > ceMaxCycleUs) ceMaxCycleUs = cycleUs;
  ceCycles++;
  if (millis() - ceLastReport >= 5000UL) {
    ceLastReport = millis();
    Serial.print(F("\n[CE] cycles=")); Serial.print(ceCycles);
    Serial.print(F(" maxCycle_us=")); Serial.print(ceMaxCycleUs);
    for (uint8_t i = 0; i < NUM_TASKS; i++) {
      Serial.print(F(" t")); Serial.print(i);
      Serial.print(F("_us=")); Serial.print(ceMaxTaskUs[i]);
    }
    Serial.println();
  }
#endif
}

// ---------------------------------------------------------------------------
// LED tasks
// ---------------------------------------------------------------------------
bool led1On = false;
bool led2On = false;

// Each LED task is dispatched once per half-period and simply toggles its
// LED, so the dispatcher's timing defines the blink rate (HL.6).
void taskLed1(void) {
  led1On = !led1On;
  digitalWrite(LED1_PIN, led1On ? HIGH : LOW);
}

void taskLed2(void) {
  led2On = !led2On;
  digitalWrite(LED2_PIN, led2On ? HIGH : LOW);
}

// Called by the serial task to (re)configure one LED task. Only that LED's
// table entry is touched, so the other LED is unaffected (Table 5).
void setLedInterval(uint8_t ledNumber, unsigned long halfPeriod) {
  uint8_t id  = (ledNumber == 1) ? TASK_LED1 : TASK_LED2;
  uint8_t pin = (ledNumber == 1) ? LED1_PIN  : LED2_PIN;
  if (ledNumber == 1) led1On = true; else led2On = true;
  digitalWrite(pin, HIGH);                    // begin on-phase immediately
  taskTable[id].period  = halfPeriod;
  taskTable[id].lastRun = millis();
  taskTable[id].enabled = true;
}

// ---------------------------------------------------------------------------
// Serial task: non-blocking prompt/read state machine
// ---------------------------------------------------------------------------
enum InputState { ASK_LED, ASK_INTERVAL };
InputState inputState  = ASK_LED;
uint8_t    selectedLed = 0;          // 1 or 2, chosen in ASK_LED
char       lineBuf[16];
uint8_t    lineLen  = 0;
bool       prompted = false;         // has the current prompt been printed?

// Read available serial chars into lineBuf without blocking.
// Returns true once a full line (terminated by \n or \r) is ready.
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
  return false;                               // nothing complete yet
}

void taskSerial(void) {
  if (!prompted) {                            // print each prompt exactly once
    if (inputState == ASK_LED) Serial.print(F("What LED? (1 or 2) "));
    else                       Serial.print(F("What interval (in msec)? "));
    prompted = true;
  }

  if (!readLineNonBlocking()) return;         // no full line yet: yield

  long value = atol(lineBuf);
  Serial.println(value);                      // echo the accepted number

  if (inputState == ASK_LED) {
    if (value == 1 || value == 2) { selectedLed = (uint8_t)value; inputState = ASK_INTERVAL; }
    else                          { Serial.println(F("  Please enter 1 or 2.")); }
  } else {                                    // ASK_INTERVAL
    if (value > 1) {
      unsigned long half = (unsigned long)value / 2UL;   // Table 5: 600 -> 300/300
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
  prompted = false;                           // trigger the next prompt
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
}

void loop() {
  dispatchTasks();        // the cyclic executive: loop() calls nothing else
}
