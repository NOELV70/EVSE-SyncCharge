/* =========================================================================================
 * Project:     Evse-SyncCharge
 * Description: Implementation of WebSocketClientAdapter.
 * =========================================================================================
 */

#include "WebSocketClientAdapter.h"

WebSocketClientAdapter::WebSocketClientAdapter() {
    _ws.onEvent([this](WStype_t type, uint8_t * payload, size_t length) {
        switch(type) {
            case WStype_DISCONNECTED:
                _connected = false;
                break;
            case WStype_CONNECTED:
                _connected = true;
                break;
            case WStype_BIN:
            case WStype_TEXT:
                // Buffer incoming data for Client::read()
                for(size_t i=0; i<length; i++) {
                    _rxBuffer.push_back(payload[i]);
                }
                break;
            default: break;
        }
    });
}

void WebSocketClientAdapter::begin(const char* host, uint16_t port, const char* url, bool useTls) {
    if (useTls) {
        _ws.beginSSL(host, port, url, "", "mqtt");
    } else {
        _ws.begin(host, port, url, "mqtt");
    }
    _ws.setReconnectInterval(5000);
}

void WebSocketClientAdapter::loop() {
    _ws.loop();
}

int WebSocketClientAdapter::connect(IPAddress ip, uint16_t port) {
    return 0; // Not used by PubSubClient when Client is injected
}

int WebSocketClientAdapter::connect(const char *host, uint16_t port) {
    return 0; // Not used by PubSubClient when Client is injected
}

size_t WebSocketClientAdapter::write(uint8_t data) {
    return write(&data, 1);
}

size_t WebSocketClientAdapter::write(const uint8_t *buf, size_t size) {
    if (_connected) {
        _ws.sendBIN((uint8_t*)buf, size);
        return size;
    }
    return 0;
}

int WebSocketClientAdapter::available() {
    return _rxBuffer.size();
}

int WebSocketClientAdapter::read() {
    if (_rxBuffer.empty()) return -1;
    int c = _rxBuffer.front();
    _rxBuffer.erase(_rxBuffer.begin());
    return c;
}

int WebSocketClientAdapter::read(uint8_t *buf, size_t size) {
    size_t avail = _rxBuffer.size();
    if (avail == 0) return -1;
    
    size_t toRead = (size < avail) ? size : avail;
    for (size_t i = 0; i < toRead; i++) {
        buf[i] = _rxBuffer[i];
    }
    _rxBuffer.erase(_rxBuffer.begin(), _rxBuffer.begin() + toRead);
    return toRead;
}

int WebSocketClientAdapter::peek() {
    if (_rxBuffer.empty()) return -1;
    return _rxBuffer.front();
}

void WebSocketClientAdapter::flush() {}

void WebSocketClientAdapter::stop() {
    _ws.disconnect();
}

uint8_t WebSocketClientAdapter::connected() {
    return _connected;
}

WebSocketClientAdapter::operator bool() {
    return _connected;
}