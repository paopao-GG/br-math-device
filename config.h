/*
 * config.h - all tunable values for the BR Math Device.
 *
 * Everything you are likely to want to change lives in this file:
 * pin assignments, the answer key, the card UIDs, and the buzzer tunes.
 */

#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// ---------------------------------------------------------------------------
// Game shape
// ---------------------------------------------------------------------------

#define NUM_ROUNDS  5
#define NUM_CARDS   4
#define UID_LENGTH  4   // Mifare Classic 1K UIDs are 4 bytes. If uid_dump reports
                        // a 7-byte UID, widen this to 7 and re-run it.

// Card identities. The numeric values index into CARD_UIDS below.
enum CardId : uint8_t {
  CARD_A       = 0,
  CARD_B       = 1,
  CARD_C       = 2,
  CARD_D       = 3,
  CARD_UNKNOWN = 4,   // a card was read, but its UID is not in the table
  CARD_NONE    = 5    // nothing scanned yet this round
};

// Which card is the correct answer for each round.
// Round:                                 1       2       3       4       5
const CardId ANSWER_KEY[NUM_ROUNDS] = { CARD_C, CARD_D, CARD_A, CARD_B, CARD_C };

// ---------------------------------------------------------------------------
// Card UIDs  <-- FILL THIS IN
// ---------------------------------------------------------------------------
//
// Flash tools/uid_dump/uid_dump.ino, open the Serial Monitor at 9600 baud, tap
// each card, and paste the four "Paste this:" lines below.
//
// ORDER MATTERS. Row 0 must be card A, row 1 card B, row 2 card C, row 3 card D.
// The answer key above indexes into these rows, so a wrong order means every
// round is scored against the wrong card.
//
// The all-zero rows below are placeholders. While they are still here, the
// device prints a warning on Serial at boot and no scan will ever be correct.

const byte CARD_UIDS[NUM_CARDS][UID_LENGTH] = {
  {0xFA, 0x4F, 0x37, 0x80},   // A
  {0x1C, 0xA3, 0x17, 0x49},   // B
  {0xAB, 0x28, 0x07, 0x13},   // C
  {0x8F, 0x42, 0x77, 0x29},   // D
};

// ---------------------------------------------------------------------------
// Pins (Arduino Mega 2560)
// ---------------------------------------------------------------------------

// RC522 RFID reader, on the Mega's hardware SPI pins.
// SCK=52, MOSI=51, MISO=50 are fixed by the hardware and are not listed here.
// POWER THE RC522 FROM 3.3V. 5V will destroy it.
#define RFID_SS_PIN   53
#define RFID_RST_PIN  49

// 1602 LCD on I2C (Mega: SDA=20, SCL=21).
// If the screen stays blank/garbled, run an I2C scanner and try 0x3F.
#define LCD_ADDR      0x27
#define LCD_COLS      16
#define LCD_ROWS      2

// A single potentiometer, shared by every round: it sets the current round's digit,
// which is latched when that round's button is pressed. The player re-dials it each
// round. A1-A4 are free.
#define POT_PIN  A0

// One LED and one button per round.
const uint8_t LED_PINS[NUM_ROUNDS]    = { 22, 24, 26, 28, 30 };
const uint8_t BUTTON_PINS[NUM_ROUNDS] = { 23, 25, 27, 29, 31 };

// Passive buzzer, driven with tone().
#define BUZZER_PIN    8

// Left deliberately UNCONNECTED. Its floating noise seeds random(), so the
// 5-digit code differs on every power-up. Wiring anything here makes the code
// repeat identically every boot.
#define RANDOM_SEED_PIN  A15

// ---------------------------------------------------------------------------
// Input tuning
// ---------------------------------------------------------------------------

#define BUTTON_DEBOUNCE_MS  30

#define POT_MIN_DIGIT    1
#define POT_MAX_DIGIT    9
#define POT_SAMPLES      4    // ADC reads averaged per poll, to damp noise
#define POT_HYSTERESIS  12    // ADC counts the value must travel PAST a band
                              // edge before the digit changes. Without this the
                              // digit flickers when a pot rests on a boundary.
                              // Raise it if you still see flicker; lower it if
                              // the pot feels sticky.

// ---------------------------------------------------------------------------
// Buzzer tunes
// ---------------------------------------------------------------------------

struct Note {
  uint16_t freq;     // Hz
  uint16_t duration; // ms
};

// One tune per button, so each round sounds distinct.
const Note TUNE_BUTTON_1[] = { {440, 90}, {554, 110} };
const Note TUNE_BUTTON_2[] = { {494, 90}, {622, 110} };
const Note TUNE_BUTTON_3[] = { {523, 90}, {659, 110} };
const Note TUNE_BUTTON_4[] = { {587, 90}, {740, 110} };
const Note TUNE_BUTTON_5[] = { {659, 90}, {831, 110} };

const Note* const BUTTON_TUNES[NUM_ROUNDS] = {
  TUNE_BUTTON_1, TUNE_BUTTON_2, TUNE_BUTTON_3, TUNE_BUTTON_4, TUNE_BUTTON_5
};
const uint8_t BUTTON_TUNE_LENGTHS[NUM_ROUNDS] = { 2, 2, 2, 2, 2 };

// Beep acknowledging an RFID scan. Plays on every card read, including cards
// that are not one of the four (those show '?' on the LCD).
// Lengthen the duration if it is hard to hear in a noisy room.
const Note TUNE_SCAN[] = { {1200, 90} };
const uint8_t TUNE_SCAN_LENGTH = 1;

// End of game.
const Note TUNE_WIN[]  = { {523, 120}, {659, 120}, {784, 120}, {1047, 260} };
const uint8_t TUNE_WIN_LENGTH = 4;

const Note TUNE_FAIL[] = { {330, 160}, {247, 320} };
const uint8_t TUNE_FAIL_LENGTH = 2;

#endif  // CONFIG_H
