# Wiring — BR Math Device (Arduino Mega 2560)

Every pin here matches [config.h](config.h). If you change a pin in one place, change it
in the other, or the device will misbehave in ways that look like broken hardware.

## Power

| Rail | Feeds |
|---|---|
| **3.3V** | RC522 RFID reader **only** |
| **5V** | LCD, potentiometer, buttons (via internal pullups), buzzer |
| **GND** | Everything — all grounds must be common |

The Mega has several GND pins; any of them will do, and you'll need a breadboard ground
rail since almost every component needs one.

## RC522 RFID reader

| RC522 pin | Mega pin |
|---|---|
| SDA / SS | **53** |
| SCK | **52** |
| MOSI | **51** |
| MISO | **50** |
| IRQ | *not connected* |
| GND | GND |
| RST | **49** |
| VCC | **3.3V** — see warning below |

SCK, MOSI and MISO are the Mega's hardware SPI pins and cannot be moved. SS and RST can
be changed in config.h if you need those pins for something else.

## 1602 I2C LCD

| LCD pin | Mega pin |
|---|---|
| GND | GND |
| VCC | 5V |
| SDA | **20** |
| SCL | **21** |

I2C address is `0x27` (`LCD_ADDR` in config.h). Some backpacks ship as `0x3F` — if the
screen stays blank or shows only blocks, run an I2C scanner and change it.

## Potentiometer ×1

There is a **single** potentiometer, shared by all five rounds — it drives whichever
round is current. Three wires:

| Pot leg | Mega pin |
|---|---|
| Outer leg 1 | 5V |
| Wiper (middle) | **A0** |
| Outer leg 2 | GND |

If it reads backwards (turning right lowers the digit), swap the two outer legs.

## LEDs ×5

Each LED: Mega pin → 220Ω resistor → LED **anode** (long leg); LED **cathode** (short
leg, flat side of the rim) → GND.

| LED | Mega pin |
|---|---|
| LED 1 | **22** |
| LED 2 | **24** |
| LED 3 | **26** |
| LED 4 | **28** |
| LED 5 | **30** |

## Buttons ×5

Each button: one leg → Mega pin, other leg → GND. **No resistors** — the firmware uses
the Mega's internal pullups, so a pressed button reads LOW.

| Button | Mega pin |
|---|---|
| Button 1 | **23** |
| Button 2 | **25** |
| Button 3 | **27** |
| Button 4 | **29** |
| Button 5 | **31** |

On a 4-leg tactile switch, the legs are paired internally. Use two legs that are
diagonally opposite each other and you can't get it wrong.

## Passive buzzer

| Buzzer pin | Mega pin |
|---|---|
| + (signal) | **8** |
| − | GND |

Must be a **passive** buzzer. An active buzzer only makes one fixed tone and the
per-button tunes won't work.

## Round mapping

Round N uses LED N and button N. The single potentiometer is shared by all five rounds,
so it has no column here. Wire each LED with its button as a set — swapping two rounds
fails silently, because the device still runs and just lights the wrong LED.

| Round | LED | Button |
|---|---|---|
| 1 | 22 | 23 |
| 2 | 24 | 25 |
| 3 | 26 | 27 |
| 4 | 28 | 29 |
| 5 | 30 | 31 |

## Full pin usage

| Mega pin | Used by |
|---|---|
| 8 | Buzzer |
| 20 (SDA) | LCD |
| 21 (SCL) | LCD |
| 22 | LED 1 |
| 23 | Button 1 |
| 24 | LED 2 |
| 25 | Button 2 |
| 26 | LED 3 |
| 27 | Button 3 |
| 28 | LED 4 |
| 29 | Button 4 |
| 30 | LED 5 |
| 31 | Button 5 |
| 49 | RC522 RST |
| 50 (MISO) | RC522 |
| 51 (MOSI) | RC522 |
| 52 (SCK) | RC522 |
| 53 (SS) | RC522 |
| A0 | Potentiometer wiper |
| A1–A4 | *free* |
| A15 | **Reserved — leave unconnected** |
| 3.3V | RC522 VCC |
| 5V | LCD, potentiometer |

Everything else on the board is free.

## Three things that will bite you

**The RC522 runs on 3.3V.** Putting its VCC on 5V destroys the module — this is the one
mistake here that costs you a part rather than an afternoon. Its data pins are fine
connected directly to the Mega's 5V SPI pins; it's only VCC that matters.

**A15 must stay unconnected.** The firmware reads the electrical noise on that floating
pin to seed the random number generator. Wire anything to it and the pin stops floating,
so every power-up generates the *same* 5-digit code.

**LEDs are polarised and need their resistor.** Long leg (anode) toward the 220Ω resistor
and the Mega pin, short leg (cathode) to GND. Backwards, it simply won't light; without
the resistor, you'll eventually kill the LED or the pin driving it.
