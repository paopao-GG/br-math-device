# BR Math Device

Arduino Mega firmware for a 3-level multiple-choice quiz prop. Questions are on
paper; the device judges the answers. See [TDD.md](TDD.md) for the full design.

## How it plays

Three levels hold nine questions between them. One LED and one button belong to each
**level**, not to each question:

| Level | Questions | Code digits it locks | Button | LED |
|---|---|---|---|---|
| 1 | 5 | 1, 2, 3 | 23 | 22 |
| 2 | 3 | 4 | 25 | 24 |
| 3 | 1 | 5 | 27 | 26 |

On power-up the device picks a random 5-digit code (digits 1–9, different every
boot) and shows it on the LCD. Each level then runs in two phases:

**Quiz phase** — scan one of the four RFID cards (A/B/C/D) as the answer, press the
level's button.

```
CODE:47391 L1Q3     <- the code, then level 1 question 3
ANS:C   [##...]     <- the scanned card, then this level's 5 questions
```

- correct → the LED **blinks twice** and the next question opens
- wrong, or no card scanned → **no blink**, miss tune, and the *same question stays
  live*. There is no way past a question except by answering it.

**Code phase** — once every question in the level is cleared, dial the level's share
of the code on the potentiometer and press the same button to lock each digit.

```
CODE:47391 L1D2     <- level 1, code digit 2 of the 3 it owns
DIAL:7  [47_..]     <- the live pot digit, then all five code slots
```

- correct digit → LED blinks once, digit latches, next slot opens
- wrong digit → no blink, miss tune, retry the same slot
- the level's **last** digit → LED goes **solid** and the next level begins

After level 3's digit locks, all three LEDs are solid and the LCD shows `SOLVED!`
with the code and a `MISSES:n` count. Because a question can only be retried, every
game ends solved — the miss count is what says how cleanly. Power-cycle for a new code.

The answer key is fixed (`ANSWER_KEY` in [config.h](config.h)):

| Level | Q1 | Q2 | Q3 | Q4 | Q5 |
|---|---|---|---|---|---|
| **1** | **B** | **D** | **A** | **C** | **D** |
| **2** | **A** | **C** | **B** | — | — |
| **3** | **D** | — | — | — | — |

## Wiring (Arduino Mega 2560)

Summary below. **[WIRING.md](WIRING.md) has the wire-by-wire tables** — use that one at
the bench.

| Part | Pins |
|---|---|
| RC522 RFID | SS→53, SCK→52, MOSI→51, MISO→50, RST→49, **VCC→3.3V**, GND→GND |
| 1602 I2C LCD | SDA→20, SCL→21, VCC→5V, GND→GND |
| Potentiometer ×1 | wiper → A0; outer legs → 5V and GND |
| LEDs ×3 | 22, 24, 26 → 220Ω resistor → LED → GND |
| Buttons ×3 | 23, 25, 27 → button → GND (no resistors; internal pullups) |
| Passive buzzer | 8 → buzzer → GND |
| A15 | **leave unconnected** |

Two things bite people here:

- **The RC522 runs on 3.3V.** Putting it on 5V destroys it. Its logic pins are fine
  on the Mega's 5V SPI lines; it's only VCC that matters.
- **A15 must stay floating.** Its electrical noise is what seeds the random code.
  Wire anything to it and every boot produces the same code.

LED 1 goes with button 1 and level 1, and so on. The single potentiometer is shared by
every level.

LEDs on 28/30 and buttons on 29/31 are left over from the earlier five-round build. They
stay wired and are never read or driven — the firmware doesn't even configure those pins.

## Libraries

Two are needed, both via the Arduino IDE's **Library Manager**:

- **MFRC522** (by GithubCommunity) — for the RFID reader. Not yet installed.
- **LiquidCrystal I2C** — already installed here (Frank de Brabander's version).

Several different libraries are all called "LiquidCrystal_I2C" and they disagree on
how to start the display — de Brabander's uses `begin()`, the others use `init()`.
The sketch detects which one you have at compile time and calls the right one, so any
of them will work. You don't need to do anything about this; it's noted only because
the resulting error message (`no member named 'begin'`) is otherwise baffling.

## Bring-up

**1. Read the card UIDs.** The firmware can't recognise your cards until it knows
their UIDs, and those are burned into each card at the factory. Open
`tools/uid_dump/uid_dump.ino` in the Arduino IDE, upload it, open the Serial Monitor
at 9600 baud, and tap each card. It prints a paste-ready line per card:

```
Card detected!
  UID (hex):  DE AD BE EF
  Paste this: {0xDE, 0xAD, 0xBE, 0xEF},
```

**2. Paste them into `CARD_UIDS` in [config.h](config.h), in the order A, B, C, D.**
The order matters — the answer key indexes into that table, so a wrong order scores
every question against the wrong card. If a card prints nothing at all, the reader is
miswired or on 5V.

**3. Flash the game.** Open `br-math-device.ino` in the Arduino IDE and upload.

(If you prefer the command line, PlatformIO works too: `pio run -t upload` for the
game, `pio run -d tools/uid_dump -t upload` for the UID reader.)

## Checks

- Blank or garbled LCD: run an I2C scanner and change `LCD_ADDR` in config.h to `0x3F`.
- Scan beep too quiet to hear across the room: raise the duration in `TUNE_SCAN`
  (config.h). It beeps on every card read, including cards that aren't one of the four.
- Digit flickering between two values as the pot rests: raise `POT_HYSTERESIS`. Pot
  feeling sticky or refusing to reach 1 or 9: lower it.
- Turning the pot must only ever change the digit in the `_` slot. If it also moves a
  digit you already locked in, the latch is broken.
- The pot is deliberately dead during a quiz phase — `DIAL:` only appears once all of a
  level's questions are answered. That's not a fault.
- The one test worth doing deliberately: answer a question with the **wrong card**. The
  LED must not blink and the LCD must still show the same `LxQy`. That proves the
  retry rule is live — a question can't be skipped, only answered.
- The Serial Monitor narrates the whole game — the code, each question's expected card,
  each slot's expected digit, what was scanned or dialed, and every miss.

## Layout

- `br-math-device.ino` — the game
- `config.h` — pins, answer key, card UIDs, tunes, tuning constants. Everything you'd
  want to change is in here.
- `WIRING.md` — wire-by-wire connection tables
- `TDD.md` — the design doc: hardware, game flow, rules
- `tools/uid_dump/` — the card UID reader
- `platformio.ini` — for building from the CLI; the Arduino IDE works without it
