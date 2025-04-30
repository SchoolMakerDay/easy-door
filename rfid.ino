#include <SPI.h>
#include <MFRC522.h>

#define SS_PIN 10
#define RST_PIN 9

MFRC522 mfrc522(SS_PIN, RST_PIN);  

void setup() {
  Serial.begin(9600);
  SPI.begin();          
  mfrc522.PCD_Init();  
  Serial.println("Avvicina un badge RFID...");
}

void loop() {

  if (!mfrc522.PICC_IsNewCardPresent() || !mfrc522.PICC_ReadCardSerial()) {
    return;
  }


  String uidString = "";
  for (byte i = 0; i < mfrc522.uid.size; i++) {
    if (mfrc522.uid.uidByte[i] < 0x10) uidString += "0"; 
    uidString += String(mfrc522.uid.uidByte[i], HEX);
  }

  uidString.toUpperCase(); 

  Serial.print("Codice badge: ");
  Serial.println(uidString);


  mfrc522.PICC_HaltA();
  mfrc522.PCD_StopCrypto1();
}
