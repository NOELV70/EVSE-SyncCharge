/* =========================================================================================
 * Project:     Evse-SyncCharge
 * Description: Implementation of the EvseRfid class. Handles communication with the
 *              MFRC522 RFID reader for user authentication and access control.
 *
 * Author:      Noel Vellemans
 * Copyright:   (C) 2026 Noel Vellemans
 * License:     GNU General Public License v2.0 (GPLv2)
 * =========================================================================================
 */

#include "EvseRfid.h"
#include "EvseLogger.h"

EvseRfid::EvseRfid(int ssPin, int rstPin) 
    : _ssPin(ssPin), _rstPin(rstPin), _mfrc522(ssPin, rstPin), _lastScanTime(0) 
{
}

void EvseRfid::begin() {
    SPI.begin();        // Initialize SPI bus
    _mfrc522.PCD_Init(); // Initialize MFRC522 card
    
    // Optional: Log version to verify hardware connection
    byte v = _mfrc522.PCD_ReadRegister(MFRC522::VersionReg);
    logger.infof("[RFID] Initialized (MFRC522 Version: 0x%02X)", v);
    if (v == 0x00 || v == 0xFF) {
        logger.warn("[RFID] Warning: Communication failure, check wiring!");
    }
}

void EvseRfid::addAllowedUid(String uid) {
    uid.toUpperCase();
    // Avoid duplicates
    for (const auto& id : _allowedUids) {
        if (id == uid) return;
    }
    _allowedUids.push_back(uid);
    logger.infof("[RFID] Added authorized UID: %s", uid.c_str());
}

void EvseRfid::removeAllowedUid(String uid) {
    uid.toUpperCase();
    for (auto it = _allowedUids.begin(); it != _allowedUids.end(); ++it) {
        if (*it == uid) {
            _allowedUids.erase(it);
            logger.infof("[RFID] Removed authorized UID: %s", uid.c_str());
            return;
        }
    }
}

void EvseRfid::clearAllowedUids() {
    _allowedUids.clear();
    logger.info("[RFID] Cleared all authorized UIDs");
}

bool EvseRfid::isUidAllowed(String uid) {
    uid.toUpperCase();
    for (const auto& id : _allowedUids) {
        if (id == uid) return true;
    }
    return false;
}

void EvseRfid::onCardScanned(RfidCallback callback) {
    _callback = callback;
}

void EvseRfid::loop() {
    // Debounce: Limit scans to once every 2 seconds to prevent flooding
    if (millis() - _lastScanTime < 2000) return;

    // 1. Look for new cards
    if (!_mfrc522.PICC_IsNewCardPresent()) return;

    // 2. Select one of the cards
    if (!_mfrc522.PICC_ReadCardSerial()) return;

    // 3. Process UID
    String uid = uidToHexString(_mfrc522.uid.uidByte, _mfrc522.uid.size);
    _lastScanTime = millis();

    bool authorized = isUidAllowed(uid);
    
    logger.infof("[RFID] Card Scanned: %s | Authorized: %s", uid.c_str(), authorized ? "YES" : "NO");

    if (_callback) {
        _callback(uid, authorized);
    }

    // 4. Halt PICC (Stop communicating with card)
    _mfrc522.PICC_HaltA();
    // 5. Stop encryption on PCD
    _mfrc522.PCD_StopCrypto1();
}

String EvseRfid::uidToHexString(byte *buffer, byte bufferSize) {
    String res = "";
    for (byte i = 0; i < bufferSize; i++) {
        if (buffer[i] < 0x10) res += "0";
        res += String(buffer[i], HEX);
    }
    res.toUpperCase();
    return res;
}