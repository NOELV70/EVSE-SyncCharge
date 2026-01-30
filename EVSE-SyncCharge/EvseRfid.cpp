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
#include <ArduinoJson.h>

EvseRfid::EvseRfid() 
    : _ssPin(0), _rstPin(0), _buzzerPin(0), _mfrc522(nullptr), _lastScanTime(0) 
{
}

void EvseRfid::begin(int ssPin, int rstPin, int buzzerPin) {
    _ssPin = ssPin;
    _rstPin = rstPin;
    _buzzerPin = buzzerPin;

    if (_mfrc522) delete _mfrc522;
    _mfrc522 = new MFRC522(_ssPin, _rstPin);

    pinMode(_buzzerPin, OUTPUT);
    digitalWrite(_buzzerPin, LOW);
    SPI.begin();        // Initialize SPI bus
    _mfrc522->PCD_Init(); // Initialize MFRC522 card
    
    // Optional: Log version to verify hardware connection
    byte v = _mfrc522->PCD_ReadRegister(MFRC522::VersionReg);
    logger.infof("[RFID] Initialized (MFRC522 Version: 0x%02X)", v);
    if (v == 0x00 || v == 0xFF) {
        logger.warn("[RFID] Warning: Communication failure, check wiring!");
    }
    
    _prefs.begin("evse-rfid", false);
    _enabled = _prefs.getBool("enabled", false);
    loadTags();
    logger.infof("[RFID] Loaded %d tags from NVS.", _tags.size());
}

void EvseRfid::setEnabled(bool enabled) {
    _enabled = enabled;
    _prefs.putBool("enabled", enabled);
    logger.infof("[RFID] System set to %s", enabled ? "ENABLED" : "DISABLED");
}

bool EvseRfid::isEnabled() { return _enabled; }

void EvseRfid::setBuzzerEnabled(bool enabled) {
    _buzzerEnabled = enabled;
}

void EvseRfid::startLearning() {
    _learning = true;
    _lastScannedUid = "";
    logger.info("[RFID] Learning mode started... Waiting for card.");
}

bool EvseRfid::isLearning() { return _learning; }

String EvseRfid::getLastScannedUid() { return _lastScannedUid; }

void EvseRfid::clearLastScannedUid() { _lastScannedUid = ""; }

bool EvseRfid::addTag(String uid, String name) {
    uid.toUpperCase();
    // Update existing tag
    for (auto& t : _tags) {
        if (t.uid == uid) {
            t.name = name; // Update name
            saveTags();
            logger.infof("[RFID] Updated tag: %s (%s)", uid.c_str(), name.c_str());
            return true;
        }
    }
    // Check if max tags reached
    if (_tags.size() >= MAX_RFID_TAGS) {
        logger.warnf("[RFID] Cannot add new tag. List is full (Max: %d)", MAX_RFID_TAGS);
        return false;
    }
    // Add new tag
    _tags.push_back({uid, name, true});
    saveTags();
    logger.infof("[RFID] Added tag: %s (%s)", uid.c_str(), name.c_str());
    return true;
}

void EvseRfid::toggleTagStatus(String uid) {
    uid.toUpperCase();
    for (auto& t : _tags) {
        if (t.uid == uid) {
            t.active = !t.active;
            saveTags();
            logger.infof("[RFID] Tag %s status set to: %s", uid.c_str(), t.active ? "ACTIVE" : "INACTIVE");
            return;
        }
    }
}

void EvseRfid::deleteTag(String uid) {
    uid.toUpperCase();
    for (auto it = _tags.begin(); it != _tags.end(); ++it) {
        if (it->uid == uid) {
            _tags.erase(it);
            saveTags();
            logger.infof("[RFID] Removed tag: %s", uid.c_str());
            return;
        }
    }
}

void EvseRfid::clearAllowedUids() {
    _tags.clear();
    saveTags();
    logger.info("[RFID] Cleared all authorized UIDs");
}

std::vector<RfidTag> EvseRfid::getTags() { return _tags; }

void EvseRfid::saveTags() {
    JsonDocument doc;
    JsonArray arr = doc.to<JsonArray>();
    for (const auto& t : _tags) {
        JsonObject obj = arr.add<JsonObject>();
        obj["u"] = t.uid;
        obj["n"] = t.name;
        obj["a"] = t.active;
    }
    String out; serializeJson(doc, out);
    if (_prefs.putString("tags", out) == 0) {
        logger.error("[RFID] Failed to save tags to NVS!");
    } else {
        logger.debugf("[RFID] Saved %d tags to NVS", _tags.size());
    }
}

void EvseRfid::loadTags() {
    _tags.clear();
    String data = _prefs.getString("tags", "[]");
    JsonDocument doc; deserializeJson(doc, data);
    JsonArray arr = doc.as<JsonArray>();
    for (JsonObject obj : arr) {
        // Default to true for 'active' if the field is missing (for backward compatibility)
        _tags.push_back({obj["u"].as<String>(), obj["n"].as<String>(), obj["a"] | true});
    }
}

bool EvseRfid::isUidAllowed(String uid) {
    uid.toUpperCase();
    for (const auto& t : _tags) {
        // Only allow if the tag is found AND it's active
        if (t.uid == uid) return t.active;
    }
    return false;
}

void EvseRfid::onCardScanned(RfidCallback callback) {
    _callback = callback;
}

void EvseRfid::loop() {
    // Handle Buzzer (Non-blocking)
    if (_beeping && millis() - _buzzerStartTime > _buzzerDuration) {
        digitalWrite(_buzzerPin, LOW);
        _beeping = false;
    }

    // Debounce: Limit scans to once every 1.5 seconds to prevent flooding
    // Bypass debounce if in learning mode to ensure we catch the tag immediately
    if (!_learning && (millis() - _lastScanTime < 1500)) return;

    // 1. Look for new cards
    if (!_mfrc522 || !_mfrc522->PICC_IsNewCardPresent()) return;

    // 2. Select one of the cards
    if (!_mfrc522->PICC_ReadCardSerial()) {
        if (_learning) logger.warn("[RFID] Learn Mode: Card detected but Read failed");
        return;
    }

    // 3. Process UID
    String uid = uidToHexString(_mfrc522->uid.uidByte, _mfrc522->uid.size);
    _lastScanTime = millis();

    if (_learning) {
        logger.infof("[RFID] LEARN MODE DETECTED UID: %s", uid.c_str());
        _lastScannedUid = uid;
        _learning = false;
        
        // Feedback for learning
        if (_buzzerEnabled) {
            digitalWrite(_buzzerPin, HIGH);
            _buzzerStartTime = millis();
            _buzzerDuration = 600; // Distinct long beep
            _beeping = true;
        }

        _mfrc522->PICC_HaltA();
        _mfrc522->PCD_StopCrypto1();
        return;
    }

    if (!_enabled) {
        _mfrc522->PICC_HaltA();
        _mfrc522->PCD_StopCrypto1();
        return;
    }

    bool authorized = isUidAllowed(uid);
    
    if (_buzzerEnabled) {
        digitalWrite(_buzzerPin, HIGH);
        _buzzerStartTime = millis();
        _buzzerDuration = authorized ? 200 : 1000; // Short for success, long for failure
        _beeping = true;
    }
    
    logger.infof("[RFID] Card Scanned: %s | Authorized: %s", uid.c_str(), authorized ? "YES" : "NO");

    if (_callback) {
        _callback(uid, authorized);
    }

    // 4. Halt PICC (Stop communicating with card)
    _mfrc522->PICC_HaltA();
    // 5. Stop encryption on PCD
    _mfrc522->PCD_StopCrypto1();
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

// Global Instance
EvseRfid rfid;