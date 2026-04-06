#include "rfid_scanner.h"
#include <SPI.h>
#include <MFRC522.h>

#define SS_PIN  5 
#define RST_PIN 22

MFRC522 rfid(SS_PIN, RST_PIN);

// --- AUTHORIZED USERS DATABASE ---
// Add the UIDs you want to grant access to here.
// Make sure to copy them EXACTLY as they appeared in the Serial Monitor!
const String AUTHORIZED_CARDS[] = {
  " 45 43 0e 30", // Replace with your card's UID
  " e3 75 b7 00"  // Replace with your keychain's UID
};
const int NUM_AUTHORIZED_CARDS = 2; // Keep this updated if you add more cards

void setupRFID() {
  SPI.begin(18, 19, 23, 5);      
  rfid.PCD_Init();  
}

String scanCard() {
  if (!rfid.PICC_IsNewCardPresent() || !rfid.PICC_ReadCardSerial()) {
    return ""; 
  }

  String uidString = "";
  for (byte i = 0; i < rfid.uid.size; i++) {
    uidString += String(rfid.uid.uidByte[i] < 0x10 ? " 0" : " ");
    uidString += String(rfid.uid.uidByte[i], HEX);
  }
  
  rfid.PICC_HaltA(); 
  return uidString;
}

// --- NEW SECURITY FUNCTION ---
bool isAuthorized(String scannedUID) {
  for (int i = 0; i < NUM_AUTHORIZED_CARDS; i++) {
    // .equalsIgnoreCase ensures it doesn't fail if letters are upper/lowercase
    if (scannedUID.equalsIgnoreCase(AUTHORIZED_CARDS[i])) {
      return true; // Match found!
    }
  }
  return false; // No match found in the database
}