# CMPE311 Project 3: PROJECT-MYMILLI

Two LEDs on pins 2 and 3 of an Uno R3 blink at independent, User-set
intervals entered over the serial-monitor. Tasks are dispatched by the
PROJECT-CE cyclic executive. Per ECO 0.3, all timing comes from `mymilli()`
in `myMilli.h`: Timer/Counter1 runs as a PWM counter with a 1.000 ms period,
a developer-written ISR counts milliseconds, and all register constants are
defined in the header.

| File | Description |
|---|---|
| `projectMyMilli/projectMyMilli.ino` | Sketch |
| `projectMyMilli/myMilli.h` | Time base: register constants, ISR, mymilli() |
| `projectMyMilli_SDD_RevC.docx` | System Design Document, Revision C |
| `projectMyMilli_ECN-002.docx` | Engineering Change Note ECN-002 |

Open `projectMyMilli/projectMyMilli.ino` in Arduino IDE 2.3+, upload, open the
Serial Monitor at 9600 baud with Newline, and enter `2`, `600`, `1`, `1600`.
For tests T.13 and T.17 set `CE_INSTRUMENT` to 1; set it back to 0 after.

Author: Dulcey Comeau, UMBC
