/* =========================================================================================
 * Project:     Evse-SyncCharge
 * Description: Adapter to make WebSocketsClient look like a standard Client (Stream)
 *              for use with PubSubClient.
 * =========================================================================================
 */

#ifndef WEBSOCKET_CLIENT_ADAPTER_H
#define WEBSOCKET_CLIENT_ADAPTER_H

#include <Arduino.h>
#include <Client.h>
#include <WebSocketsClient.h>
#include <vector>

class WebSocketClientAdapter : public Client {
private:
    WebSocketsClient _ws;
    std::vector<uint8_t> _rxBuffer;
    bool _connected = false;
    
public:
    WebSocketClientAdapter();
    void begin(const char* host, uint16_t port, const char* url = "/", bool useTls = false);
    void loop();

    // Client Interface Implementation
    virtual int connect(IPAddress ip, uint16_t port) override;
    virtual int connect(const char *host, uint16_t port) override;
    virtual size_t write(uint8_t data) override;
    virtual size_t write(const uint8_t *buf, size_t size) override;
    virtual int available() override;
    virtual int read() override;
    virtual int read(uint8_t *buf, size_t size) override;
    virtual int peek() override;
    virtual void flush() override;
    virtual void stop() override;
    virtual uint8_t connected() override;
    virtual operator bool() override;
};

#endif