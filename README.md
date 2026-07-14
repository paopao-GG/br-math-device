# BR Math Device

Arduino Mega firmware for a 5-round multiple-choice quiz prop. Questions are on
paper; the device judges the answers. See [TDD.md](TDD.md) for the original design.

## How it plays

On power-up the device picks a random 5-digit code (digits 1–9, different every
boot) and shows it on the LCD:

```
CODE:47391 Q3:C     <- the code, the current round, the card currently scanned
1:42:73:34:-5:-     <- the five digit slots: locked, live, then not yet reached
```

In round N the player:

1. scans one of the four RFID cards (A/B/C/D) as their answer — the buzzer beeps to
   confirm the read,
2. dials the potentiometer to digit N of the code,
3. presses button N to lock in.

**LED N lights only if both the card and the digit are correct.** A wrong card, a
wrong digit, or no card at all scores the round wrong and the game moves on — there
are no retries. Each button has its own tune.

After round 5 the LCD shows `SCORE: n/5`, the buzzer plays a win or fail tune, and
the LEDs stay lit showing which rounds were right. Power-cycle to play again with a
new code.

The answer key is fixed at **C, D, A, B, C** (`ANSWER_KEY` in [config.h](config.h)).

## Wiring (Arduino Mega 2560)

Summary below. **[WIRING.md](WIRING.md) has the wire-by-wire tables** — use that one at
the bench.

| Part | Pins |
|---|---|
| RC522 RFID | SS→53, SCK→52, MOSI→51, MISO→50, RST→49, **VCC→3.3V**, GND→GND |
| 1602 I2C LCD | SDA→20, SCL→21, VCC→5V, GND→GND |
| Potentiometer ×1 | wiper → A0; outer legs → 5V and GND |
| LEDs ×5 | 22, 24, 26, 28, 30 → 220Ω resistor → LED → GND |
| Buttons ×5 | 23, 25, 27, 29, 31 → button → GND (no resistors; internal pullups) |
| Passive buzzer | 8 → buzzer → GND |
| A15 | **leave unconnected** |

Two things bite people here:

- **The RC522 runs on 3.3V.** Putting it on 5V destroys it. Its logic pins are fine
  on the Mega's 5V SPI lines; it's only VCC that matters.
- **A15 must stay floating.** Its electrical noise is what seeds the random code.
  Wire anything to it and every boot produces the same code.

LED 1 goes with button 1 and round 1, and so on down the line. The single potentiometer is
shared by every round.

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
every round against the wrong card. If a card prints nothing at all, the reader is
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
- Turning the pot must only ever change the **current** round's digit. If it also moves a
  digit you already locked in, the latch is broken.
- The one test worth doing deliberately: play a round with the **right card but a
  wrong digit**. The LED must stay off. That proves the both-must-match rule is live.
- The Serial Monitor narrates the whole game — the code, each round's expected answer,
  what was scanned, and why a round scored the way it did.

## Layout

- `br-math-device.ino` — the game
- `config.h` — pins, answer key, card UIDs, tunes, tuning constants. Everything you'd
  want to change is in here.
- `WIRING.md` — wire-by-wire connection tables
- `TDD.md` — the design doc: hardware, game flow, rules
- `tools/uid_dump/` — the card UID reader
- `platformio.ini` — for building from the CLI; the Arduino IDE works without it
