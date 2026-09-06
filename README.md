# MoodSync

A mood-adaptive smart room automation system - built for **3707ICT: Automation and IoT** (Griffith University, Group Project, 70%).

Select an emotional state — **Calm**, **Energize**, **Cozy**, or **Focus** — from a cloud dashboard, and MoodSync automatically adjusts room lighting and heating/cooling to match. The system uses edge intelligence to override unsafe or wasteful choices (e.g. it won't turn on heating if the room is already hot), and automatically reverts to an energy-saving state when the room is empty.

## Team

- **Jasmine [Surname]** - Perception & Processing layers (sensors, local automation rules, edge override logic)
- **Youssef [Surname]** - Network & Application layers (Wi-Fi/MQTT setup, Adafruit IO dashboard)

## Architecture

MoodSync implements the four-layer IoT reference model:

| Layer | Components |
|---|---|
| **Perception** | DHT22 (temperature & humidity), PIR (motion) → RGB LED, relay, active buzzer |
| **Processing** | ESP32 — local automation rules + edge override logic |
| **Network** | MQTT over Wi-Fi, star topology |
| **Application** | Adafruit IO dashboard — emotion selector, live readings, historical charts |

## Hardware

- ESP32 dev board
- DHT22 temperature/humidity sensor
- PIR (HC-SR501) motion sensor
- RGB LED / WS2812B strip (PWM-controlled mood lighting)
- Relay module (heater/fan proxy)
- Active buzzer (mode-confirmation alert)

## Automation rules

1. **Emotion selection** (dashboard → MQTT) → lighting/heating mapping
2. **Environmental override** - blocks heating if the room is already hot, even if "Cozy" is selected
3. **Occupancy auto-off** - PIR detects an empty room → reverts to an "Away" state, everything off

## Repository structure

```
/perception     - DHT22 + PIR sensor code
/processing     - ESP32 automation rules, edge override logic
/network        - Wi-Fi + MQTT setup
/application    - Adafruit IO feed/dashboard config
/docs           - architecture diagram, wiring diagram, pin assignment table
/hardware       - Wokwi project files, breadboard references
```

## Setup

1. Clone this repo.
2. Copy `secrets_example.h` to `secrets.h` and fill in your own Wi-Fi and Adafruit IO credentials. **`secrets.h` is git-ignored - never commit real credentials.**
3. Open the project in Arduino IDE / PlatformIO, select the ESP32 board, and flash.
4. Set up an Adafruit IO account and create feeds matching the topics in `/network`.

## Security

MQTT authentication is handled via Adafruit IO username/key (see `secrets_example.h`). Credentials are never hardcoded or committed - see `.gitignore`.

## Course

3707ICT — Automation and IoT, Griffith University
Convenor: A/Prof Jun Jo · Tutor: Hung Vu

## Report & demo

- Final report: see `/docs`
- Demo video:
