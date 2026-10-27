/*
 * PROJECT-ASYNC : Independent asynchronous control of two LEDs
 * CMPE 310 - UMBC
 *
 * Two LEDs on digital pins 2 and 3 (EC.3) blink independently. The User
 * sets each LED's blink interval (full period, in msec) over the Arduino
 * IDE serial-monitor (HL.3, HL.5). Serial input is handled non-blocking
 * so it never interrupts the blinking (HL.4). No RTOS or 3rd-party
 * multitasking library is used (EC.5): concurrency is achieved with
 * millis()-based non-blocking scheduling.
 */

const uint8_t LED1_PIN = 2;          // EC.3: LED1 on digital pin 2
const uint8_t LED2_PIN = 3;          // EC.3: LED2 on digital pin 3

// Per-LED blink state. halfPeriod = interval/2 (on-time == off-time).
struct Led {
  uint8_t       pin;
  unsigned long halfPeriod;          // msec; 0 = not yet set (stays off)
  unsigned long lastToggle;          // millis() timestamp of last change
  bool          on;
};

Led led1 = { LED1_PIN, 0, 0, false };
Led led2 = { LED2_PIN, 0, 0, false };

// Non-blocking serial input state machine.
enum InputState { ASK_LED, ASK_INTERVAL };
InputState inputState  = ASK_LED;
int        selectedLed = 0;          // 1 or 2, chosen in ASK_LED
char       lineBuf[16];
uint8_t    lineLen  = 0;
bool       prompted = false;         // has the current prompt been printed?

void setup() {
  pinMode(LED1_PIN, OUTPUT);
  pinMode(LED2_PIN, OUTPUT);
  digitalWrite(LED1_PIN, LOW);
  digitalWrite(LED2_PIN, LOW);
  Serial.begin(9600);
}

// Advance one LED's blink without blocking (HL.6: rate stays constant).
void serviceLed(Led &led) {
  if (led.halfPeriod == 0) return;            // rate not set -> stay off
  unsigned long now = millis();
  if (now - led.lastToggle >= led.halfPeriod) {
    led.lastToggle = now;
    led.on = !led.on;
    digitalWrite(led.pin, led.on ? HIGH : LOW);
  }
}

// Read available serial chars into lineBuf without blocking.
// Returns true once a full line (terminated by \n or \r) is ready.
bool readLineNonBlocking() {
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

void handleInput() {
  if (!prompted) {                            // print each prompt exactly once
    if (inputState == ASK_LED) Serial.print(F("What LED? (1 or 2) "));
    else                       Serial.print(F("What interval (in msec)? "));
    prompted = true;
  }

  if (!readLineNonBlocking()) return;         // no full line yet -> LEDs keep blinking

  long value = atol(lineBuf);
  Serial.println(value);                      // echo the accepted number

  if (inputState == ASK_LED) {
    if (value == 1 || value == 2) { selectedLed = (int)value; inputState = ASK_INTERVAL; }
    else                          { Serial.println(F("  Please enter 1 or 2.")); }
  } else {                                    // ASK_INTERVAL
    if (value > 0) {
      unsigned long half = (unsigned long)value / 2UL;   // Table 5: 600 -> 300/300
      Led &t = (selectedLed == 1) ? led1 : led2;
      t.halfPeriod = half;
      t.lastToggle = millis();
      t.on = true;
      digitalWrite(t.pin, HIGH);              // begin on-phase immediately
      Serial.print(F("  LED")); Serial.print(selectedLed);
      Serial.print(F(" -> ")); Serial.print(half);
      Serial.print(F(" ms on / ")); Serial.print(half);
      Serial.println(F(" ms off."));
    } else {
      Serial.println(F("  Interval must be a positive number of msec."));
    }
    inputState = ASK_LED;
  }
  prompted = false;                           // trigger the next prompt
}

void loop() {
  serviceLed(led1);       // blinking runs every iteration, independent of input
  serviceLed(led2);
  handleInput();          // non-blocking; never stalls the loop (HL.4)
}
