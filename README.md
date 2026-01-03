# Evse_Simplified
Bridging the gap between the grid and your EV with smart PWM signaling, OTA agility, and MQTT-driven charging intelligence.


Smart ESP32 EV Controller
A robust, WiFi-enabled Electric Vehicle Supply Equipment (EVSE) controller built on the ESP32. 
This firmware manages the SAE J1772 signaling protocol, providing smart charging capabilities with full MQTT integration for Home Assistant and remote monitoring.

⚡ Core Features
Smart Protocol Management: Full implementation of SAE J1772 pilot signal PWM generation and feedback monitoring.

IoT Ready: Integrated MQTT client for real-time telemetry and remote control (Start/Stop/Current Limiting).

Home Assistant Integration: Supports MQTT Discovery for instant "plug-and-play" dashboard setup.

Safety First: Real-time three-phase current monitoring and relay state management.

OTA Updates: Seamless Over-The-Air firmware updates for maintenance without physical access.

Structured Logging: Tiered logging system (Info, Debug, Error) for easier hardware debugging.
