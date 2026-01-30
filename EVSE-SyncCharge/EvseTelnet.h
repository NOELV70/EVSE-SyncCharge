/* =========================================================================================
 * Project:     Evse-SyncCharge
 * Description: Header for the Telnet Server. Provides remote logging capabilities
 *              over a raw TCP connection with authentication.
 * =========================================================================================
 */

#ifndef EVSE_TELNET_H
#define EVSE_TELNET_H

#include <WiFi.h>
#include <Preferences.h>
#include "EvseConfig.h"

// Telnet configuration
constexpr unsigned long TELNET_AUTH_TIMEOUT_MS = 30000;  // 30s to authenticate
constexpr int TELNET_MAX_LOGIN_ATTEMPTS = 3;

// Telnet protocol constants
constexpr uint8_t TELNET_IAC  = 255;  // Interpret As Command
constexpr uint8_t TELNET_WILL = 251;
constexpr uint8_t TELNET_WONT = 252;
constexpr uint8_t TELNET_DO   = 253;
constexpr uint8_t TELNET_DONT = 254;
constexpr uint8_t TELNET_ECHO = 1;
constexpr uint8_t TELNET_SGA  = 3;    // Suppress Go Ahead

class EvseTelnet : public Print {
public:
    EvseTelnet();
    void begin(AppConfig& config);
    void loop();
    void stop();
    
    // Configuration
    void updateConfig(bool enabled, uint16_t port);
    bool isEnabled() const { return _enabled; }
    uint16_t getPort() const { return _port; }
    bool isClientConnected() { return _client && _client.connected(); }

    // Print interface implementation for Logger
    virtual size_t write(uint8_t c) override;
    virtual size_t write(const uint8_t *buffer, size_t size) override;

private:
    WiFiServer* _server = nullptr;
    WiFiClient _client;
    AppConfig* _appConfig = nullptr;
    
    bool _enabled = false;
    uint16_t _port = 23;
    
    enum AuthState {
        AUTH_USER,
        AUTH_PASS,
        AUTH_LOGGED_IN
    } _authState = AUTH_USER;

    String _inputBuffer;
    unsigned long _connectTime = 0;
    int _loginAttempts = 0;
    uint8_t _iacState = 0;  // 0=normal, 1=got IAC, 2=got IAC+cmd

    void handleNewClient();
    void sendTelnetNegotiation();
    void handleClientInput();
    void processCommand(const String& input);
    void resetClientState();
    void disconnectClient(const char* reason);
};

#endif // EVSE_TELNET_H