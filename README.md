# EVSE-SyncCharge

What ? A real-time link between your EV and your 'home'-grid, delivering millisecond-level signaling, OTA updates, and open IoT and/or OCCP integration.

Most DIY EVSE controllers behave like smart relays — they turn power on and off, EVSE-SyncCharge is a charge controller.
It implements proper pilot-signal behavior, respects the electrical and safety constraints of the vehicle’s onboard charger, and is designed to integrate into real-world energy-management systems.

_If you’re building more than a power switch, this is the controller you want._

<img width="1209" height="716" alt="image" src="https://github.com/user-attachments/assets/f9b5a872-c2fd-47f3-84fe-b304f740b171" />

EVSE-SyncCharge is a firmware build to transform the ESP32 into a fully compliant, Vehicle Supply Equipment (EVSE) controller. 

This Firmare implements the full SAE J1772 / IEC 61851 protocol stack, providing a robust foundation for dynamic load balancing, 
solar energy matching.

Safety-First Architecture
Built on a "Safety" design philosophy, the system prioritizes physical protection of the vehicle and infrastructure above all else.

* Integrated RCM Protection: Native support for Residual Current Monitors (RCM) with automated IEC-compliant self-testing intervals. 
The system executes a pre-charge safety check before every session and instantly trips the contactor if a fault is detected.

Multi-Layer System Supervision:

* Hardware WDT: An second hardware supervisor resets the MCU in the event of a network stack deadlock.

* ThrottleAlive™ Protocol: A centralized safety heartbeat that automatically throttles charging to a safe minimum, if external control signals (MQTT/OCPP) are lost, preventing grid overloads during network outages.

* Boot Loop Protection: A persistent "Strike System" using RTC memory tracks system stability across reboots. If the device enters a rapid crash loop (>5 crashes without stability), it engages a **Safety Lockout** to prevent dangerous relay chattering. The system intelligently distinguishes between a **Power Outage** (Safe Auto-Recovery) and a **System Crash** (Lockout).

* Synchronized Soft-Stop: Prevents contactor arcing by electronically terminating the charge via the Pilot signal, milliseconds before opening the mechanical relay.

* Anti-Chatter Hysteresis: Intelligent state-machine logic filters signal noise to prevent rapid relay cycling, extending hardware lifespan.

Universal Connectivity
* Designed for the modern energy ecosystem, EVSE-SyncCharge speaks the languages of both Smart Homes and Commercial Grids.

* OCPP 1.6J Compliance: Full WebSocket/WSS implementation allows connection to commercial backends (SteVe, Monta, etc.) for remote billing, authorization, and fleet management.

* **Integrated RFID Access Control:** A complete, user-configurable RFID management system. Enable or disable RFID authentication, add up to 10 authorized tags with custom names (e.g., "Noel's Key"), and use them to start/stop charging sessions. Features a web-based "Learn Mode" for easy tag registration and provides instant visual feedback with LED flashes for accepted or denied scans. All tags are persistently stored in NVS.

* Native MQTT & Home Assistant: Features "Zero-Config" Auto-Discovery for Home Assistant.
  Instantly exposes sensors for Current, Voltage, Pilot Duty, and Vehicle State without writing a single line of YAML.

* Captive Portal Onboarding: A polished "Out-of-the-Box" experience allows users to configure WiFi, Static IPs, and Amperage limits via a smartphone browser—no coding required.
*   **Customizable Web Interface:** Features a responsive dashboard with **Different selectable color themes** (Yellow, Blue, Dark Blue, Green, Dark Green), allowing users to personalize the charger's appearance via the Admin panel.

Intelligent Energy Management
Turn your EV into a grid-stabilizing asset.

**Dynamic 1-Phase / 3-Phase Switching:**
The system supports runtime switching between single-phase and three-phase charging modes. This is particularly useful for Solar PV integration, allowing charging to start at 1.4kW (6A 1-phase) and scale up to 22kW (32A 3-phase) as solar production increases.
*   **Auto-Switching Logic:** (When configured in Auto Mode) Automatically engages L2+L3 when requested current exceeds 23A, and drops back to 1-Phase if current falls below 7A.
*   **Safety Interlock:** Enforces a mandatory **15-second safety delay** during phase transitions to allow the vehicle's onboard charger capacitors to discharge, preventing hardware damage.

Solar Excess Charging: Supports dynamic amperage adjustment in real-time. 
The unique "Solar Throttle" mode allows the system to modulate charging power to match solar production curve perfectly.

Dynamic Load Balancing: Real-time API endpoints allow external energy meters to throttle the EVSE instantly when household loads (like heat pumps or ovens) peak.

Technical Specifications
  Core Architecture	Dual-Core ESP32 (FreeRTOS)
  Protocol	SAE J1772 / IEC 61851 (States A-F)
  PWM Precision	1kHz @ 12-bit Resolution
  Security	WPA2/WPA3 WiFi, TLS/SSL for OCPP
  Updates	OTA (Over-The-Air) with Safety Interlock
  Diagnostics	Real-time "Cyan-Diag" Web Console
  Settings saved to nvs.
    
<img width="599" height="747" alt="image" src="https://github.com/user-attachments/assets/c54b1611-8b2c-4dae-ab47-75383c240171" />
<img width="578" height="894" alt="image" src="https://github.com/user-attachments/assets/615cf967-1121-4093-954a-ca737cb721dd" />
<img width="544" height="877" alt="image" src="https://github.com/user-attachments/assets/2cb59e41-261e-4c45-a2f2-ec754b5e7882" />
<img width="522" height="730" alt="image" src="https://github.com/user-attachments/assets/e8977c46-8974-4426-9abe-26caa5f04311" />
<img width="432" height="895" alt="image" src="https://github.com/user-attachments/assets/2c37baa7-36bf-4647-b4c2-8c43871fd1a0" />
<img width="462" height="717" alt="image" src="https://github.com/user-attachments/assets/9486444c-4c0a-4d8c-b392-407ee8619fe5" />


Real Charge Control. Not Just a Smart Relay.

_**EVSE-SyncCharge**_ : 
* is a EV charge controller that understands the power of the pilot signal,
* enforces onboard-charger safety limits,
* and integrates seamlessly with professional energy-management systems.

Built for/by engineers. Trusted by infrastructure.
