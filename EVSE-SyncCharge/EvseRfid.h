#ifndef EVSE_RFID_H
#define EVSE_RFID_H

#include <Arduino.h>
#include <SPI.h>
#include <MFRC522.h>
#include <vector>
#include <functional>

class EvseRfid {
public:
    // Constructor: Define Chip Select (SS/SDA) and Reset (RST) pins
    EvseRfid(int ssPin, int rstPin);

    void begin();
    void loop();

    // Management of authorized cards
    void addAllowedUid(String uid);
    void removeAllowedUid(String uid);
    void clearAllowedUids();
    bool isUidAllowed(String uid);
    
    // Callback: Function to call when a card is detected
    // Arguments: (String uid, bool isAuthorized)
    typedef std::function<void(String, bool)> RfidCallback;
    void onCardScanned(RfidCallback callback);

private:
    int _ssPin;
    int _rstPin;
    MFRC522 _mfrc522;
    std::vector<String> _allowedUids;
    RfidCallback _callback;
    unsigned long _lastScanTime;
    
    String uidToHexString(byte *buffer, byte bufferSize);
};

#endif