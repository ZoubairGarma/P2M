#ifndef RFID_SCANNER_H
#define RFID_SCANNER_H

#include <Arduino.h>

void setupRFID();
String scanCard();
bool isAuthorized(String scannedUID); // Our new security check

#endif