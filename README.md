# EVSE-SyncCharge

EVSE-SyncCharge: The Industrial-Grade ESP32 Firmware.


A real-time link between grid and EV, delivering millisecond-level signaling, OTA firmware updates, and open IoT integration.

<img width="577" height="813" alt="image" src="https://github.com/user-attachments/assets/12d64285-cb6b-4eac-becc-d822a734c904" />


EVSE-SyncCharge is a firmware architecture designed to transform the ESP32 into a fully compliant, safety-first Electric Vehicle Supply Equipment (EVSE) controller. 

This Firmare implements the full SAE J1772 / IEC 61851 protocol stack, providing a robust foundation for dynamic load balancing, 
solar energy matching.

Safety-First Architecture
Built on a "Safety" design philosophy, the system prioritizes physical protection of the vehicle and infrastructure above all else.

Integrated RCM Protection: Native support for Residual Current Monitors (RCM) with automated IEC-compliant self-testing intervals. 
The system executes a pre-charge safety check before every session and instantly trips the contactor if a fault is detected.

Dual-Layer Watchdog Supervision:
Hardware WDT: An 8-second hardware supervisor resets the MCU in the event of a network stack deadlock.
ThrottleAlive™ Protocol: A centralized safety heartbeat that automatically throttles charging to a safe minimum, if external control signals (MQTT/OCPP) are lost, preventing grid overloads during network outages.
Synchronized Soft-Stop: Prevents contactor arcing by electronically terminating the charge via the Pilot signal, milliseconds before opening the mechanical relay.
Anti-Chatter Hysteresis: Intelligent state-machine logic filters signal noise to prevent rapid relay cycling, extending hardware lifespan.

Universal Connectivity
* Designed for the modern energy ecosystem, EVSE-SyncCharge speaks the languages of both Smart Homes and Commercial Grids.

* OCPP 1.6J Compliance: Full WebSocket/WSS implementation allows connection to commercial backends (SteVe, Monta, etc.) for remote billing, authorization, and fleet management.

* **Integrated RFID Access Control:** A complete, user-configurable RFID management system. Enable or disable RFID authentication, add up to 10 authorized tags with custom names (e.g., "Noel's Key"), and use them to start/stop charging sessions. Features a web-based "Learn Mode" for easy tag registration and provides instant visual feedback with LED flashes for accepted or denied scans. All tags are persistently stored in NVS.

* Native MQTT & Home Assistant: Features "Zero-Config" Auto-Discovery for Home Assistant.
  Instantly exposes sensors for Current, Voltage, Pilot Duty, and Vehicle State without writing a single line of YAML.

* Captive Portal Onboarding: A polished "Out-of-the-Box" experience allows users to configure WiFi, Static IPs, and Amperage limits via a smartphone browser—no coding required.

Intelligent Energy Management
Turn your EV into a grid-stabilizing asset.

Solar Excess Charging: Supports dynamic amperage adjustment (6A–64A) in real-time. 
The unique "Solar Throttle" mode allows the system to modulate charging power to match solar production curve perfectly.

Dynamic Load Balancing: Real-time API endpoints allow external energy meters to throttle the EVSE instantly when household loads (like heat pumps or ovens) peak.

Technical Specifications
  Core Architecture	Dual-Core ESP32 (FreeRTOS)
  Protocol	SAE J1772 / IEC 61851 (States A-F)
  PWM Precision	1kHz @ 12-bit Resolution
  Security	WPA2/WPA3 WiFi, TLS/SSL for OCPP
  Updates	OTA (Over-The-Air) with Safety Interlock
  Diagnostics	Real-time "Cyan-Diag" Web Console

Why EVSE-SyncCharge?
  Most DIY controllers are just "smart relays." EVSE-SyncCharge is a Charge Controller. 
  It understands the physics of the Pilot signal, respects the safety limits of the vehicle's Onboard Charger, and integrates seamlessly into professional energy management systems.

<img width="599" height="747" alt="image" src="https://github.com/user-attachments/assets/c54b1611-8b2c-4dae-ab47-75383c240171" />
<img width="578" height="894" alt="image" src="https://github.com/user-attachments/assets/615cf967-1121-4093-954a-ca737cb721dd" />
<img width="544" height="877" alt="image" src="https://github.com/user-attachments/assets/2cb59e41-261e-4c45-a2f2-ec754b5e7882" />
<img width="522" height="730" alt="image" src="https://github.com/user-attachments/assets/e8977c46-8974-4426-9abe-26caa5f04311" />
<img width="432" height="895" alt="image" src="https://github.com/user-attachments/assets/2c37baa7-36bf-4647-b4c2-8c43871fd1a0" />
<img width="462" height="717" alt="image" src="https://github.com/user-attachments/assets/9486444c-4c0a-4d8c-b392-407ee8619fe5" />

