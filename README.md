# EVSE-SyncCharge : Charge Your Car, NOT Your Electricity Bill.
<br>
<h3> <center> Your Solar Power, Your Car, Zero Waste, iow, the ability to charge your EV with excess solar energy, AND never pay grid prices when you don’t have to. <br> </h3>
<br>
<br>
<img width="1088" height="960" alt="EVSE-Sync-CHARGE" src="https://github.com/user-attachments/assets/3285fc1c-a4ee-49e0-ad6b-7e0b0edb869d" />
<br>

> [!CAUTION]
> ### ⚠ Project Status Notice
> **EVSE-SyncCharge is an ongoing project.**
>
> It has recently passed the alpha phase as a working proof of concept.<br>The system is functional and actively tested, but development is still in progress.
>
> Detailed schematics, hardware documentation, and additional technical information will be released gradually as available free time permits.<br>

EVSE-SyncCharge - What?<br> 
EVSE-SyncCharge isn’t just another EV charger — it’s the intelligent bridge between your home’s energy ecosystem and your vehicle.
With millisecond-precise control, seamless MQTT and Home Assistant integration, and secure over-the-air updates that keep your system evolving, EVSE-SyncCharge puts real power back in your hands.


Most DIY EVSE controllers behave like smart relays — they turn power on and off, EVSE-SyncCharge is a 'smart' charge controller.
It implements proper pilot-signal behavior, respects the electrical and safety constraints of the vehicle’s onboard charger, and is designed to integrate into real-world energy-management systems.

_If you’re building more than a power switch, this is the DIY controller you want/need._

<img width="1209" height="716" alt="image" src="https://github.com/user-attachments/assets/f9b5a872-c2fd-47f3-84fe-b304f740b171" />

EVSE-SyncCharge is a firmware (and DIY hardware) build  a fully compliant, Vehicle Supply Equipment (EVSE) controller. 

This Firmare implements the full SAE J1772 / IEC 61851 protocol stack, providing a robust foundation for dynamic load balancing, solar energy matching.

Safety-First Architecture
Built on a "Safety" design philosophy, the system prioritizes physical protection of the vehicle and infrastructure above all else.

* Integrated RCM Protection: Native support for Residual Current Monitors (RCM) with automated IEC-compliant self-testing intervals. 
The system executes a pre-charge safety check before every session and instantly trips the contactor if a fault is detected.

Multi-Layer System Supervision:

* Hardware WDT: A hardware Watchdog resets the MCU in the event of a network stack deadlock.

* ThrottleAlive™ Protocol: A centralized safety heartbeat that automatically throttles charging to a safe minimum, if external control signals (MQTT/OCPP) are lost, preventing grid overloads during network outages.

* Boot Loop Protection: A persistent "Strike System" using NV-memory tracks system stability across reboots. If the device enters a rapid crash loop (>5 crashes without stability), it engages a **Safety Lockout** to prevent dangerous relay chattering. The system intelligently distinguishes between a **Power Outage** (Safe Auto-Recovery) and a **System Crash** (Lockout).

* Synchronized Soft-Stop: Prevents contactor arcing by electronically terminating the charge via the Pilot signal, milliseconds before opening the mechanical relay.

* OTA Interlock: During firmware updates, the system executes an immediate safety break, bypassing standard debounce timers to ensure the high-voltage contactor is physically open before the MCU begins flashing.

* Anti-Chatter Hysteresis: Intelligent state-machine logic filters signal noise to prevent rapid relay cycling, extending hardware lifespan.

Universal Connectivity
* Designed for the modern energy ecosystem, EVSE-SyncCharge speaks the languages of both Smart Homes and Commercial Grids.
* **Proximity Pilot (PP) Sensing:** Automatically detects the cable's physical current limit (e.g., 16A, 32A) and enforces it for safety. This feature is fully configurable via the web UI.
* OCPP 1.6J Compliance: Full WebSocket/WSS implementation allows connection to commercial backends (SteVe, Monta, etc.). 

* **Integrated RFID Access Control:** A complete, user-configurable RFID management system. Enable/disable the reader and buzzer, add up to 10 authorized tags with custom names (e.g., "Noel's Key"), and use them to start/stop charging sessions. Features a web-based "Learn Mode" for easy tag registration and provides instant visual feedback with LED flashes for accepted or denied scans. All tags are persistently stored in NVS.

* Native MQTT & Home Assistant: Features "Zero-Config" Auto-Discovery for Home Assistant.
  Instantly exposes sensors for Current, Voltage, Pilot Duty, and Vehicle State without writing a single line of YAML.
<img width="858" height="779" alt="image" src="https://github.com/user-attachments/assets/cfbec73f-aa70-41ca-9319-36fec41cc1e8" />


* Captive Portal Onboarding: A polished "Out-of-the-Box" experience allows users to configure WiFi, Static IPs, and Amperage limits via a smartphone browser—no coding required.
*   **Customizable Web Interface:** Features a responsive dashboard with **7 selectable color themes** (Yellow, Blue, Dark Blue, Green, Dark Green, Red, Dark Red), allowing users to personalize the charger's appearance via the Admin panel.
 ![EVSE-themes](https://github.com/user-attachments/assets/0e4e9efa-d81b-4593-9af7-b700fb3cc890)
 
*   **Telnet Remote Console:** Integrated Telnet server for real-time remote logging and debugging. Connect via any Telnet client to view live system logs, authentication events, and state transitions.

  
*   **Dynamic LED Feedback:** Supports WS2812B addressable LEDs for intuitive status indication (Charging, Error, RFID Auth, Solar Wait) with customizable colors and effects.
<img width="383" height="898" alt="image" src="https://github.com/user-attachments/assets/f7f33cb6-147e-42d2-9696-ef317b9fb602" />
<img width="394" height="636" alt="image" src="https://github.com/user-attachments/assets/d638d74d-9e7b-4ebb-ab89-0c76c4c89c36" />
<img width="390" height="673" alt="image" src="https://github.com/user-attachments/assets/c2fdf9f1-dcc9-4f47-88de-dff8de6d4e72" />


### Enterprise Connectivity & Reliability 
We have significantly hardened the MQTT and Network stack to ensure the charger remains online and responsive in real-world conditions.

*   **TLS Security (MQTTS):** Added support for MQTT over TLS (MQTTS). You can now toggle "Use TLS" in the MQTT configuration page to encrypt control traffic between the charger and your broker.
*   **Smart Availability & LWT:** The charger now utilizes MQTT "Last Will and Testament" (LWT). If the device loses power or WiFi, the broker automatically marks the device as `offline`. All Home Assistant entities will instantly grey out, preventing "ghost" commands.
*   **Exponential Backoff Strategy:** Instead of hammering the broker with retries during an outage, the system uses an intelligent backoff algorithm ($1s \to 2s \to 4s \dots$ up to 5 mins), reducing network congestion.
*   **PSRAM Memory Optimization:** Generating large Home Assistant discovery payloads is now offloaded to the ESP32's external PSRAM (SPIRAM). This preserves critical internal SRAM for the WiFi and TLS stacks, significantly reducing the risk of crashes during heavy network activity.

Intelligent Energy Management
Turn your EV into a grid-stabilizing asset.

**Dynamic 1-Phase / 3-Phase Switching:**
The system supports runtime switching between single-phase and three-phase charging modes. This is particularly useful for Solar PV integration, allowing charging to start at 1.4kW (6A 1-phase) and scale up to 22kW (32A 3-phase) as solar production increases.
*   **Auto-Switching Logic:** (When configured in Auto Mode) Automatically engages L2+L3 when requested current exceeds 23A, and drops back to 1-Phase if current falls below 7A.
*   **Safety Interlock:** Enforces a mandatory **15-second safety delay** during phase transitions to allow the vehicle's onboard charger capacitors to discharge, preventing hardware damage.

Solar Excess Charging: Supports dynamic power adjustment in real-time. 
The unique "Solar Throttle" mode allows the system to modulate charging power to match solar production curve perfectly.

Dynamic Load Balancing: Real-time API endpoints allow external energy meters to throttle the EVSE instantly when household loads (like heat pumps or ovens) peak.

Technical Specifications<br>
  Core Architecture	Dual-Core ESP32 (FreeRTOS) <br>
  Protocol	SAE J1772 / IEC 61851 (States A-F)<br>
  PWM Precision	1kHz @ 12-bit Resolution<br>
  Security	WPA2/WPA3 WiFi, TLS/SSL for OCPP<br>
  Updates	OTA (Over-The-Air) with Safety Interlock<br>
  Diagnostics	Real-time "Cyan-Diag" Web Console<br>
  Settings saved to nvs.<br>
    



_**EVSE-SyncCharge**_ : 
* Real Charge Control. Not Just a Smart Relay.<br>
* is a EV charge controller that understands the power of the pilot signal,<br>
* enforces onboard-charger safety limits,<br>
* and integrates seamlessly with home and commercial energy-management systems.<br>
* EVSE-SyncCharge sits at the intersection of your power-grid and your vehicle, acting as the "brain" that negotiates every watt.

Built by an engineer, Trusted where it matters..<br>

**☕ “Fuel my circuits and my code—buy me a coffee and keep this kernel from crashing!”** 

[![ko-fi](https://ko-fi.com/img/githubbutton_sm.svg)](https://ko-fi.com/C0C21TRVZ5)
