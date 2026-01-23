# Evse_Simplified

Evse_Simplified: The Industrial-Grade ESP32 Firmware.
A real-time link between grid and EV, delivering millisecond-level signaling, OTA firmware updates, and open IoT integration.

<img width="577" height="813" alt="image" src="https://github.com/user-attachments/assets/12d64285-cb6b-4eac-becc-d822a734c904" />


Evse_Simplified is a firmware architecture designed to transform the ESP32 into a fully compliant, safety-first Electric Vehicle Supply Equipment (EVSE) controller. 
Unlike basic "smart plugs," this Firmare implements the full SAE J1772 / IEC 61851 protocol stack, providing a robust foundation for dynamic load balancing, 
solar energy matching.

Safety-First Architecture
Built on a "Safety-Kernel" design philosophy, the system prioritizes physical protection of the vehicle and infrastructure above all else.

Integrated RCM Protection: Native support for Residual Current Monitors (RCM) with automated IEC-compliant self-testing intervals. 
The system executes a pre-charge safety check before every session and instantly trips the contactor if a fault is detected.

Dual-Layer Watchdog Supervision:
Hardware WDT: An 8-second hardware supervisor resets the MCU in the event of a network stack deadlock.
ThrottleAlive™ Protocol: A centralized safety heartbeat that automatically throttles charging to a safe minimum (6A) if external control signals (MQTT/OCPP) are lost, preventing grid overloads during network outages.
Synchronized Soft-Stop: Prevents contactor arcing by electronically terminating the charge via the Pilot signal, milliseconds before opening the mechanical relay.
Anti-Chatter Hysteresis: Intelligent state-machine logic filters signal noise to prevent rapid relay cycling, extending hardware lifespan.

Universal Connectivity
Designed for the modern energy ecosystem, Evse_Simplified speaks the languages of both Smart Homes and Commercial Grids.

OCPP 1.6J Compliance: Full WebSocket/WSS implementation allows connection to commercial backends (SteVe, Monta, etc.) for remote billing, authorization, and fleet management.

Native MQTT & Home Assistant: Features "Zero-Config" Auto-Discovery for Home Assistant. Instantly exposes sensors for Current, Voltage, Pilot Duty, and Vehicle State without writing a single line of YAML.

Captive Portal Onboarding: A polished "Out-of-the-Box" experience allows users to configure WiFi, Static IPs, and Amperage limits via a smartphone browser—no coding required.

Intelligent Energy Management
Turn your EV into a grid-stabilizing asset.

Solar Excess Charging: Supports dynamic amperage adjustment (6A–80A) in real-time. The unique "Solar Throttle" mode allows the system to modulate charging power to match solar production curve perfectly.
Dynamic Load Balancing: Real-time API endpoints allow external energy meters to throttle the EVSE instantly when household loads (like heat pumps or ovens) peak.

Technical Specifications
Feature	Specification
Core Architecture	Dual-Core ESP32 (FreeRTOS)
Protocol	SAE J1772 / IEC 61851 (States A-F)
PWM Precision	1kHz @ 12-bit Resolution
Security	WPA2/WPA3 WiFi, TLS/SSL for OCPP
Updates	OTA (Over-The-Air) with Safety Interlock
Diagnostics	Real-time "Cyan-Diag" Web Console

Why Evse_Simplified?
Most DIY controllers are just "smart relays." Evse_Simplified is a Charge Controller. 
It understands the physics of the Pilot signal, respects the safety limits of the vehicle's Onboard Charger, and integrates seamlessly into professional energy management systems.

