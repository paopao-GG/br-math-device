/*
 * uid_dump - RC522 card UID reader
 *
 * Flash this FIRST, before the main sketch. Open the Serial Monitor at 9600
 * baud and tap each of the four cards. Copy the "Paste this:" lines into
 * CARD_UIDS in config.h, in the order A, B, C, D.
 *
 * Wiring is identical to the main sketch:
 *   RC522 SDA/SS -> 53      RC522 3.3V -> 3.3V  (NOT 5V, that kills it)
 *   RC522 SCK    -> 52      RC522 GND  -> GND
 *   RC522 MOSI   -> 51
 *   RC522 MISO   -> 50
 *   RC522 RST    -> 49
 */

#include <SPI.h>
#include <MFRC522.h>

#define RFID_SS_PIN  53
#define RFID_RST_PIN 49

MFRC522 mfrc522(RFID_SS_PIN, RFID_RST_PIN);

void setup() {
  Serial.begin(9600);
  while (!Serial) { }

  SPI.begin();
  mfrc522.PCD_Init();
  delay(50);

  Serial.println(F("=== RC522 UID dump ==="));

  // A reader that is miswired or dead reads back 0x00 or 0xFF here.
  byte version = mfrc522.PCD_ReadRegister(MFRC522::VersionReg);
  Serial.print(F("Reader firmware version: 0x"));
  Serial.println(version, HEX);
  if (version == 0x00 || version == 0xFF) {
    Serial.println(F("WARNING: no reader detected. Check wiring and that VCC is on 3.3V."));
  }

  Serial.println(F("Tap a card..."));
  Serial.println();
}

void loop() {
  if (!mfrc522.PICC_IsNewCardPresent()) return;
  if (!mfrc522.PICC_ReadCardSerial()) return;

  byte size = mfrc522.uid.size;

  Serial.println(F("Card detected!"));

  Serial.print(F("  UID (hex):  "));
  for (byte i = 0; i < size; i++) {
    if (mfrc522.uid.uidByte[i] < 0x10) Serial.print('0');
    Serial.print(mfrc522.uid.uidByte[i], HEX);
    if (i < size - 1) Serial.print(' ');
  }
  Serial.println();

  Serial.print(F("  Paste this: {"));
  for (byte i = 0; i < size; i++) {
    Serial.print(F("0x"));
    if (mfrc522.uid.uidByte[i] < 0x10) Serial.print('0');
    Serial.print(mfrc522.uid.uidByte[i], HEX);
    if (i < size - 1) Serial.print(F(", "));
  }
  Serial.println(F("},"));

  if (size != 4) {
    Serial.print(F("  NOTE: this card has a "));
    Serial.print(size);
    Serial.println(F("-byte UID, not 4. UID_LENGTH in config.h must be widened to match."));
  }

  Serial.println();

  // Halt the card so the same one can be tapped again to confirm the UID is stable.
  mfrc522.PICC_HaltA();
  mfrc522.PCD_StopCrypto1();
}
