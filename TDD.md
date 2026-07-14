# TDD — Math Device

Technical design for a 5-round multiple-choice quiz device built on an Arduino Mega.
The questions are printed on paper; the device's job is to take the player's answer
physically — as an RFID card plus a dialed digit — and judge it.

## Hardware Components

| Qty | Component |
|-----|-----------|
| 1 | Arduino Mega |
| 1 | RFID reader (RC522) |
| 4 | RFID card, marked A, B, C, D |
| 5 | Push button |
| 5 | LED |
| 1 | Potentiometer |
| 1 | 1602 I2C LCD |
| 1 | Passive buzzer |

One LED and one push button belong to each of the 5 rounds. The **single potentiometer
is shared by every round** — it sets the digit for whichever round is current, and the
player re-dials it each round. The same 4 RFID cards are reused every round.

## System / Game Flow

### 1. Power on

The system generates a **5-digit code** — digits 1–9 only, a new code every restart —
and displays it on the LCD:

```
CODE:47391 Q3:C
1:42:73:34:-5:-
```

- **Line 1** — the code, the current round (`Q3`), and the card currently scanned
  (`-` if none, `?` if the card is not one of the four).
- **Line 2** — the five digit slots. Because there is only one potentiometer, only one
  slot is live at a time:

  | Slot | Shows |
  |---|---|
  | Rounds already locked in (1, 2 above) | the digit that was locked |
  | The current round (3 above) | the potentiometer, live |
  | Rounds not yet reached (4, 5 above) | `-` |

### 2. Answering a round

Each round has one multiple-choice question on paper, with choices A, B, C, D. In
round N the player:

1. Picks the RFID card marked with their answer and **scans it**. The buzzer beeps to
   confirm the read. Rescanning replaces the answer — the last scan counts.
2. Turns **the potentiometer** to digit N of the code shown on the LCD.
3. Presses **button N** to lock the answer in. Each button plays its own tune.

The button press latches the dialed digit, which is what allows a single potentiometer to
serve all five rounds: once latched, the digit is frozen, and turning the pot afterwards
cannot disturb it — it now drives the next round instead.

### 3. Scoring

On the button press, **LED N lights only if both of these are true**:

- the scanned card is the right answer for that round, **and**
- the dialed digit matches digit N of the code.

If either is wrong the LED stays off. Either way the game advances to the next round —
there are no retries. Pressing the button without scanning a card locks the round in as
wrong.

### 4. End of game

After round 5 the LCD shows `SCORE: n/5` and the buzzer plays a win tune if all 5 were
correct, a fail tune otherwise. The LEDs stay lit showing which rounds were right.
Power-cycle to play again with a new code.

## Rules

**Answer key.** The correct card changes each round. Across the 5 rounds it is:

| Round | 1 | 2 | 3 | 4 | 5 |
|-------|---|---|---|---|---|
| Correct card | C | D | A | B | C |

This is fixed and does not change between restarts — only the 5-digit code is random.

**Buttons.** Only round N's button is live during round N. The other four are ignored,
so a stray press cannot skip ahead.

**Potentiometer.** One pot, shared. It always drives the current round's digit, and the
digit it is showing when the button is pressed is the one that counts.

**Buzzer.** Beeps on every RFID scan, plays a distinct tune per button, and plays a win
or fail tune at the end.
