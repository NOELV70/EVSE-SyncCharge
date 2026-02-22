/* =========================================================================================
 * Project:     Evse-SyncCharge
 * Description: Implementation of WebSocketClientAdapter.
 *              This class implements the Adapter design pattern to make the message-based
 *              WebSocketsClient library conform to the stream-based Arduino Client
 *              interface. This allows libraries like PubSubClient (MQTT) to be used
 *              over a WebSocket connection (e.g., for MQTTS over WebSockets).
 *
 * Author:      Noel Vellemans
 * Copyright:   (C) 2026 Noel Vellemans
 * License:     GNU General Public License v2.0 (GPLv2)
 * =========================================================================================
 */

#include "WebSocketClientAdapter.h"

WebSocketClientAdapter::WebSocketClientAdapter() {
    _ws.onEvent([this](WStype_t type, uint8_t * payload, size_t length) {
        // This lambda function is the core of the adapter. It listens for
        // WebSocket events and updates the adapter's state accordingly.
        switch(type) {
            case WStype_DISCONNECTED:
                // When the WebSocket disconnects, update our internal state.
                _connected = false;
                break;
            case WStype_CONNECTED:
                // When the WebSocket connects, update our internal state.
                _connected = true;
                break;
            case WStype_BIN:
            case WStype_TEXT:
                // When a data message (binary or text) arrives, append its
                // payload to our internal receive buffer (_rxBuffer). This is
                // how we convert message-based data into a stream.
                for(size_t i=0; i<length; i++) {
                    _rxBuffer.push_back(payload[i]);
                }
                break;
            default: 
                // Other events (like PING, PONG, ERROR) are ignored for this adapter.
                break;
        }
    });
}

void WebSocketClientAdapter::begin(const char* host, uint16_t port, const char* url, bool useTls) {
    // This method configures and starts the underlying WebSocket client.
    // It sets the subprotocol to "mqtt", which is required by many brokers.
    if (useTls) {
        _ws.beginSSL(host, port, url, "", "mqtt");
    } else {
        _ws.begin(host, port, url, "mqtt");
    }
    _ws.setReconnectInterval(5000);
}

void WebSocketClientAdapter::loop() {
    // This must be called repeatedly in the main sketch loop. It allows the
    // underlying WebSocketsClient to handle its network tasks and event processing.
    _ws.loop();
}

// =========================================================================
// Arduino Client Interface Implementation
// =========================================================================

int WebSocketClientAdapter::connect(IPAddress ip, uint16_t port) {
    // This method is part of the Client interface but is not used in our scenario.
    // PubSubClient, when given a client object via `setClient()`, expects the
    // connection to be managed externally. Our `begin()` and `loop()` handle it.
    return 0; 
}

int WebSocketClientAdapter::connect(const char *host, uint16_t port) {
    // Same as above, this is a required override but is not used.
    return 0; 
}

size_t WebSocketClientAdapter::write(uint8_t data) {
    // Convenience method to write a single byte.
    return write(&data, 1);
}

size_t WebSocketClientAdapter::write(const uint8_t *buf, size_t size) {
    // This is the primary write method. If the WebSocket is connected, it sends
    // the provided buffer as a single binary WebSocket frame.
    if (_connected) {
        _ws.sendBIN((uint8_t*)buf, size);
        return size;
    }
    // If not connected, report that zero bytes were written.
    return 0;
}

int WebSocketClientAdapter::available() {
    // Returns the number of bytes currently stored in our receive buffer.
    // This tells the calling library how much data it can read.
    return _rxBuffer.size();
}

int WebSocketClientAdapter::read() {
    // Reads a single byte from the front of the receive buffer.
    if (_rxBuffer.empty()) return -1;

    // Get the first byte.
    int c = _rxBuffer.front();
    // Remove the byte from the buffer.
    // Note: `erase(begin())` on a vector can be inefficient. For high-throughput
    // applications, a `std::deque` would be a better choice for `_rxBuffer`.
    _rxBuffer.erase(_rxBuffer.begin());
    return c;
}

int WebSocketClientAdapter::read(uint8_t *buf, size_t size) {
    // Reads a chunk of data from the receive buffer into a user-provided buffer.
    size_t avail = _rxBuffer.size();
    if (avail == 0) return -1;
    
    // Determine how many bytes to read: either the requested size or the
    // number of bytes available, whichever is smaller.
    size_t toRead = (size < avail) ? size : avail;

    // Copy the data from our internal buffer to the user's buffer.
    for (size_t i = 0; i < toRead; i++) {
        buf[i] = _rxBuffer[i];
    }

    // Remove the copied bytes from our internal buffer.
    _rxBuffer.erase(_rxBuffer.begin(), _rxBuffer.begin() + toRead);

    // Return the number of bytes actually read.
    return toRead;
}

int WebSocketClientAdapter::peek() {
    // Returns the next byte in the buffer without removing it.
    if (_rxBuffer.empty()) return -1;
    return _rxBuffer.front();
}

void WebSocketClientAdapter::flush() {
    // The Client interface requires a `flush()` method. In this context, there's
    // no transmit buffer to wait for, as `_ws.sendBIN()` sends data immediately.
    // So, this is a no-op.
}

void WebSocketClientAdapter::stop() {
    // Disconnects the WebSocket and clears the receive buffer.
    _ws.disconnect();
    _rxBuffer.clear();
    _connected = false;
}

uint8_t WebSocketClientAdapter::connected() {
    // Returns the connection status as a `uint8_t` (0 or 1), as per the
    // Client interface specification.
    return _connected;
}

WebSocketClientAdapter::operator bool() {
    // Allows the object to be used in boolean expressions (e.g., `if (client)`).
    return _connected;
}