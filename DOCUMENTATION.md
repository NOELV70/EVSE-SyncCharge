# ═══════════════════════════════════════════════════════════════════════════════
#                           EVSE-SyncCharge
#            Industrial-Grade ESP32 EV Charging Controller
# ═══════════════════════════════════════════════════════════════════════════════

> **Bridging the gap between the grid and your EV with smart PWM signaling, 
> OTA agility, and MQTT-driven charging intelligence.**

EVSE-SyncCharge is a mission-critical, WiFi-enabled Electric Vehicle Supply 
Equipment (EVSE) controller firmware. Built on the dual-core ESP32, it implements 
the full **SAE J1772 / IEC 61851** protocol stack while providing a modern, 
developer-friendly IoT interface.

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

---

## 🔐 ADVANCED ACCESS CONTROL

### RFID Management System
A fully-featured, web-configurable RFID authentication system.

| Feature | Description |
|---------|-------------|
| **Enable/Disable** | Single-click activation in web UI |
| **Tag Management** | Add, name, and delete up to **10 authorized tags** |
| **Learn Mode** | Web-based registration—no manual UID entry required |
| **Start/Stop** | Tap registered card to toggle charging session |
| **Visual Feedback** | LED flashes green (accepted) or red (denied) |
| **Persistent Storage** | Tags stored in NVS, survive reboots |

---

## 🏛️ TECHNICAL DEEP DIVE

### The SAE J1772 State Machine

| State | Pilot Voltage | Description | Firmware Action |
|-------|---------------|-------------|-----------------|
| **A** | +12V | Standby / No Vehicle | PWM Off; Relay Open |
| **B** | +9V ±1 | Vehicle Detected | Start PWM; Wait for 'Ready' |
| **C** | +6V ±1 | Ready to Charge | Close Relay; Monitor Load |
| **D** | +3V ±1 | Ventilation Required | Close Relay; Log Vent State |
| **E/F** | 0V / -12V | Error / Diode Fault | Emergency Stop; Lockout |

### Hardware Pin Configuration

| Component | GPIO | Function |
|-----------|------|----------|
| **Relay Control** | 16 | Enables/Disables High-Voltage AC Output |
| **Pilot PWM** | 27 | SAE J1772 Control Pilot (1kHz) |
| **Pilot Feedback** | 36 | ADC Input for Pilot State Detection |
| **RFID SS** | 5 | SPI Slave Select for RC522 |
| **RFID RST** | 17 | Reset for RC522 module |
| **Buzzer** | 4 | Audio feedback for RFID |
| **RGB LED** | Configurable | WS2812 status indicator |

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

---

## ⚡ INTELLIGENT ENERGY MANAGEMENT

### Solar Excess Charging
- Dynamic amperage adjustment (6A–80A) in real-time
- **"Solar Throttle" mode**: Modulates power to match solar production curve
- Sub-6A charging option for maximum PV utilization

### Dynamic Load Balancing
- Real-time MQTT/OCPP endpoints for external energy meters
- Instantly throttle EVSE when household loads peak
- Protect main fuse from overload

---

## 📱 DEPLOYMENT & CONFIGURATION

### First Boot
1. Device starts as WiFi Access Point: `EVSE-XXXX-SETUP`
2. Connect via smartphone/laptop
3. Captive portal auto-opens

### Web Setup
- Enter WiFi credentials
- Configure MQTT broker details
- Set maximum current limits
- Configure OCPP backend (optional)

### Real-Time Control
- Dashboard: `http://evse-xxxx.local`
- Manual start/stop override
- Cyan-Diag diagnostic block
- Settings management

---

## 📝 PROJECT INFO

| Field | Value |
|-------|-------|
| **Lead Developer** | Noel Vellemans |
| **License** | GNU General Public License v2.0 (GPLv2) |
| **Copyright** | © 2026 Noel Vellemans |

---

## 🔬 APPENDIX: Pilot Line Test Fixture

### Simulating EVSE States with Cascade Resistors

For bench testing without a vehicle, use this resistor network:

**Components:**
- Series resistor: 1 kΩ (in line with PWM)
- BAT48 diode: ~0.3V drop
- Cascade resistors (E12 values):
  - R9 = 3.3 kΩ → 9V state
  - R6 = 1.6 kΩ → 6V state (parallel with R9)
  - R3 = 560 Ω → 3V state (parallel with R9||R6)

**Operation:**
| State | Resistors Active | Pilot Voltage |
|-------|------------------|---------------|
| 9V | R9 only | 8.9–10.4V |
| 6V | R9 ‖ R6 | 5.9–6.95V |
| 3V | R9 ‖ R6 ‖ R3 | 2.9–3.45V |
| 0V | Short to GND | 0V |

---

# ═══════════════════════════════════════════════════════════════════════════════
#                              END OF DOCUMENT
# ═══════════════════════════════════════════════════════════════════════════════
