# libraries/

Archived copies of the exact third-party libraries this firmware was built and tested against:

| Folder              | Version  | Source |
|---------------------|----------|--------|
| `MFRC522`           | 1.4.12   | https://github.com/miguelbalboa/rfid |
| `LiquidCrystal_I2C` | –        | https://github.com/fdebrabander/Arduino-LiquidCrystal-I2C-library (header guard `FDB_LIQUID_CRYSTAL_I2C_H`) |

They are here as a record, **not** as a build input:

- **PlatformIO** still resolves both from `lib_deps` in `../platformio.ini`.
- **Arduino IDE** does not read a `libraries/` folder inside a sketch folder. Install both
  through the Library Manager (or drop them in your sketchbook's `libraries/`) instead.

The `LiquidCrystal_I2C` fork matters: several unrelated libraries share that name and disagree
on how to start the display. `setup()` in `../br-math-device.ino` keys off
`FDB_LIQUID_CRYSTAL_I2C_H` to call `begin()` for this one and `init()` for the others, so it
compiles either way — but this is the copy the hardware was verified with.
