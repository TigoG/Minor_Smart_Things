# Minor Smart Things

Central repository for small IoT projects: a Weather Station and an Actuator (sunscreen) — firmware, hardware design, BOM and testing materials.

Table of contents

- [Overview](#overview)
- [Goals](#goals)
- [Architecture](#architecture)
- [Subprojects](#subprojects)
  - [Actuator](#actuator)
  - [Weather Station](#weather-station)
- [Wiring & BOM](#wiring--bom)
- [Usage examples](#usage-examples)
- [License](#license)

## Overview

Minor Smart Things contains firmware, hardware notes and documentation for small IoT devices. The two main working pieces are a field Weather Station (ESP32) that publishes sensor telemetry and an Actuator (ESP8266/ESP32) that controls a sunscreen or similar motorised device.

## Goals

- Provide compact, extensible open-source examples for low-power environmental sensing and actuator control.
- Use standard protocols (MQTT/HTTP) and secure transport where possible.
- Make it easy to build, test and deploy firmware using PlatformIO and common commodity parts.

## Architecture

The high-level architecture diagram is available here: [`design/architecture.puml`](design/architecture.puml:1).

Summary:

- Field Devices:
  - Weather Station (ESP32) — sensors: light (BH1750 / LDR), wind speed & direction, temperature/humidity (DHT22).
  - Actuator Device (ESP8266/ESP32) — motor/relay to open/close sunscreen with position feedback.
- Connectivity: MQTT (recommended: Mosquitto / Cloud MQTT) and optional HTTP/REST for APIs.
- Cloud / Backend: ingest, processing and dashboards. Devices publish telemetry to MQTT topics and subscribe to commands.

Refer to [`design/architecture.puml`](design/architecture.puml:1) for data flows and notes on security (MQTT over TLS, device auth).

## Subprojects

### Actuator

Location: [`Actuator/`](Actuator/:1)

Brief: Firmware for a sunscreen actuator with local motor control and position feedback. Uses PlatformIO (Arduino framework).

Quick start:

1. Open the project in VS Code with the PlatformIO extension.
2. Build:
   - pio run -e esp32dev
   - (or) platformio run -e esp32dev
3. Upload: pio run -t upload -e esp32dev
4. Serial monitor: pio device monitor -e esp32dev --baud 115200

Files of interest:

- [`Actuator/src/main.cpp`](Actuator/src/main.cpp:1)
- [`Actuator/include/ShadeController.h`](Actuator/include/ShadeController.h:1)
- [`Actuator/include/StepperController.h`](Actuator/include/StepperController.h:1)

Notes:

- Check [`Actuator/platformio.ini`](Actuator/platformio.ini:1) to adjust board/env.

### Weather Station

Location: [`weatherStation/`](weatherStation/:1)

Brief: ESP32-based weather station that reads multiple sensors and publishes telemetry over MQTT (or stores locally). Uses PlatformIO and several Adafruit libraries.

Quick start:

1. Open the project in VS Code with the PlatformIO extension.
2. Install dependencies automatically via PlatformIO (see lib_deps in [`weatherStation/platformio.ini`](weatherStation/platformio.ini:1)).
3. Build: pio run -e esp32dev
4. Upload: pio run -t upload -e esp32dev
5. Serial monitor: pio device monitor -e esp32dev --baud 115200

Files of interest:

- [`weatherStation/src/main.cpp`](weatherStation/src/main.cpp:1)
- [`weatherStation/include/README`](weatherStation/include/README:1)
- [`weatherStation/src/managers/DisplayManager.cpp`](weatherStation/src/managers/DisplayManager.cpp:1)

Notes:

- Secrets (Wi-Fi / MQTT credentials) are kept in [`weatherStation/src/secret.h`](weatherStation/src/secret.h:1). Do not commit sensitive credentials to public repos.

## Wiring & Bill of Materials (BOM)

- Wiring notes and diagrams: [`design/wiring.md`](design/wiring.md:1)
- BOM and LaTeX source: [`design/bom/bom_README.md`](design/bom/bom_README.md:1) and [`design/bom/bom.md`](design/bom/bom.md:1)

Build the BOM PDF:

pdflatex -interaction=nonstopmode -output-directory=design design/bom.tex

## Usage examples

Local testing (single device):

- Build and upload the firmware using the PlatformIO commands above.
- Open the serial monitor to view logs and telemetry.

MQTT example (topics/payload):

- Telemetry topic: devices/{device_id}/telemetry
- Actuator command topic: devices/{actuator_id}/actuator/command
- Example telemetry payload:

{
  "light": 123,
  "wind_speed": 1.7,
  "temp": 21.5,
  "humidity": 55.1
}

Example actuator command:

{
  "command": "set_position",
  "position": 75
}

## Development & Build

Recommended tools:

- VS Code with PlatformIO extension
- PlatformIO Core (pio) for CLI builds
- pdflatex (TeX Live / MiKTeX) for BOM PDF generation

PlatformIO common commands:

- Build: pio run -e esp32dev
- Upload: pio run -t upload -e esp32dev
- Monitor serial: pio device monitor -e esp32dev --baud 115200

## Authors / Contacts

- Project: Minor Smart Things
- Maintainer: Tigo Goes