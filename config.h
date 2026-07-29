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
//
// Three levels holding nine questions between them. A level is not a single
// question: it is a run of questions (the QUIZ phase) followed by the digits of
// the code that level unlocks (the CODE phase).
//
//   Level | Questions | Code digits it locks
//     1   |     5     | 1, 2, 3
//     2   |     3     | 4
//     3   |     1     | 5
//
// LEVEL_QUESTIONS and LEVEL_CODE_DIGITS are the only source of truth for those
// counts. The sketch derives every offset into ANSWER_KEYS and the code by summing
// them, so there is no second table that has to be kept in agreement.

#define NUM_LEVELS       3
#define TOTAL_QUESTIONS  9   // must equal the sum of LEVEL_QUESTIONS
#define CODE_LENGTH      5   // must equal the sum of LEVEL_CODE_DIGITS

#define NUM_SETS    3   // three separate sets of paper questions, each with its own
                        // four cards and its own answer key. The player picks one at
                        // boot by scanning any card from it.
#define NUM_CARDS   4
#define UID_LENGTH  4   // Mifare Classic 1K UIDs are 4 bytes. If uid_dump reports
                        // a 7-byte UID, widen this to 7 and re-run it.

// constexpr, not plain const, so the static_asserts below can read them.
constexpr uint8_t LEVEL_QUESTIONS[NUM_LEVELS]   = { 5, 3, 1 };
constexpr uint8_t LEVEL_CODE_DIGITS[NUM_LEVELS] = { 3, 1, 1 };

// Change a level's question or digit count above and these catch the totals you
// forgot to change with it. Without them the mismatch is silent and shows up as
// the wrong answer key on the last question, or a code slot nobody can dial.
static_assert(LEVEL_QUESTIONS[0] + LEVEL_QUESTIONS[1] + LEVEL_QUESTIONS[2]
              == TOTAL_QUESTIONS, "TOTAL_QUESTIONS must equal the sum of LEVEL_QUESTIONS");
static_assert(LEVEL_CODE_DIGITS[0] + LEVEL_CODE_DIGITS[1] + LEVEL_CODE_DIGITS[2]
              == CODE_LENGTH, "CODE_LENGTH must equal the sum of LEVEL_CODE_DIGITS");

// Card identities. The numeric values index into CARD_UIDS below.
enum CardId : uint8_t {
  CARD_A       = 0,
  CARD_B       = 1,
  CARD_C       = 2,
  CARD_D       = 3,
  CARD_UNKNOWN = 4,   // a card was read, but its UID is not in the table
  CARD_NONE    = 5    // nothing scanned yet this question
};

// Which card answers each question, one row per set, laid out level by level. Level 1
// has five questions but there are only four cards, so one card must repeat in each
// row - kept far apart so it does not read as a pattern. Each row uses all four cards
// in its level-1 span and never repeats a card back-to-back.
//
// The three sets are different paper questions, so each has its own key. Edit a row to
// match whatever the paper for that set actually says.
// The outer dimension is left for the initializer to fill, so the static_assert
// below actually counts the rows written here. Pin it to [NUM_SETS] instead and the
// assert turns tautological - a forgotten row would just zero-fill (to all card A)
// and pass silently.
const CardId ANSWER_KEYS[][TOTAL_QUESTIONS] = {
  //  level 1 (Q1-Q5)                       level 2 (Q1-Q3)         level 3
  { CARD_B, CARD_D, CARD_A, CARD_C, CARD_D,  CARD_A, CARD_C, CARD_B,  CARD_D },  // set 1
  { CARD_C, CARD_A, CARD_D, CARD_B, CARD_A,  CARD_D, CARD_B, CARD_C,  CARD_A },  // set 2
  { CARD_D, CARD_B, CARD_C, CARD_A, CARD_B,  CARD_C, CARD_A, CARD_D,  CARD_B },  // set 3
};

static_assert(sizeof(ANSWER_KEYS) / sizeof(ANSWER_KEYS[0]) == NUM_SETS,
              "ANSWER_KEYS must have exactly NUM_SETS rows");
static_assert(sizeof(ANSWER_KEYS[0]) / sizeof(ANSWER_KEYS[0][0]) == TOTAL_QUESTIONS,
              "each ANSWER_KEYS row must have exactly TOTAL_QUESTIONS entries");

// ---------------------------------------------------------------------------
// Card UIDs
// ---------------------------------------------------------------------------
//
// Twelve cards: three sets of four (A, B, C, D). The values below were dumped
// from the physical cards and transcribed from rfid-uid.txt, in file order -
// lines 1-4 are set 1's A/B/C/D, lines 5-8 set 2, lines 9-12 set 3.
//
// ORDER MATTERS on two axes. The outer index picks the set (0=set 1, ...); the
// inner index must be A, B, C, D in that order, because ANSWER_KEYS above indexes
// into it. A wrong row scores that set's questions against the wrong card.
//
// To replace a card, flash tools/uid_dump/uid_dump.ino, tap it, and paste the new
// UID into the right [set][letter] slot. All-zero slots are treated as unconfigured.

// Outer dimension left implicit for the same reason as ANSWER_KEYS: it makes the
// row-count static_assert below real rather than tautological.
const byte CARD_UIDS[][NUM_CARDS][UID_LENGTH] = {
  {                             // set 1
    {0xAB, 0x28, 0x07, 0x13},   //   A
    {0xF3, 0xB6, 0x98, 0x19},   //   B
    {0x9C, 0xBA, 0x01, 0x02},   //   C
    {0x4D, 0xE3, 0x01, 0x02},   //   D
  },
  {                             // set 2
    {0x5A, 0xB2, 0x00, 0x02},   //   A
    {0x71, 0xC9, 0x01, 0x02},   //   B
    {0x27, 0xA7, 0x00, 0x02},   //   C
    {0xC5, 0x3C, 0x01, 0x02},   //   D
  },
  {                             // set 3
    {0x19, 0x1A, 0x01, 0x02},   //   A
    {0x7D, 0x1F, 0x02, 0x02},   //   B
    {0x49, 0x4D, 0x00, 0x02},   //   C
    {0x1A, 0x5C, 0x02, 0x02},   //   D
  },
};

static_assert(sizeof(CARD_UIDS) / sizeof(CARD_UIDS[0]) == NUM_SETS,
              "CARD_UIDS must have exactly NUM_SETS sets");

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

// A single potentiometer, shared by every level: during a level's CODE phase it sets
// the digit for the slot being dialed, which is latched when that level's button is
// pressed. It does nothing during the QUIZ phase. A1-A4 are free.
#define POT_PIN  A0

// One LED and one button per LEVEL. Pins 28/30 (LEDs 4-5) and 29/31 (buttons 4-5)
// are left over from the old five-round build: still wired, deliberately unused.
const uint8_t LED_PINS[NUM_LEVELS]    = { 22, 24, 26 };
const uint8_t BUTTON_PINS[NUM_LEVELS] = { 23, 25, 27 };

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

// LED feedback. A correct card answer blinks twice, a correctly dialed code digit
// blinks once, and a wrong answer does not blink at all - the absence IS the "no".
// Keep the blink well clear of BUTTON_DEBOUNCE_MS so the two never read as one event.
#define BLINK_ON_MS      120
#define BLINK_OFF_MS     120
#define CORRECT_BLINKS     2
#define DIGIT_BLINKS       1

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

// One tune per level's button, rising level to level, so a player can hear which
// level they are on without looking at the LCD.
const Note TUNE_BUTTON_1[] = { {440, 90}, {554, 110} };
const Note TUNE_BUTTON_2[] = { {523, 90}, {659, 110} };
const Note TUNE_BUTTON_3[] = { {659, 90}, {831, 110} };

const Note* const BUTTON_TUNES[NUM_LEVELS] = {
  TUNE_BUTTON_1, TUNE_BUTTON_2, TUNE_BUTTON_3
};
const uint8_t BUTTON_TUNE_LENGTHS[NUM_LEVELS] = { 2, 2, 2 };

// Beep acknowledging an RFID scan. Plays on every card read, including cards
// that are not one of the four (those show '?' on the LCD).
// Lengthen the duration if it is hard to hear in a noisy room.
const Note TUNE_SCAN[] = { {1200, 90} };
const uint8_t TUNE_SCAN_LENGTH = 1;

// A wrong card, a wrong digit, or a button press with nothing scanned. Fires on
// every miss now rather than once at the end, so keep it short and un-annoying:
// the player will hear it repeatedly while they work a question out.
const Note TUNE_WRONG[] = { {330, 120}, {247, 200} };
const uint8_t TUNE_WRONG_LENGTH = 2;

// A code digit accepted, with more slots still to dial in this level.
const Note TUNE_DIGIT_LOCK[] = { {880, 70}, {1175, 90} };
const uint8_t TUNE_DIGIT_LOCK_LENGTH = 2;

// A level's last digit accepted: its LED has just gone solid.
const Note TUNE_LEVEL_CLEAR[] = { {659, 100}, {784, 100}, {1047, 200} };
const uint8_t TUNE_LEVEL_CLEAR_LENGTH = 3;

// All three levels cleared.
const Note TUNE_WIN[]  = { {523, 120}, {659, 120}, {784, 120}, {1047, 260} };
const uint8_t TUNE_WIN_LENGTH = 4;

#endif  // CONFIG_H
