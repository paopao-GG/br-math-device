/*
 * BR Math Device
 *
 * A 5-round multiple-choice quiz prop. The questions themselves are on paper;
 * the device only judges the answers.
 *
 * On power-up it picks a random 5-digit code (digits 1-9) and shows it on the
 * LCD. In round N the player scans one of four RFID cards (A/B/C/D) as their
 * answer, dials the potentiometer to digit N of the code, then presses button N
 * to lock in. LED N lights only if BOTH the card and the dialed digit are
 * correct. After round 5 the device shows the score and plays a win/fail tune.
 *
 * There is ONE potentiometer, shared by all five rounds. It drives whichever
 * round is current; pressing that round's button latches the digit, freeing the
 * pot for the next round. Rounds not yet reached show '-' on the LCD.
 *
 * Before this sketch is useful, the four card UIDs must be filled into
 * CARD_UIDS in config.h. See tools/uid_dump/uid_dump.ino.
 *
 * Wiring is documented in config.h. The one thing worth repeating: the RC522
 * runs on 3.3V, not 5V.
 */

#include <SPI.h>
#include <MFRC522.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

#include "config.h"

// ---------------------------------------------------------------------------
// State
// ---------------------------------------------------------------------------

enum GameState : uint8_t {
  STATE_BOOT,
  STATE_PLAYING,
  STATE_RESULT
};

MFRC522 mfrc522(RFID_SS_PIN, RFID_RST_PIN);
LiquidCrystal_I2C lcd(LCD_ADDR, LCD_COLS, LCD_ROWS);

GameState state = STATE_BOOT;
uint8_t   currentRound = 0;

uint8_t codeDigit[NUM_ROUNDS];      // the secret code, 1-9 per round
uint8_t potDigit;                   // the one pot's live reading, 1-9. Shared by every
                                    // round: it drives whichever round is current.
uint8_t lockedDigit[NUM_ROUNDS];    // digit captured when the round was locked in
bool    roundCorrect[NUM_ROUNDS];   // did the player get the round right

CardId scannedCard = CARD_NONE;     // pending answer for the current round

// Button debounce, one slot per button.
bool          buttonStable[NUM_ROUNDS];      // true = pressed
bool          buttonLastRead[NUM_ROUNDS];
unsigned long buttonLastChangeMs[NUM_ROUNDS];

// Last strings pushed to the LCD, so we only redraw on change.
char prevLine1[LCD_COLS + 1] = "";
char prevLine2[LCD_COLS + 1] = "";

// ---------------------------------------------------------------------------
// Buzzer
// ---------------------------------------------------------------------------

// Blocking, which is fine: the game is turn-based and nothing needs servicing
// while a tune plays.
void playTune(const Note* tune, uint8_t length) {
  for (uint8_t i = 0; i < length; i++) {
    tone(BUZZER_PIN, tune[i].freq, tune[i].duration);
    delay(tune[i].duration + 25);
  }
  noTone(BUZZER_PIN);
}

// ---------------------------------------------------------------------------
// Potentiometer (one, shared by all rounds)
// ---------------------------------------------------------------------------

const uint8_t POT_BANDS = POT_MAX_DIGIT - POT_MIN_DIGIT + 1;

// The ADC range is split into POT_BANDS equal slices, one per digit.
int bandLow(uint8_t digit) {
  return ((long)(digit - POT_MIN_DIGIT) * 1024L) / POT_BANDS;
}

int bandHigh(uint8_t digit) {
  return (((long)(digit - POT_MIN_DIGIT + 1) * 1024L) / POT_BANDS) - 1;
}

uint8_t rawToDigit(int raw) {
  int digit = ((long)raw * POT_BANDS) / 1024L + POT_MIN_DIGIT;
  return constrain(digit, POT_MIN_DIGIT, POT_MAX_DIGIT);
}

int readPotRaw() {
  long sum = 0;
  for (uint8_t i = 0; i < POT_SAMPLES; i++) {
    sum += analogRead(POT_PIN);
  }
  return sum / POT_SAMPLES;
}

// Only move off the current digit once the pot has travelled POT_HYSTERESIS
// counts PAST the edge of that digit's band. A pot resting exactly on a band
// edge would otherwise flicker between two digits on ADC noise alone, which
// makes the code impossible to dial reliably.
void updatePotDigit() {
  int raw = readPotRaw();

  if (raw < bandLow(potDigit) - POT_HYSTERESIS ||
      raw > bandHigh(potDigit) + POT_HYSTERESIS) {
    potDigit = rawToDigit(raw);
  }
}

// ---------------------------------------------------------------------------
// Buttons
// ---------------------------------------------------------------------------

// Returns true once per press, on the debounced falling edge.
bool buttonPressed(uint8_t round) {
  bool reading = (digitalRead(BUTTON_PINS[round]) == LOW);  // INPUT_PULLUP

  if (reading != buttonLastRead[round]) {
    buttonLastRead[round] = reading;
    buttonLastChangeMs[round] = millis();
    return false;
  }

  if (millis() - buttonLastChangeMs[round] < BUTTON_DEBOUNCE_MS) {
    return false;
  }

  if (reading != buttonStable[round]) {
    buttonStable[round] = reading;
    return reading;  // fire on press, not release
  }

  return false;
}

// ---------------------------------------------------------------------------
// RFID
// ---------------------------------------------------------------------------

// CARD_NONE means nothing was on the reader this pass, so an existing pending
// answer is left alone.
CardId pollRfid() {
  if (!mfrc522.PICC_IsNewCardPresent()) return CARD_NONE;
  if (!mfrc522.PICC_ReadCardSerial())   return CARD_NONE;

  CardId found = CARD_UNKNOWN;

  if (mfrc522.uid.size == UID_LENGTH) {
    for (uint8_t c = 0; c < NUM_CARDS; c++) {
      if (memcmp(mfrc522.uid.uidByte, CARD_UIDS[c], UID_LENGTH) == 0) {
        found = (CardId)c;
        break;
      }
    }
  }

  // Required, or the same card cannot be scanned a second time.
  mfrc522.PICC_HaltA();
  mfrc522.PCD_StopCrypto1();

  return found;
}

bool uidsConfigured() {
  for (uint8_t c = 0; c < NUM_CARDS; c++) {
    for (uint8_t b = 0; b < UID_LENGTH; b++) {
      if (CARD_UIDS[c][b] != 0x00) return true;
    }
  }
  return false;
}

// ---------------------------------------------------------------------------
// Display
// ---------------------------------------------------------------------------

char cardLetter(CardId card) {
  switch (card) {
    case CARD_A: return 'A';
    case CARD_B: return 'B';
    case CARD_C: return 'C';
    case CARD_D: return 'D';
    case CARD_UNKNOWN: return '?';   // card read, but not one of ours
    default: return '-';             // nothing scanned yet
  }
}

// The LCD is on I2C and slow. Redrawing every loop makes it visibly flicker,
// so only push a line when its content actually changed.
void writeLine(uint8_t row, const char* text, char* previous) {
  if (strcmp(text, previous) == 0) return;

  lcd.setCursor(0, row);

  uint8_t i = 0;
  for (; i < LCD_COLS && text[i] != '\0'; i++) {
    lcd.print(text[i]);
  }
  for (; i < LCD_COLS; i++) {
    lcd.print(' ');   // blank out whatever the previous line left behind
  }

  strncpy(previous, text, LCD_COLS);
  previous[LCD_COLS] = '\0';
}

void renderPlaying() {
  char line1[LCD_COLS + 1];
  char line2[LCD_COLS + 1];

  // e.g. "CODE:47391 Q3:C"
  snprintf(line1, sizeof(line1), "CODE:%u%u%u%u%u Q%u:%c",
           codeDigit[0], codeDigit[1], codeDigit[2], codeDigit[3], codeDigit[4],
           currentRound + 1, cardLetter(scannedCard));

  // e.g. "1:42:73:34:-5:-" - the format from the design doc. With a single shared
  // pot, only the current round has a live digit: finished rounds show the digit
  // they locked in, and rounds not yet reached show '-'.
  char* cursor = line2;
  for (uint8_t i = 0; i < NUM_ROUNDS; i++) {
    if (i < currentRound) {
      cursor += sprintf(cursor, "%u:%u", i + 1, lockedDigit[i]);
    } else if (i == currentRound) {
      cursor += sprintf(cursor, "%u:%u", i + 1, potDigit);
    } else {
      cursor += sprintf(cursor, "%u:-", i + 1);
    }
  }

  writeLine(0, line1, prevLine1);
  writeLine(1, line2, prevLine2);
}

void renderResult() {
  uint8_t score = 0;
  for (uint8_t i = 0; i < NUM_ROUNDS; i++) {
    if (roundCorrect[i]) score++;
  }

  char line1[LCD_COLS + 1];
  snprintf(line1, sizeof(line1), "SCORE: %u/%u", score, NUM_ROUNDS);

  writeLine(0, line1, prevLine1);
  writeLine(1, (score == NUM_ROUNDS) ? "ALL CORRECT!" : "TRY AGAIN", prevLine2);

  if (score == NUM_ROUNDS) {
    playTune(TUNE_WIN, TUNE_WIN_LENGTH);
  } else {
    playTune(TUNE_FAIL, TUNE_FAIL_LENGTH);
  }

  Serial.print(F("Final score: "));
  Serial.print(score);
  Serial.print('/');
  Serial.println(NUM_ROUNDS);
}

// ---------------------------------------------------------------------------
// Game flow
// ---------------------------------------------------------------------------

void startRound(uint8_t round) {
  currentRound = round;
  scannedCard = CARD_NONE;

  // Sync the debouncer to whatever this button is doing right now, so a button
  // already held down when the round opens does not read as a fresh press.
  bool held = (digitalRead(BUTTON_PINS[round]) == LOW);
  buttonStable[round] = held;
  buttonLastRead[round] = held;
  buttonLastChangeMs[round] = millis();

  Serial.print(F("Round "));
  Serial.print(round + 1);
  Serial.print(F(" - answer is card "));
  Serial.print(cardLetter(ANSWER_KEY[round]));
  Serial.print(F(", code digit is "));
  Serial.println(codeDigit[round]);
}

void lockInRound() {
  uint8_t round = currentRound;

  // Latching is what lets one pot serve every round: the digit is frozen here, so
  // the pot is free to move on to the next round without disturbing this one.
  lockedDigit[round] = potDigit;

  bool cardOk  = (scannedCard == ANSWER_KEY[round]);
  bool digitOk = (lockedDigit[round] == codeDigit[round]);
  roundCorrect[round] = cardOk && digitOk;

  digitalWrite(LED_PINS[round], roundCorrect[round] ? HIGH : LOW);

  Serial.print(F("  locked in: card "));
  Serial.print(cardLetter(scannedCard));
  Serial.print(cardOk ? F(" (ok)") : F(" (wrong)"));
  Serial.print(F(", digit "));
  Serial.print(lockedDigit[round]);
  Serial.println(digitOk ? F(" (ok)") : F(" (wrong)"));

  playTune(BUTTON_TUNES[round], BUTTON_TUNE_LENGTHS[round]);

  if (round + 1 < NUM_ROUNDS) {
    startRound(round + 1);
  } else {
    currentRound = NUM_ROUNDS;
    state = STATE_RESULT;
    renderResult();
  }
}

void newGame() {
  randomSeed(analogRead(RANDOM_SEED_PIN));

  for (uint8_t i = 0; i < NUM_ROUNDS; i++) {
    codeDigit[i] = random(1, 10);   // 1-9, never 0
    lockedDigit[i] = 0;
    roundCorrect[i] = false;
    digitalWrite(LED_PINS[i], LOW);
  }

  potDigit = rawToDigit(readPotRaw());   // start from wherever the pot actually is

  Serial.print(F("Code: "));
  for (uint8_t i = 0; i < NUM_ROUNDS; i++) Serial.print(codeDigit[i]);
  Serial.println();

  lcd.clear();
  prevLine1[0] = '\0';
  prevLine2[0] = '\0';

  lcd.setCursor(0, 0);
  lcd.print(F("BR MATH DEVICE"));
  lcd.setCursor(0, 1);
  lcd.print(F("GET READY..."));
  delay(1500);
  lcd.clear();

  state = STATE_PLAYING;
  startRound(0);
}

// ---------------------------------------------------------------------------
// Arduino entry points
// ---------------------------------------------------------------------------

void setup() {
  Serial.begin(9600);

  for (uint8_t i = 0; i < NUM_ROUNDS; i++) {
    pinMode(LED_PINS[i], OUTPUT);
    digitalWrite(LED_PINS[i], LOW);
    pinMode(BUTTON_PINS[i], INPUT_PULLUP);
  }
  pinMode(BUZZER_PIN, OUTPUT);

  SPI.begin();
  mfrc522.PCD_Init();

  // Several libraries are called "LiquidCrystal_I2C" and they disagree on how to
  // start the display: Frank de Brabander's uses begin(), the others use init().
  // FDB_LIQUID_CRYSTAL_I2C_H is de Brabander's include guard, so this picks the
  // right call whichever one is installed.
#ifdef FDB_LIQUID_CRYSTAL_I2C_H
  lcd.begin();
#else
  lcd.init();
#endif
  lcd.backlight();

  Serial.println(F("=== BR Math Device ==="));

  byte version = mfrc522.PCD_ReadRegister(MFRC522::VersionReg);
  if (version == 0x00 || version == 0xFF) {
    Serial.println(F("WARNING: RC522 not responding. Check wiring; VCC must be 3.3V."));
  }

  if (!uidsConfigured()) {
    Serial.println(F("WARNING: CARD_UIDS in config.h is still all zeros."));
    Serial.println(F("         Run tools/uid_dump and paste the four UIDs in."));
    Serial.println(F("         Until then, no scan can ever be scored correct."));
  }

  newGame();
}

void loop() {
  if (state != STATE_PLAYING) return;   // game is over; power-cycle to restart

  CardId scan = pollRfid();
  if (scan != CARD_NONE) {
    scannedCard = scan;   // last scan wins - rescanning replaces the answer
    playTune(TUNE_SCAN, TUNE_SCAN_LENGTH);
    Serial.print(F("  scanned card "));
    Serial.println(cardLetter(scan));
  }

  updatePotDigit();

  renderPlaying();

  // Only the current round's button is live.
  if (buttonPressed(currentRound)) {
    lockInRound();
  }
}
