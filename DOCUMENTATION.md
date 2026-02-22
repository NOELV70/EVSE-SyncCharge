# ═══════════════════════════════════════════════════════════════════════════════
#                           EVSE-SyncCharge
#            Industrial-Grade ESP32 EV Charging Controller
# ═══════════════════════════════════════════════════════════════════════════════

EVSE-SyncCharge : Charge Your Car, NOT Your Electricity Bill.
 
Your Solar Power, Your Car, Zero Waste, iow, the ability to charge your EV with excess solar energy — and never pay grid prices when you don’t have to.

EVSE-SyncCharge - What ? A real-time link between your EV and your 'home'-grid, delivering millisecond signaling, OTA updates, and open IoT and/or OCCP integration.

EVSE-SyncCharge is a mission-critical, WiFi-enabled Electric Vehicle Supply 
Equipment (EVSE) controller firmware. Built on the dual-core ESP32, it implements 
the full **SAE J1772 / IEC 61851** protocol stack while providing a modern, 
developer-friendly IoT and web interface.

Unlike "dumb" chargers that merely click a relay, this system provides a real-time 
window into the charging process—enabling dynamic load balancing, solar energy 
matching, and industrial-grade safety monitoring.

---

## 🚀 CORE CAPABILITIES

### Smart Protocol Management
- Full **1kHz PWM generation** with 12-bit resolution
- High-precision ADC feedback for vehicle state detection (States A-F)
- Non-blocking state machine with millisecond-precision transitions

### Zero-Preset Configuration  
- No hardcoded credentials
- All settings (WiFi, MQTT, OCPP, Amperage limits) configured via **Captive Web Portal**
- Persistent storage in **NVS (Non-Volatile Storage)**—survives reboots and updates

### IoT & Smart Home Integration
- **Native MQTT** with Auto-Discovery for Home Assistant ("plug-and-play")
- **OCPP 1.6J Compliance** (WebSocket/WSS) for commercial backends (SteVe, Monta)
- Real-time telemetry: Current, Voltage, Pilot Duty, Vehicle State

### One-Click Networking
- Intelligent transition from DHCP to Static IP
- Auto-detects network parameters (IP, Gateway, Subnet) to suggest optimal configuration

### Remote Diagnostics
- **Cyan-Diag Web Console**: Real-time Pilot Voltage, Free Heap, System Uptime
- **Telnet Console**: Authenticated remote log streaming with session management

### Enterprise Connectivity
- **Secure Transport**: MQTT over TLS (MQTTS) and WebSockets (WSS)
- **Smart Availability**: LWT (Last Will & Testament) for instant offline detection
- **Resilience**: Exponential backoff reconnection strategies

---

## 🛡️ MISSION-CRITICAL SAFETY LAYER

### 1. Hardware Watchdog (WDT) Supervisor
Prevents the charger from becoming "stuck" in an unsafe state during software crashes.

| Feature | Implementation |
|---------|----------------|
| **Timeout** | second hardware supervisor |
| **The "Kick"** | Main loop resets timer every cycle |
| **Lockup Recovery** | WiFi/MQTT deadlock triggers immediate MCU reset |
| **Safety Default** | On reset, relay GPIO forced LOW (contactor open) |

### 2. Synchronized PWM-Abort & OTA Interlock
Prevents arcing and contactor wear through "Soft-Stop" sequencing.

| Phase | Action |
|-------|--------|
| **Pilot Abort** | PWM instantly set to +12V (100% duty)—signals EV to cease draw |
| **Zero-Load Break** | Relay opens only after vehicle confirms power cessation |
| **OTA Safety** | `openImmediately()` bypasses debounce timers during firmware updates |

### 3. ThrottleAlive™ Protocol
Centralized safety heartbeat for external control systems.

- If MQTT/OCPP commands stop arriving, charging **ramps down to 6A minimum**
- Prevents grid overloads during network outages
- Configurable timeout (default: disabled, 0 = off)

### 4. Mechanical Protection

| Feature | Value | Purpose |
|---------|-------|---------|
| **Anti-Chatter Hysteresis** | 3000ms `RELAY_SWITCH_DELAY` | Prevents rapid relay cycling from noise |
| **Pre-Init Pin Lock** | GPIO 16 forced LOW at boot | Eliminates startup glitches |

### 5. Integrated RCM Protection
Native support for Residual Current Monitors with IEC-compliant self-testing.

- **Boot-up Self-Test**: Validates RCM before first charge
- **Periodic Self-Test**: Every 24 hours (IEC 62955 / IEC 61851 recommendation)
- **Pre-Charge Test**: Safety check before every charging session
- **Instant Trip**: Immediately opens contactor on fault detection

### 6. Boot Loop Protection
A persistent "Strike System" using RTC memory tracks system stability across reboots.

- **Crash Detection**: Tracks rapid crash loops (>5 crashes without stability).
- **Safety Lockout**: Engages if instability is detected to prevent dangerous relay chattering.
- **Smart Recovery**: Distinguishes between **Power Outage** (Safe Auto-Recovery) and **System Crash** (Lockout).

---

## 🔐 ADVANCED ACCESS CONTROL

### RFID Management System
A fully-featured, web-configurable RFID authentication system.

| Feature | Description |
|---------|-------------|
| **Enable/Disable** | Single-click activation in web UI |
| **Tag Management** | Add, name, and delete up to **10 authorized tags** |
| **Buzzer Control** | Enable or disable audio feedback for scans |
| **Learn Mode** | Web-based registration—no manual UID entry required |
| **Start/Stop** | Tap registered card to toggle charging session |
| **Visual Feedback** | LED flashes green (accepted) or red (denied) |
| **Persistent Storage** | Tags stored in NVS, survive reboots |

---

## ⚡ INTELLIGENT ENERGY MANAGEMENT

### Proximity Pilot (PP) Cable Sensing
The controller automatically detects the maximum current rating of the connected charging cable via the Proximity Pilot (PP) signal. This is a critical safety feature that prevents the charger from delivering more current than the cable can handle, regardless of user settings.

- **Automatic Detection**: Reads the resistance in the cable plug (per IEC 61851) to determine if it's a 13A, 20A, 32A, or 63A cable.
- **Configurable**: This feature is enabled by default but can be disabled in the web UI. If disabled, the system will use the user-configured "Max Current" as the hardware limit.
- **Safety Hierarchy**: The final charging current is always the **minimum** of three values:
  1.  The dynamically requested current (from MQTT/OCPP/Web).
  2.  The user-configured "Max Current" (the installation's ceiling).
  3.  The detected cable rating (the physical hardware limit).

### Solar Excess Charging
- Dynamic amperage adjustment (6A–80A) in real-time
- **"Solar Throttle" mode**: Modulates power to match solar production curve
- Sub-6A charging option for maximum PV utilization

---

## 🌈 VISUAL STATUS FEEDBACK

### Intelligent LED System
The controller supports WS2812B (Neopixel) LED strips to provide immediate visual feedback on the charger's status. This eliminates the need to check the web dashboard for common states.

**Fully Configurable:**
Every state can be customized with specific **Colors** (Red, Green, Blue, Cyan, Magenta, Yellow, White) and **Effects** (Solid, Breathe, Flash, Flow, Pulse) via the Web UI.

**Key Status Indicators:**
| System State | Default Indication | Meaning |
|--------------|--------------------|---------|
| **Standby** | Green (Breathing) | Ready to charge, waiting for vehicle. |
| **Connected** | Blue (Solid) | Vehicle plugged in, waiting for start command. |
| **Charging** | Cyan (Flowing) | Active energy transfer. |
| **Solar Idle** | Yellow (Pulse) | Paused due to low solar production (<6A). |
| **RFID Accepted** | Green (Flash) | Authentication successful. |
| **RFID Rejected** | Red (Flash) | Access denied. |
| **Error/Fault** | Red (Fast Flash) | RCM trip, Diode fault, or Safety Lockout. |
| **WiFi Setup** | Magenta (Pulse) | Access Point active for configuration. |

---

## ⚠️ HARDWARE DIAGNOSTICS (DANGER ZONE)

**WARNING: DO NOT CONNECT A VEHICLE WHILE USING THIS PAGE.**

The "Hardware Diagnostics" page provides direct, low-level control over the EVSE hardware. It bypasses the standard J1772 state machine and safety interlocks to allow for bench testing of the relay, pilot signal generation, and sensor calibration.

**Access Control:**
Due to the potential risks, this page is protected by a secondary authentication layer.
- **Default User:** `hardware`
- **Default Password:** `hardware`
*(These can be changed in the Admin Security settings)*
> **Security Note:** The authentication realm for this page is dynamic. This prevents browsers from caching credentials, ensuring a fresh login prompt is requested upon every visit.

**Capabilities:**
- **Force PWM:** Manually set the Control Pilot duty cycle (10% - 96%) to verify signal generation and ADC feedback.
- **Force Relay:** Manually close/open the main contactor (L1) and phase expansion relays (L1+L2+L3).
- **Raw Sensor Data:** View raw ADC values (mV) for the Pilot and Proximity sensors to diagnose wiring issues.

**Safety Notice:**
This mode is intended for **bench testing only**. Forcing relays closed or manipulating the pilot signal while a vehicle is connected can result in equipment damage or unsafe conditions.

---

## �️ TECHNICAL DEEP DIVE

### The SAE J1772 State Machine

| State | Pilot Voltage | Description | Firmware Action |
|-------|---------------|-------------|-----------------|
| **A** | +12V | Standby / No Vehicle | PWM Off; Relay Open |
| **B** | +9V ±1 | Vehicle Detected | Start PWM; Wait for 'Ready' |
| **C** | +6V ±1 | Ready to Charge | Close Relay; Monitor Load |
| **D** | +3V ±1 | Ventilation Required | Close Relay; Log Vent State |
| **E/F** | 0V / -12V | Error / Diode Fault | Emergency Stop; Lockout |

### Hardware Pin Configuration

| Component | GPIO (Default) | Function |
|------------------|----------------|------------------------------------------|
| **Relay L1 (Main)** | 14 | Enables/Disables High-Voltage AC Output |
| **Relay L2+L3** | 13 | Phase Expansion (3-Phase Switching) |
| **Pilot PWM** | 27 | SAE J1772 Control Pilot (1kHz) |
| **Pilot Feedback** | 36 | ADC Input for Pilot State Detection |
| **Proximity Pilot**| 39 | ADC Input for Cable Current Sensing |
| **RCM Fault** | 25 | Residual Current Monitor Input |
| **RCM Test** | 26 | RCM Self-Test Trigger |
| **RFID SS** | 5 | SPI Slave Select for RC522 |
| **RFID RST** | 17 | Reset for RC522 module |
| **Buzzer** | 4 | Audio feedback for RFID |
| **RGB LED** | 22 | WS2812 status indicator |

### Technical Specifications

| Feature | Specification |
|---------|---------------|
| **Core Architecture** | Dual-Core ESP32 (FreeRTOS) |
| **Protocol** | SAE J1772 / IEC 61851 (States A-F) |
| **PWM Precision** | 1kHz @ 12-bit Resolution |
| **Current Range** | 6A–80A (dynamic adjustment) |
| **Security** | WPA2/WPA3 WiFi, TLS/SSL for OCPP |
| **Updates** | OTA (Over-The-Air) with Safety Interlock |
| **Diagnostics** | Web Console, Telnet Logging |

---

## 📡 CONNECTIVITY & PROTOCOLS

### MQTT Interface
```
tele/EVSE-[ID]/LWT     → Online/Offline status
tele/EVSE-[ID]/SENSOR  → Current, Voltage, Pilot State
cmnd/EVSE-[ID]/charge  → START or STOP
cmnd/EVSE-[ID]/limit   → Set maximum current (6A–80A)
```

### OCPP 1.6J
- Full WebSocket/WSS implementation
- Compatible with: SteVe, Monta, Open Charge Point Protocol backends


### Telnet Console
- Authenticated remote log streaming (uses Web UI credentials)
- Configurable port (default: 23)
- Real-time firmware debug output

## ⚡ INTELLIGENT ENERGY MANAGEMENT

### Solar Excess Charging
- Dynamic amperage adjustment (6A–80A) in real-time
- **"Solar Throttle" mode**: Modulates power to match solar production curve
- Sub-6A charging option for maximum PV utilization

### Dynamic 1-Phase / 3-Phase Switching
Runtime switching for optimal Solar PV integration (1.4kW to 22kW range).
- **Auto-Switching**: Engages L2+L3 when current > 23A; drops to 1-Phase < 7A.
- **Safety Interlock**: Enforces mandatory **15-second safety delay** during transitions to discharge vehicle capacitors.

### Dynamic Load Balancing
- Real-time MQTT/OCPP endpoints for external energy meters
- Instantly throttle EVSE when household loads peak
- Protect main fuse from overload


---

##  QUICK START GUIDE

### Phase 1: Initial Connectivity
Out of the box, the controller broadcasts its own WiFi network for configuration.

1.  **Power Up**: Energize the controller.
2.  **Connect**: On your smartphone or laptop, search for a WiFi network named **`EVSE-XXXX-SETUP`** (where XXXX is the unique device ID).
3.  **Captive Portal**: Connect to this network. A "Sign In to Network" page should appear automatically.
    -   *If not, open a browser and navigate to `http://192.168.4.1`*

### Phase 2: Network Configuration
1.  **WiFi Setup**:
    -   Click **"Scan WiFi"** to find your home network.
    -   Select your SSID and enter the password.
2.  **IP Assignment**:
    -   **DHCP**: Default. Good for simple setups.
    -   **Static IP (Recommended)**: Select "Static IP". The system will auto-detect your current Subnet and Gateway. Enter a fixed IP (e.g., `192.168.1.200`) to ensure the device is always reachable at the same address.
3.  **Save & Reboot**: Click Save. The device will restart and connect to your home network.

### Phase 3: MQTT & Smart Home Setup
Once connected to your home WiFi, access the dashboard at `http://evse-xxxx.local` (or the IP you assigned).

1.  Navigate to **Settings > MQTT Configuration**.
2.  **Broker Details**:
    -   **Enable MQTT**: Toggle to "Enabled".
    -   **Host**: Enter your broker IP (e.g., `192.168.1.10`) or hostname.
    -   **Port**: Default `1883` (TCP) or `8883` (TLS).
3.  **Security (v5.7+)**:
    -   **Use TLS**: Enable for encrypted MQTTS connections.
    -   **Use WebSockets**: Enable if your broker requires WS/WSS.
4.  **Home Assistant**:
    -   Ensure your Home Assistant has the MQTT integration enabled.
    -   The charger will automatically appear in HA as a new device with controls for **Current Limit**, **Start/Stop**, and sensors for **Power**, **Voltage**, and **Vehicle State**.

---

## 📝 PROJECT INFO

| Field | Value |
|-------|-------|
| **Lead Developer** | Noel Vellemans |
| **License** | GNU General Public License v2.0 (GPLv2) |
| **Copyright** | © 2026 Noel Vellemans |

═══════════════════════════════════════════════════════════════════════════════
#                              END OF DOCUMENT
# ═══════════════════════════════════════════════════════════════════════════════
