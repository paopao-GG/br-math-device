/*
 * BR Math Device
 *
 * A 3-level multiple-choice quiz prop. The questions themselves are on paper;
 * the device only judges the answers.
 *
 * There are three separate SETS of paper questions, each with its own four RFID
 * cards (twelve cards in all) and its own answer key. At boot the player picks a
 * set by scanning any card from it and pressing button 1 to confirm; the rest of
 * the game then runs against that set alone.
 *
 * Three levels hold nine questions between them - five, then three, then one.
 * Each level runs in two phases:
 *
 *   QUIZ  the level's questions, one at a time. Scan one of four RFID cards
 *         (A/B/C/D) as the answer and press the level's button. Correct, the LED
 *         blinks twice and the next question opens. Wrong, it does not blink at
 *         all and the SAME question stays live - there is no way past a question
 *         except by answering it.
 *
 *   CODE  once every question is cleared, the level unlocks its share of the
 *         5-digit code: three digits for level 1, one each for levels 2 and 3.
 *         Dial each digit on the potentiometer and press the same button to lock
 *         it. The level's last digit turns its LED SOLID and opens the next level.
 *
 * On power-up the device picks a random 5-digit code (digits 1-9) and shows it on
 * the LCD. Because a wrong answer can only be retried, every game ends solved;
 * what the device records instead is the number of misses along the way.
 *
 * There is ONE potentiometer. It does nothing during a QUIZ phase and drives the
 * digit being dialed during a CODE phase; pressing the button latches that digit,
 * freeing the pot for the next slot.
 *
 * LEDs and buttons 4 and 5 are left over from the earlier five-round build. They
 * stay wired and are deliberately unused.
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
  STATE_SELECT,    // choosing which question set to play, before the game proper
  STATE_PLAYING,
  STATE_DONE
};

// Which half of the current level we are in. A button press means "judge my
// card" in one and "lock this digit" in the other, so almost everything below
// dispatches on this.
enum Phase : uint8_t {
  PHASE_QUIZ,
  PHASE_CODE
};

MFRC522 mfrc522(RFID_SS_PIN, RFID_RST_PIN);
LiquidCrystal_I2C lcd(LCD_ADDR, LCD_COLS, LCD_ROWS);

GameState state = STATE_SELECT;
Phase     phase = PHASE_QUIZ;

uint8_t selectedSet = 0;       // which question set is being played, 0..NUM_SETS-1
int8_t  pendingSet  = -1;      // set highlighted during selection, -1 = none yet

uint8_t currentLevel    = 0;   // 0..NUM_LEVELS-1
uint8_t currentQuestion = 0;   // index within the current level
uint8_t currentSlot     = 0;   // code-digit index within the current level

uint8_t codeDigit[CODE_LENGTH];     // the secret code, 1-9 per slot
uint8_t lockedDigit[CODE_LENGTH];   // digits the player has locked in, for display
uint8_t potDigit;                   // the pot's live reading, 1-9. Meaningful only
                                    // during a CODE phase.

uint16_t misses = 0;                // wrong cards + wrong digits + empty presses

CardId scannedCard = CARD_NONE;     // pending answer for the current question

// Button debounce, one slot per level's button.
bool          buttonStable[NUM_LEVELS];      // true = pressed
bool          buttonLastRead[NUM_LEVELS];
unsigned long buttonLastChangeMs[NUM_LEVELS];

// Last strings pushed to the LCD, so we only redraw on change.
char prevLine1[LCD_COLS + 1] = "";
char prevLine2[LCD_COLS + 1] = "";

// ---------------------------------------------------------------------------
// Level geometry
// ---------------------------------------------------------------------------

// LEVEL_QUESTIONS and LEVEL_CODE_DIGITS in config.h are the only source of truth
// for the shape of the game. Everything else is summed from them here rather than
// written down a second time, so the two can never drift apart.

uint8_t questionBase(uint8_t level) {
  uint8_t base = 0;
  for (uint8_t i = 0; i < level; i++) base += LEVEL_QUESTIONS[i];
  return base;
}

uint8_t digitBase(uint8_t level) {
  uint8_t base = 0;
  for (uint8_t i = 0; i < level; i++) base += LEVEL_CODE_DIGITS[i];
  return base;
}

// Index into a set's ANSWER_KEYS row for the question on screen right now.
uint8_t currentQuestionIndex() {
  return questionBase(currentLevel) + currentQuestion;
}

// Index into codeDigit[] for the slot being dialed right now. Also the count of
// slots already locked, which is what the LCD needs to draw the code bar.
uint8_t currentSlotIndex() {
  return digitBase(currentLevel) + currentSlot;
}

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
// LEDs
// ---------------------------------------------------------------------------

// Blocking for the same reason playTune() is. Only ever called on the current
// level, whose LED is off - the cleared levels behind it are solid and stay solid.
void blinkLed(uint8_t level, uint8_t times) {
  for (uint8_t i = 0; i < times; i++) {
    digitalWrite(LED_PINS[level], HIGH);
    delay(BLINK_ON_MS);
    digitalWrite(LED_PINS[level], LOW);
    delay(BLINK_OFF_MS);
  }
}

// ---------------------------------------------------------------------------
// Potentiometer (one, shared by all levels)
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
bool buttonPressed(uint8_t level) {
  bool reading = (digitalRead(BUTTON_PINS[level]) == LOW);  // INPUT_PULLUP

  if (reading != buttonLastRead[level]) {
    buttonLastRead[level] = reading;
    buttonLastChangeMs[level] = millis();
    return false;
  }

  if (millis() - buttonLastChangeMs[level] < BUTTON_DEBOUNCE_MS) {
    return false;
  }

  if (reading != buttonStable[level]) {
    buttonStable[level] = reading;
    return reading;  // fire on press, not release
  }

  return false;
}

// Sync the debouncer to whatever this button is doing right now, so a button
// already held down when the level opens does not read as a fresh press.
void syncButton(uint8_t level) {
  bool held = (digitalRead(BUTTON_PINS[level]) == LOW);
  buttonStable[level] = held;
  buttonLastRead[level] = held;
  buttonLastChangeMs[level] = millis();
}

// ---------------------------------------------------------------------------
// RFID
// ---------------------------------------------------------------------------

// Reads a card if one is on the reader. Returns false when nothing was there this
// pass, so a pending answer is left alone. Returns true whenever a card was read -
// even a card that is none of ours - so the caller can still beep and show '?'.
// A wrong-size UID leaves out[] zeroed, which matches no real card.
bool readUid(byte out[UID_LENGTH]) {
  if (!mfrc522.PICC_IsNewCardPresent()) return false;
  if (!mfrc522.PICC_ReadCardSerial())   return false;

  memset(out, 0, UID_LENGTH);
  if (mfrc522.uid.size == UID_LENGTH) {
    memcpy(out, mfrc522.uid.uidByte, UID_LENGTH);
  }

  // Required, or the same card cannot be scanned a second time.
  mfrc522.PICC_HaltA();
  mfrc522.PCD_StopCrypto1();

  return true;
}

// Which of the given set's four cards this UID is, or CARD_UNKNOWN. During play a
// card from any OTHER set falls through to CARD_UNKNOWN and reads as '?'.
CardId matchInSet(const byte uid[UID_LENGTH], uint8_t set) {
  for (uint8_t c = 0; c < NUM_CARDS; c++) {
    if (memcmp(uid, CARD_UIDS[set][c], UID_LENGTH) == 0) return (CardId)c;
  }
  return CARD_UNKNOWN;
}

// Which set this UID belongs to, or -1 if it is none of the twelve. Used only
// during selection. UIDs are unique across sets, so the first match is the answer.
int8_t matchSet(const byte uid[UID_LENGTH]) {
  for (uint8_t s = 0; s < NUM_SETS; s++) {
    if (matchInSet(uid, s) != CARD_UNKNOWN) return s;
  }
  return -1;
}

bool uidsConfigured() {
  for (uint8_t s = 0; s < NUM_SETS; s++) {
    for (uint8_t c = 0; c < NUM_CARDS; c++) {
      for (uint8_t b = 0; b < UID_LENGTH; b++) {
        if (CARD_UIDS[s][c][b] != 0x00) return true;
      }
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

// Line 1 is the same in both phases apart from its Q/D marker: the code, then
// where we are. e.g. "CODE:47391 L1Q3" or "CODE:47391 L1D2".
void formatHeader(char* out, size_t size, char marker, uint8_t index) {
  snprintf(out, size, "CODE:%u%u%u%u%u L%u%c%u",
           codeDigit[0], codeDigit[1], codeDigit[2], codeDigit[3], codeDigit[4],
           currentLevel + 1, marker, index + 1);
}

// e.g. "ANS:C   [##...]" - the pending card, then one cell per question in this
// level: '#' answered, '.' still to come. The bar is 5, 3 or 1 wide by level.
void renderQuiz() {
  char line1[LCD_COLS + 1];
  char line2[LCD_COLS + 1];

  formatHeader(line1, sizeof(line1), 'Q', currentQuestion);

  char* cursor = line2;
  cursor += sprintf(cursor, "ANS:%c   [", cardLetter(scannedCard));
  for (uint8_t q = 0; q < LEVEL_QUESTIONS[currentLevel]; q++) {
    *cursor++ = (q < currentQuestion) ? '#' : '.';
  }
  *cursor++ = ']';
  *cursor = '\0';

  writeLine(0, line1, prevLine1);
  writeLine(1, line2, prevLine2);
}

// e.g. "DIAL:7  [47_..]" - the live pot digit, then all five code slots: the
// digits locked so far, '_' for the slot being dialed, '.' for slots no level
// has reached yet.
void renderCode() {
  char line1[LCD_COLS + 1];
  char line2[LCD_COLS + 1];

  formatHeader(line1, sizeof(line1), 'D', currentSlot);

  uint8_t lockedCount = currentSlotIndex();

  char* cursor = line2;
  cursor += sprintf(cursor, "DIAL:%u  [", potDigit);
  for (uint8_t s = 0; s < CODE_LENGTH; s++) {
    if (s < lockedCount)       *cursor++ = '0' + lockedDigit[s];
    else if (s == lockedCount) *cursor++ = '_';
    else                       *cursor++ = '.';
  }
  *cursor++ = ']';
  *cursor = '\0';

  writeLine(0, line1, prevLine1);
  writeLine(1, line2, prevLine2);
}

// Every game ends solved - a question cannot be skipped - so the score to report
// is how cleanly it was done, not whether it was done.
void renderResult() {
  char line1[LCD_COLS + 1];
  char line2[LCD_COLS + 1];

  snprintf(line1, sizeof(line1), "SOLVED!  %u%u%u%u%u",
           codeDigit[0], codeDigit[1], codeDigit[2], codeDigit[3], codeDigit[4]);
  snprintf(line2, sizeof(line2), "MISSES:%u", misses);

  writeLine(0, line1, prevLine1);
  writeLine(1, line2, prevLine2);

  playTune(TUNE_WIN, TUNE_WIN_LENGTH);

  Serial.print(F("Solved with "));
  Serial.print(misses);
  Serial.println(F(" miss(es)."));
}

// ---------------------------------------------------------------------------
// Game flow
// ---------------------------------------------------------------------------

void startQuestion() {
  scannedCard = CARD_NONE;

  Serial.print(F("  L"));
  Serial.print(currentLevel + 1);
  Serial.print(F(" Q"));
  Serial.print(currentQuestion + 1);
  Serial.print('/');
  Serial.print(LEVEL_QUESTIONS[currentLevel]);
  Serial.print(F(" expects card "));
  Serial.println(cardLetter(ANSWER_KEYS[selectedSet][currentQuestionIndex()]));
}

void enterCodePhase() {
  phase = PHASE_CODE;
  scannedCard = CARD_NONE;

  // The pot has been ignored for the whole quiz phase and may have been turned
  // in that time, so resync before showing it - otherwise the first digit on the
  // LCD is a stale reading that does not match where the knob is pointing.
  potDigit = rawToDigit(readPotRaw());

  Serial.print(F("  L"));
  Serial.print(currentLevel + 1);
  Serial.print(F(" questions cleared - dial "));
  Serial.print(LEVEL_CODE_DIGITS[currentLevel]);
  Serial.println(F(" code digit(s)"));
  Serial.print(F("  slot 1 expects digit "));
  Serial.println(codeDigit[currentSlotIndex()]);
}

void startLevel(uint8_t level) {
  currentLevel = level;
  currentQuestion = 0;
  currentSlot = 0;
  phase = PHASE_QUIZ;

  syncButton(level);

  Serial.print(F("Level "));
  Serial.print(level + 1);
  Serial.print(F(" - "));
  Serial.print(LEVEL_QUESTIONS[level]);
  Serial.print(F(" question(s), then "));
  Serial.print(LEVEL_CODE_DIGITS[level]);
  Serial.println(F(" code digit(s)"));

  startQuestion();
}

void finishGame() {
  state = STATE_DONE;
  renderResult();
}

// QUIZ phase: judge the scanned card. A wrong answer costs a miss and leaves the
// question exactly where it was - the only way forward is to get it right.
void submitAnswer() {
  bool correct = (scannedCard == ANSWER_KEYS[selectedSet][currentQuestionIndex()]);

  Serial.print(F("    answered "));
  Serial.print(cardLetter(scannedCard));
  Serial.println(correct ? F(" (correct)") : F(" (wrong - retry)"));

  if (!correct) {
    misses++;
    playTune(TUNE_WRONG, TUNE_WRONG_LENGTH);
    scannedCard = CARD_NONE;   // force a fresh scan rather than button-mashing
    return;
  }

  blinkLed(currentLevel, CORRECT_BLINKS);
  playTune(BUTTON_TUNES[currentLevel], BUTTON_TUNE_LENGTHS[currentLevel]);

  currentQuestion++;
  if (currentQuestion < LEVEL_QUESTIONS[currentLevel]) {
    startQuestion();
  } else {
    enterCodePhase();
  }
}

// CODE phase: judge the dialed digit, and on success latch it. Latching is what
// lets one pot serve every slot - the digit is frozen here, so the pot is free to
// move on without disturbing what is already locked.
void submitDigit() {
  uint8_t slot = currentSlotIndex();
  uint8_t dialed = potDigit;

  Serial.print(F("    dialed "));
  Serial.print(dialed);

  if (dialed != codeDigit[slot]) {
    Serial.print(F(" (wrong - expected "));
    Serial.print(codeDigit[slot]);
    Serial.println(F(", retry)"));
    misses++;
    playTune(TUNE_WRONG, TUNE_WRONG_LENGTH);
    return;
  }

  Serial.println(F(" (locked)"));
  lockedDigit[slot] = dialed;
  currentSlot++;

  if (currentSlot < LEVEL_CODE_DIGITS[currentLevel]) {
    blinkLed(currentLevel, DIGIT_BLINKS);
    playTune(TUNE_DIGIT_LOCK, TUNE_DIGIT_LOCK_LENGTH);
    Serial.print(F("  slot "));
    Serial.print(currentSlot + 1);
    Serial.print(F(" expects digit "));
    Serial.println(codeDigit[currentSlotIndex()]);
    return;
  }

  // Last digit of the level: the LED goes solid and stays solid for the rest of
  // the game, so the lit LEDs read as a progress bar across the three levels.
  digitalWrite(LED_PINS[currentLevel], HIGH);
  playTune(TUNE_LEVEL_CLEAR, TUNE_LEVEL_CLEAR_LENGTH);

  Serial.print(F("Level "));
  Serial.print(currentLevel + 1);
  Serial.println(F(" CLEARED"));

  if (currentLevel + 1 < NUM_LEVELS) {
    startLevel(currentLevel + 1);
  } else {
    finishGame();
  }
}

void newGame() {
  randomSeed(analogRead(RANDOM_SEED_PIN));

  for (uint8_t i = 0; i < CODE_LENGTH; i++) {
    codeDigit[i] = random(1, 10);   // 1-9, never 0
    lockedDigit[i] = 0;
  }
  for (uint8_t i = 0; i < NUM_LEVELS; i++) {
    digitalWrite(LED_PINS[i], LOW);
  }

  misses = 0;
  potDigit = rawToDigit(readPotRaw());   // start from wherever the pot actually is

  Serial.print(F("Code: "));
  for (uint8_t i = 0; i < CODE_LENGTH; i++) Serial.print(codeDigit[i]);
  Serial.println();

  lcd.clear();
  prevLine1[0] = '\0';
  prevLine2[0] = '\0';

  lcd.setCursor(0, 0);
  lcd.print(F("SET "));
  lcd.print(selectedSet + 1);
  lcd.print(F(" - START"));
  lcd.setCursor(0, 1);
  lcd.print(F("GET READY..."));
  delay(1500);
  lcd.clear();

  state = STATE_PLAYING;
  startLevel(0);
}

// ---------------------------------------------------------------------------
// Set selection (runs once at boot, before the game proper)
// ---------------------------------------------------------------------------

// Redrawn every loop, but writeLine only pushes a line when it actually changes,
// so the prompt is effectively static until a scan flips it.
void drawSelectPrompt() {
  if (pendingSet < 0) {
    writeLine(0, "SCAN TO PICK SET", prevLine1);
    writeLine(1, "ANY A/B/C/D CARD", prevLine2);
  } else {
    char line1[LCD_COLS + 1];
    snprintf(line1, sizeof(line1), "SET %d SELECTED", pendingSet + 1);
    writeLine(0, line1, prevLine1);
    writeLine(1, "PRESS 1 TO START", prevLine2);
  }
}

void startSelect() {
  state = STATE_SELECT;
  pendingSet = -1;

  // Sync button 1's debouncer so a button already held at boot cannot confirm a
  // set the instant one is scanned.
  syncButton(0);

  lcd.clear();
  prevLine1[0] = '\0';
  prevLine2[0] = '\0';
  drawSelectPrompt();

  Serial.println(F("Select a set: scan any card from it, then press button 1."));
}

void updateSelect() {
  byte uid[UID_LENGTH];
  if (readUid(uid)) {
    int8_t s = matchSet(uid);
    if (s >= 0) {
      playTune(TUNE_SCAN, TUNE_SCAN_LENGTH);
      pendingSet = s;
      Serial.print(F("  set "));
      Serial.print(s + 1);
      Serial.println(F(" detected - press 1 to confirm, or scan another set"));
    } else {
      playTune(TUNE_WRONG, TUNE_WRONG_LENGTH);
      Serial.println(F("  unknown card - not part of any set"));
    }
  }

  drawSelectPrompt();

  // Button 1 confirms the highlighted set. Ignored until a set has been scanned.
  if (pendingSet >= 0 && buttonPressed(0)) {
    selectedSet = pendingSet;
    Serial.print(F("Set "));
    Serial.print(selectedSet + 1);
    Serial.println(F(" selected."));
    newGame();
  }
}

// ---------------------------------------------------------------------------
// Arduino entry points
// ---------------------------------------------------------------------------

void setup() {
  Serial.begin(9600);

  for (uint8_t i = 0; i < NUM_LEVELS; i++) {
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
    Serial.println(F("         Run tools/uid_dump and paste the twelve UIDs in."));
    Serial.println(F("         Until then, no set can be selected or scored."));
  }

  startSelect();
}

void loop() {
  if (state == STATE_SELECT) { updateSelect(); return; }
  if (state != STATE_PLAYING) return;   // game is over; power-cycle to restart

  if (phase == PHASE_QUIZ) {
    byte uid[UID_LENGTH];
    if (readUid(uid)) {
      // Last scan wins. A card from another set matches nothing here and reads '?'.
      scannedCard = matchInSet(uid, selectedSet);
      playTune(TUNE_SCAN, TUNE_SCAN_LENGTH);
      Serial.print(F("    scanned card "));
      Serial.println(cardLetter(scannedCard));
    }
    renderQuiz();
  } else {
    // The pot is only read during a CODE phase. Reading it during the quiz would
    // put a live number on screen that means nothing yet.
    updatePotDigit();
    renderCode();
  }

  // Only the current level's button is live.
  if (buttonPressed(currentLevel)) {
    if (phase == PHASE_QUIZ) {
      submitAnswer();
    } else {
      submitDigit();
    }
  }
}
