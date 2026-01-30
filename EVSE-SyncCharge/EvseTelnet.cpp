/* =========================================================================================
 * Project:     Evse-SyncCharge
 * Description: Implementation of the Telnet Server. Provides remote log streaming
 *              with authentication and session management.
 *
 * Author:      Noel Vellemans
 * Copyright:   (C) 2026 Noel Vellemans
 * License:     GNU General Public License v2.0 (GPLv2)
 * =========================================================================================
 */

#include "EvseTelnet.h"
#include "EvseLogger.h"

EvseTelnet::EvseTelnet() {
}

void EvseTelnet::begin(AppConfig& config) {
    _appConfig = &config;
    
    Preferences prefs;
    prefs.begin("evse_telnet", true);
    _enabled = prefs.getBool("en", false);
    _port = prefs.getUShort("port", 23);
    prefs.end();

    if (_enabled) {
        _server = new WiFiServer(_port);
        _server->begin();
        _server->setNoDelay(true);
        logger.infof("[TELNET] Server started on port %d", _port);
    }
}

void EvseTelnet::updateConfig(bool enabled, uint16_t port) {
    if (enabled == _enabled && port == _port) return;

    // Stop existing server first
    stop();
    
    _enabled = enabled;
    _port = port;

    Preferences prefs;
    prefs.begin("evse_telnet", false);
    prefs.putBool("en", _enabled);
    prefs.putUShort("port", _port);
    prefs.end();

    if (_enabled) {
        _server = new WiFiServer(_port);
        _server->begin();
        _server->setNoDelay(true);
        logger.infof("[TELNET] Config updated. Restarted on port %d", _port);
    } else {
        logger.info("[TELNET] Service disabled.");
    }
}

void EvseTelnet::stop() {
    if (_client) _client.stop();
    if (_server) {
        delete _server;
        _server = nullptr;
    }
    resetClientState();
}

void EvseTelnet::resetClientState() {
    _authState = AUTH_USER;
    _inputBuffer = "";
    _connectTime = 0;
    _loginAttempts = 0;
    _iacState = 0;
}

void EvseTelnet::disconnectClient(const char* reason) {
    if (_client && _client.connected()) {
        _client.printf("\r\n[TELNET] %s\r\n", reason);
        _client.stop();
    }
    resetClientState();
}

void EvseTelnet::loop() {
    if (!_enabled || !_server) return;

    // Handle new connections
    if (_server->hasClient()) {
        handleNewClient();
    }

    // Check for authentication timeout
    if (_client && _client.connected() && _authState != AUTH_LOGGED_IN) {
        if (millis() - _connectTime > TELNET_AUTH_TIMEOUT_MS) {
            disconnectClient("Authentication timeout. Goodbye.");
            logger.warn("[TELNET] Client disconnected: Auth timeout");
            return;
        }
    }

    // Handle existing client data
    if (_client && _client.connected()) {
        handleClientInput();
    } else if (_connectTime > 0) {
        // Client disconnected unexpectedly
        resetClientState();
    }
}

void EvseTelnet::sendTelnetNegotiation() {
    // Tell client: we WILL handle echo (so client should NOT echo locally)
    uint8_t willEcho[] = { TELNET_IAC, TELNET_WILL, TELNET_ECHO };
    _client.write(willEcho, 3);
    
    // Tell client: we WILL suppress go-ahead
    uint8_t willSga[] = { TELNET_IAC, TELNET_WILL, TELNET_SGA };
    _client.write(willSga, 3);
    
    // Tell client: please DO suppress go-ahead
    uint8_t doSga[] = { TELNET_IAC, TELNET_DO, TELNET_SGA };
    _client.write(doSga, 3);
}

void EvseTelnet::handleNewClient() {
    WiFiClient newClient = _server->accept();
    if (!newClient) return;

    // Only one client at a time - kick the old one
    if (_client && _client.connected()) {
        _client.println("\r\n[TELNET] New session connected. Bye.");
        _client.stop();
    }

    _client = newClient;
    _client.setNoDelay(true);
    resetClientState();
    _connectTime = millis();
    _iacState = 0;
    
    logger.infof("[TELNET] Client connected from %s", _client.remoteIP().toString().c_str());
    
    // Send Telnet negotiation to disable client local echo
    sendTelnetNegotiation();
    
    _client.println("=========================================");
    _client.println(" EVSE-SyncCharge Remote Console");
    _client.println("=========================================");
    _client.print("Login: ");
}

void EvseTelnet::handleClientInput() {
    while (_client.available()) {
        uint8_t c = _client.read();
        
        // Handle Telnet IAC (Interpret As Command) sequences
        // IAC sequences are 2-3 bytes: IAC cmd [option]
        if (_iacState == 1) {
            // Got IAC, now check command
            if (c == TELNET_WILL || c == TELNET_WONT || c == TELNET_DO || c == TELNET_DONT) {
                _iacState = 2;  // Wait for option byte
            } else {
                _iacState = 0;  // Unknown/other IAC sequence, reset
            }
            continue;
        } else if (_iacState == 2) {
            // Got IAC + cmd, this is the option byte - ignore and reset
            _iacState = 0;
            continue;
        } else if (c == TELNET_IAC) {
            _iacState = 1;  // Start of IAC sequence
            continue;
        }
        
        // Normal character processing
        if (c == '\n' || c == '\r') {
            if (_inputBuffer.length() > 0) {
                processCommand(_inputBuffer);
                _inputBuffer = "";
            }
        } else if (c == 0x08 || c == 0x7F) {  // Backspace / Delete
            if (_inputBuffer.length() > 0) {
                _inputBuffer.remove(_inputBuffer.length() - 1);
                // Echo backspace only in username mode (password is hidden)
                if (_authState == AUTH_USER) {
                    _client.print("\b \b");
                } else if (_authState == AUTH_PASS) {
                    _client.print("\b \b");  // Also handle backspace for password
                }
            }
        } else if (c >= 0x20 && c < 0x7F) {  // Printable ASCII only
            if (_inputBuffer.length() < 64) {
                _inputBuffer += (char)c;
                // Echo character only for username, mask password with *
                if (_authState == AUTH_USER) {
                    _client.print((char)c);
                } else if (_authState == AUTH_PASS) {
                    _client.print('*');
                }
            }
        }
        // Ignore control characters and non-ASCII
    }
}

void EvseTelnet::processCommand(const String& input) {
    String trimmed = input;
    trimmed.trim();
    
    switch (_authState) {
        case AUTH_USER:
            if (trimmed == _appConfig->wwwUser) {
                _authState = AUTH_PASS;
                _client.print("\r\nPassword: ");
            } else {
                _loginAttempts++;
                if (_loginAttempts >= TELNET_MAX_LOGIN_ATTEMPTS) {
                    disconnectClient("Too many failed attempts. Goodbye.");
                    logger.warn("[TELNET] Client kicked: Too many login failures");
                } else {
                    _client.println("\r\nInvalid username.");
                    _client.print("Login: ");
                }
            }
            break;
            
        case AUTH_PASS:
            if (trimmed == _appConfig->wwwPass) {
                _authState = AUTH_LOGGED_IN;
                _client.println("\r\n");
                _client.println("[TELNET] Authenticated successfully!");
                _client.println("[TELNET] Streaming logs... (Ctrl+] to disconnect)");
                _client.println("-----------------------------------------");
                logger.info("[TELNET] Client authenticated");
            } else {
                _loginAttempts++;
                if (_loginAttempts >= TELNET_MAX_LOGIN_ATTEMPTS) {
                    disconnectClient("Too many failed attempts. Goodbye.");
                    logger.warn("[TELNET] Client kicked: Too many login failures");
                } else {
                    _client.println("\r\nInvalid password.");
                    _authState = AUTH_USER;
                    _client.print("Login: ");
                }
            }
            break;
            
        case AUTH_LOGGED_IN:
            // Could add commands here in future (e.g., "help", "status", "quit")
            break;
    }
}

size_t EvseTelnet::write(uint8_t c) {
    if (_enabled && _client.connected() && _authState == AUTH_LOGGED_IN) {
        return _client.write(c);
    }
    return 1;  // Return 1 to avoid Print class retrying
}

size_t EvseTelnet::write(const uint8_t *buffer, size_t size) {
    if (_enabled && _client.connected() && _authState == AUTH_LOGGED_IN) {
        return _client.write(buffer, size);
    }
    return size;  // Return size to avoid Print class retrying
}