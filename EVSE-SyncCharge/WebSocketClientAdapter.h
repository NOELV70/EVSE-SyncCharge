/* =========================================================================================
 * Project:     Evse-SyncCharge
 * Description: Header for WebSocketClientAdapter.
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

#ifndef WEBSOCKET_CLIENT_ADAPTER_H
#define WEBSOCKET_CLIENT_ADAPTER_H

#include <Arduino.h>
#include <Client.h>           // The interface we are implementing
#include <WebSocketsClient.h> // The library we are adapting
#include <vector>             // Used for the internal receive buffer

/**
 * @class WebSocketClientAdapter
 * @brief Wraps a WebSocketsClient to provide a standard Arduino Client interface.
 *
 * This adapter allows a WebSocket connection to be treated as a standard network
 * stream, making it compatible with libraries like PubSubClient that expect a
 * `Client` object. It buffers incoming WebSocket message payloads and serves them
 * through the `read()` and `available()` methods.
 */
class WebSocketClientAdapter : public Client {
private:
    WebSocketsClient _ws;           // The underlying WebSocket client instance.
    std::vector<uint8_t> _rxBuffer; // Internal buffer to store incoming message data.
    bool _connected = false;        // Tracks the connection state of the WebSocket.
    
public:
    /**
     * @brief Constructor. Sets up the WebSocket event handler.
     */
    WebSocketClientAdapter();

    /**
     * @brief Initializes the WebSocket connection.
     * @param host The server hostname.
     * @param port The server port.
     * @param url The URL path for the WebSocket endpoint (e.g., "/mqtt").
     * @param useTls True to use a secure WebSocket connection (WSS).
     */
    void begin(const char* host, uint16_t port, const char* url = "/", bool useTls = false);

    /**
     * @brief Must be called in the main loop to process WebSocket events.
     */
    void loop();

    // =========================================================================
    // Arduino Client Interface Implementation
    // =========================================================================

    // Note: These connect methods are not used by PubSubClient when the client
    // is injected. The connection is managed by this adapter's `begin()` and `loop()`.
    virtual int connect(IPAddress ip, uint16_t port) override;
    virtual int connect(const char *host, uint16_t port) override;

    virtual size_t write(uint8_t data) override;
    virtual size_t write(const uint8_t *buf, size_t size) override;

    virtual int available() override;
    virtual int read() override;
    virtual int read(uint8_t *buf, size_t size) override;
    virtual int peek() override;

    // Note: flush() is a no-op as there is no explicit transmit buffer to clear.
    virtual void flush() override;

    virtual void stop() override;
    virtual uint8_t connected() override;

    /**
     * @brief Allows the object to be used in boolean contexts (e.g., `if (client)`).
     * @return True if the WebSocket is connected, false otherwise.
     */
    virtual operator bool() override;
};

#endif