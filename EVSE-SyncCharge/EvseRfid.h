#ifndef EVSE_RFID_H
#define EVSE_RFID_H

#include <Arduino.h>
#include <SPI.h>
#include <MFRC522.h>
#include <vector>
#include <functional>
#include <Preferences.h>

struct RfidTag {
    String uid;
    String name;
    bool active;
};

const int MAX_RFID_TAGS = 10;

class EvseRfid {
public:
    // Constructor: Define Chip Select (SS/SDA) and Reset (RST) pins
    EvseRfid(int ssPin, int rstPin, int buzzerPin);

    void begin();
    void loop();

    // Management of authorized cards
    void setEnabled(bool enabled);
    bool isEnabled();
    void setBuzzerEnabled(bool enabled);
    
    void startLearning();
    bool isLearning();
    String getLastScannedUid();
    void clearLastScannedUid();

    bool addTag(String uid, String name);
    void toggleTagStatus(String uid);
    void deleteTag(String uid);
    void clearAllowedUids();
    bool isUidAllowed(String uid);
    std::vector<RfidTag> getTags();
    
    // Callback: Function to call when a card is detected
    // Arguments: (String uid, bool isAuthorized)
    typedef std::function<void(String, bool)> RfidCallback;
    void onCardScanned(RfidCallback callback);

private:
    int _ssPin;
    int _rstPin;
    int _buzzerPin;
    MFRC522 _mfrc522;
    std::vector<RfidTag> _tags;
    RfidCallback _callback;
    unsigned long _lastScanTime;
    
    Preferences _prefs;
    bool _enabled = false;
    bool _learning = false;
    bool _buzzerEnabled = true;
    bool _beeping = false;
    unsigned long _buzzerStartTime = 0;
    unsigned long _buzzerDuration = 0;
    String _lastScannedUid;

    void saveTags();
    void loadTags();
    
    String uidToHexString(byte *buffer, byte bufferSize);
};

#endif