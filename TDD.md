# TDD — Math Device

Technical design for a 3-level multiple-choice quiz device built on an Arduino Mega.
The questions are printed on paper; the device's job is to take the player's answer
physically — as an RFID card, and later as a dialed digit — and judge it.

## Hardware Components

| Qty | Component |
|-----|-----------|
| 1 | Arduino Mega |
| 1 | RFID reader (RC522) |
| 4 | RFID card, marked A, B, C, D |
| 3 | Push button |
| 3 | LED |
| 1 | Potentiometer |
| 1 | 1602 I2C LCD |
| 1 | Passive buzzer |

One LED and one push button belong to each of the 3 **levels** — not to each question.
The **single potentiometer is shared by every level**; it is used only when a level
reaches its code phase. The same 4 RFID cards are reused for every question.

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

### 1. Power on

The system generates a **5-digit code** — digits 1–9 only, a new code every restart —
and displays it on the LCD. Line 1 is constant through the game; line 2 changes with
the phase:

```
Quiz phase          Code phase
CODE:47391 L1Q3     CODE:47391 L1D2
ANS:C   [##...]     DIAL:7  [47_..]
```

- **Line 1** — the code, then the position: `L1Q3` is level 1, question 3; `L1D2` is
  level 1, code digit 2 of the 3 that level owns.
- **Line 2, quiz phase** — `ANS:` is the card currently scanned (`-` if none, `?` if the
  card is not one of the four). The bar has one cell per question *in this level* — 5,
  3 or 1 wide — with `#` for answered and `.` for still to come.
- **Line 2, code phase** — `DIAL:` is the live potentiometer digit. The bar shows all
  five code slots: digits locked so far, `_` for the slot being dialed, `.` for slots
  no level has reached yet.

The pot is **not** shown during the quiz phase, because it does nothing there.

### 2. Quiz phase

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

### 3. Code phase

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

### 4. End of game

After level 3's digit locks, all three LEDs are solid and the LCD shows:

```
SOLVED!  47391
MISSES:3
```

The buzzer plays the win tune. Because a question can only be retried, every game ends
solved — so what the device reports is how cleanly it was done, not whether it was done.
Power-cycle to play again with a new code.

## Rules

**Answer key.** One card per question, laid out level by level:

| Level | Q1 | Q2 | Q3 | Q4 | Q5 |
|---|---|---|---|---|---|
| **1** | **B** | **D** | **A** | **C** | **D** |
| **2** | **A** | **C** | **B** | — | — |
| **3** | **D** | — | — | — | — |

Level 1 has five questions but there are only four cards, so one card must repeat: D is
used at Q2 and Q5, far enough apart that it does not read as a pattern. No card repeats
back-to-back anywhere, including across level boundaries.

This is fixed and does not change between restarts — only the 5-digit code is random.

**Buttons.** Only the current level's button is live. The other two are ignored, so a
stray press cannot skip ahead. Buttons 4 and 5 from the old build are never read at all.

**Potentiometer.** One pot, shared. It is idle during a quiz phase, and during a code
phase it drives the slot being dialed. The digit it is showing when the button is
pressed is the one that counts.

**Buzzer.** Beeps on every RFID scan, plays the level's tune on a correct answer, a miss
tune on any wrong answer or wrong digit, a lock tune per code digit, a clear tune when a
level's LED goes solid, and the win tune at the end.
