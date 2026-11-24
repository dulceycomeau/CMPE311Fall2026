# CMPE311 Project 5: PROJECT-USERTOS (FreeRTOS)

Same behavior as Project 4 (two LEDs set over the serial monitor, push-button
PWM fan), rebuilt on FreeRTOS 202604.01 LTS (FreeRTOS-Kernel V11.3.1).

| Path | Description |
|---|---|
| `projectUserTOS/projectUserTOS.ino` | Sketch: creates the LED1, LED2, fan and serial tasks, starts FreeRTOS |
| `projectUserTOS/myRegs.h` | All register constants (no avr.h names) |
| `projectUserTOS/myFan.h` | Fan PWM, button ISR, fan task |
| `projectUserTOS/src/FreeRTOS/` | FreeRTOS-Kernel V11.3.1 files (unchanged), ATmega328P port (`port.c`), `FreeRTOSConfig.h` |
| `projectUserTOS_SDD_RevE.docx` | System Design Document, Revision E |
| `projectUserTOS_ECN-004.docx` | Engineering Change Note ECN-004 |

Open `projectUserTOS/projectUserTOS.ino` in Arduino IDE 2.3+ (keep the whole
folder, including `src`). Do not install a FreeRTOS library from the Library
Manager. Upload, open the Serial Monitor at 9600 baud with Newline, enter
`2`, `600`, `1`, `1600`, and press the button to change fan speed.
Wiring is unchanged from Project 4.

FreeRTOS is MIT licensed (see `src/FreeRTOS/LICENSE.md`).

Author: Dulcey Comeau, UMBC
