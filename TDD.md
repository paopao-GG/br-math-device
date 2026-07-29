# TDD — Math Device

Technical design for a 3-level multiple-choice quiz device built on an Arduino Mega.
The questions are printed on paper; the device's job is to take the player's answer
physically — as an RFID card, and later as a dialed digit — and judge it.

## Hardware Components

| Qty | Component |
|-----|-----------|
| 1 | Arduino Mega |
| 1 | RFID reader (RC522) |
| 12 | RFID card — 3 sets of 4, each marked A, B, C, D |
| 3 | Push button |
| 3 | LED |
| 1 | Potentiometer |
| 1 | 1602 I2C LCD |
| 1 | Passive buzzer |

One LED and one push button belong to each of the 3 **levels** — not to each question.
The **single potentiometer is shared by every level**; it is used only when a level
reaches its code phase.

## Question sets

There are **three separate sets of paper questions**, each with its own four RFID cards
(twelve cards in all) and its own answer key. At boot the player picks one set; the rest
of the game runs against that set alone. Within the chosen set the four cards are the
A/B/C/D answers, reused for every question exactly as before — the set dimension sits
entirely in front of the game.

An earlier revision of this device used 5 LEDs and 5 buttons. Those extra parts stay
wired on the board and are simply unused — see WIRING.md.

## Game Shape

Three levels hold nine questions between them. A level is not one question: it is a run
of questions followed by the share of the code that level unlocks.

| Level | Questions | Code digits it locks | Button | LED |
|---|---|---|---|---|
| 1 | 5 | 1, 2, 3 | 23 | 22 |
| 2 | 3 | 4 | 25 | 24 |
| 3 | 1 | 5 | 27 | 26 |

The code is still **5 digits** — it is just no longer one digit per question.

## System / Game Flow

### 1. Power on — pick a set

Before the game starts the LCD asks for a set:

```
SCAN TO PICK SET
ANY A/B/C/D CARD
```

The player scans **any card from the set they want**. The device recognises which set
the card belongs to and shows it:

```
SET 2 SELECTED
PRESS 1 TO START
```

Scanning a card from a different set changes the choice; scanning a card that is none of
the twelve plays the miss tone and is ignored. Pressing **button 1** confirms the
highlighted set and begins the game. (Pressed before any card is scanned, it does
nothing.) To change sets after starting, power-cycle.

### 2. Start of game

Once a set is confirmed the system generates a **5-digit code** — digits 1–9 only, a new
code every restart — shows a brief `SET n - START / GET READY...` screen, then displays
the code. Line 1 is constant through the game; line 2 changes with the phase:

```
Quiz phase          Code phase
CODE:47391 L1Q3     CODE:47391 L1D2
ANS:C   [##...]     DIAL:7  [47_..]
```

- **Line 1** — the code, then the position: `L1Q3` is level 1, question 3; `L1D2` is
  level 1, code digit 2 of the 3 that level owns.
- **Line 2, quiz phase** — `ANS:` is the card currently scanned (`-` if none, `?` if the
  card is not one of the selected set's four). The bar has one cell per question *in this
  level* — 5, 3 or 1 wide — with `#` for answered and `.` for still to come.
- **Line 2, code phase** — `DIAL:` is the live potentiometer digit. The bar shows all
  five code slots: digits locked so far, `_` for the slot being dialed, `.` for slots
  no level has reached yet.

The pot is **not** shown during the quiz phase, because it does nothing there.

### 3. Quiz phase

Each question is one multiple-choice question on paper, with choices A, B, C, D. The
player:

1. Picks the RFID card marked with their answer and **scans it**. The buzzer beeps to
   confirm the read. Rescanning replaces the answer — the last scan counts.
2. Presses **the current level's button** to submit.

On the press:

- **Correct** — the level's LED **blinks twice**, and the next question opens.
- **Wrong, or no card scanned** — the LED does **not** blink, the buzzer plays the miss
  tune, and the *same question stays live*. The pending card is cleared, so the player
  must scan again rather than press repeatedly.

**There is no way past a question except by answering it correctly.** The device counts
misses but never blocks or ends the game.

### 4. Code phase

Once every question in the level is answered, the level switches to code entry. For
each digit slot the level owns, the player:

1. Turns **the potentiometer** to that digit of the code shown on the LCD.
2. Presses **the same button** to lock it.

- **Correct digit** — the LED blinks once, the digit latches, and the next slot opens.
- **Wrong digit** — no blink, miss tune, the slot stays live to be retried.
- **The level's last digit** — the LED turns **SOLID** and stays solid for the rest of
  the game, and the next level begins.

The button press latches the dialed digit, which is what allows a single potentiometer
to serve every slot: once latched, the digit is frozen, and turning the pot afterwards
cannot disturb it — it now drives the next slot instead.

### 5. End of game

After level 3's digit locks, all three LEDs are solid and the LCD shows:

```
SOLVED!  47391
MISSES:3
```

The buzzer plays the win tune. Because a question can only be retried, every game ends
solved — so what the device reports is how cleanly it was done, not whether it was done.
Power-cycle to play again with a new code.

## Rules

**Answer keys.** One card per question, and a separate key per set (the paper questions
differ between sets). Laid out level by level:

| Set | L1 Q1 | Q2 | Q3 | Q4 | Q5 | L2 Q1 | Q2 | Q3 | L3 Q1 |
|---|---|---|---|---|---|---|---|---|---|
| **1** | B | D | A | C | D | A | C | B | D |
| **2** | C | A | D | B | A | D | B | C | A |
| **3** | D | B | C | A | B | C | A | D | B |

Each key uses all four cards in its level-1 span, and no card repeats back-to-back
anywhere (including across level boundaries). Level 1 has five questions but only four
cards, so one card repeats per row, kept far apart so it does not read as a pattern.

These are fixed and do not change between restarts — only the 5-digit code is random.
They live in `ANSWER_KEYS` in [config.h](config.h) and are edited to match the paper.

**Cards.** Twelve, in three sets of four (A/B/C/D). Only the selected set's four cards
score; a card from another set reads as `?` and counts as wrong. The set is chosen once
at boot (see step 1) and cannot change without a power-cycle.

**Buttons.** Only the current level's button is live. The other two are ignored, so a
stray press cannot skip ahead. Buttons 4 and 5 from the old build are never read at all.
During set selection, button 1 is the confirm button.

**Potentiometer.** One pot, shared. It is idle during a quiz phase, and during a code
phase it drives the slot being dialed. The digit it is showing when the button is
pressed is the one that counts.

**Buzzer.** Beeps on every RFID scan, plays the level's tune on a correct answer, a miss
tune on any wrong answer or wrong digit, a lock tune per code digit, a clear tune when a
level's LED goes solid, and the win tune at the end.
