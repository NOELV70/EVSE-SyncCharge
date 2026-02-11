# ⚡ EVSE-SyncCharge: The Future of Open-Source EV Charging

**Professional Grade. Mission Critical. Dual-Core Powered.**

Stop settling for basic, unreliable charging solutions. **EVSE-SyncCharge** is a premium, open-source firmware designed for the **ESP32**, delivering commercial-grade reliability with the flexibility of a DIY platform. 

Whether you are building a solar diverter, a smart home integration, or a robust commercial charger, EVSE-SyncCharge is the kernel you need.

---

## 🚀 Why EVSE-SyncCharge?

### 🛡️ Uncompromised Safety Architecture
Safety isn't a feature; it's the foundation.
*   **Dual-Core Isolation**: Charging logic runs on a dedicated high-priority core, completely isolated from WiFi and Web Interface lag. Your car stays safe even if the network freezes.
*   **Hardware Watchdogs**: Multiple layers of hardware and software watchdogs ensure the system recovers from any fault in milliseconds.
*   **RCM Integration**: Native support for Residual Current Monitoring (RCM) with automated self-testing (IEC 62955/61851 compliant).
*   **Relay Protection**: Zero-spark disconnection logic protects your high-voltage contactors from premature wear.

### ☀️ Built for Solar & Smart Energy
Maximize your self-consumption without breaking the rules.
*   **Dynamic Phase Switching**: Automatically shifts between **1-Phase** (low power) and **3-Phase** (high power) charging based on available solar excess.
*   **Solar Throttling**: Intelligent "Soft-Start" and dynamic amperage adjustment (down to 6A and below) to match your solar generation curve perfectly.
*   **Grid-Aware**: MQTT Failsafe stops charging instantly if your home energy meter goes offline, preventing expensive grid imports.

### 🔌 Enterprise Connectivity
Connect to anything, anywhere.
*   **OCPP 1.6J Support**: Native support for the Open Charge Point Protocol. Connect to commercial backends like SteVe, ChargePoint, or your own server.
*   **Secure MQTT**: Full control over local MQTT with TLS encryption. perfectly suited for Home Assistant, OpenHAB, or Node-RED.
*   **Telnet & Web Console**: Remote debugging and real-time logs via Telnet or the sleek "Cyan-Diag" web console.

### ✨ Premium Experience
*   **Zero-Preset Setup**: No hardcoded WiFi credentials. Connect via a captive portal, scan your network, and go.
*   **Visual Feedback**: Native support for addressable WS2812B LEDs to show status (Charging, Error, Solar Wait, RFID Auth) at a glance.
*   **RFID Authentication**: Secure your charger with integrated RFID tag support.

---

## 🔧 Technical Specs at a Glance
*   **Platform**: ESP32 (Dual Core)
*   **Control**: 1kHz PWM Pilot Signal (J1772) + High-Precision ADC Feedback
*   **Network**: WiFi (STA/AP), mDNS, OTA Updates
*   **Protocols**: MQTT, WebSocket, HTTP, Telnet, OCPP 1.6J
*   **Kernel**: v9.0.0 "GOOSE"

## 📥 Get Started
Transform your EV charging experience today. Flash **EVSE-SyncCharge** and take control of your energy. 

**[Link to Repository / Download]**
