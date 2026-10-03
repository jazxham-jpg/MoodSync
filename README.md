# MoodSync

A mood-adaptive smart room automation system, built for 3707ICT: Automation and IoT (Griffith University, Group Project, 70%).

Select an emotional state (Calm, Energise, Cozy, or Focus) from a cloud dashboard, and MoodSync automatically adjusts room lighting and heating/cooling to match. The system uses edge intelligence to override unsafe or wasteful choices (it won't turn on heating if the room is already hot), and automatically reverts to an energy-saving state when the room is empty.

## Team

- **Jasmine Hamouda**: Perception & Processing layers (sensors, local automation rules, edge override logic)
- **Youssef El-Samman**: Network & Application layers (Wi-Fi/MQTT setup, Adafruit IO dashboard)

## Architecture

MoodSync implements the four-layer IoT reference model:

| Layer | Components |
|---|---|
| Perception | DHT22 (temperature & humidity), PIR (motion), RGB LED, relay, active buzzer |
| Processing | ESP32, running local automation rules and edge override logic |
| Network | MQTT over Wi-Fi, star topology |
| Application | Adafruit IO dashboard, emotion selector, live readings, historical charts |

## Hardware

- ESP32 dev board
- DHT22 temperature/humidity sensor
- PIR (HC-SR501) motion sensor
- RGB LED (PWM-controlled mood lighting)
- Relay module (heater/fan proxy)
- Active buzzer (mode-confirmation alert)

## Required libraries

Install these via the Arduino IDE Library Manager (or Wokwi's Library Manager if running in-browser):

- **DHT sensor library** (Adafruit): reads temperature and humidity from the DHT22
- **Adafruit Unified Sensor**: dependency required by the DHT library
- **Adafruit MQTT Library**: handles MQTT publish/subscribe to Adafruit IO
- **WiFi.h**: built into the ESP32 board package, no separate install needed

## Automation rules

1. **Emotion selection** (dashboard to MQTT), maps to a lighting/heating preset
2. **Environmental override**: blocks heating if the room is already hot, even if "Cozy" is selected
3. **Occupancy auto-off**: PIR detects an empty room and reverts to an "Away" state, everything off
4. **Ventilation override**: humidity above threshold triggers ventilation regardless of selected mood

## Repository contents

/processing/moodsync.ino - ESP32 sketch (all automation logic)
/processing/secrets_example.h - credentials template
/hardware/diagram.json - Wokwi circuit diagram
moodsync_demo.mp4 - demo video


## Setup

1. Clone this repo.
2. Copy `processing/secrets_example.h` to wokwi and fill in your own credentials.
3. Install the required libraries listed above.
4. Open `processing/moodsync.ino` in Arduino IDE, or load `hardware/diagram.json` directly in Wokwi, select the ESP32 board, and flash/run.
5. Set up an Adafruit IO account and create feeds matching the topics the sketch publishes to (`emotion`, `temperature`, `humidity`, `status`).

## Security

MQTT authentication is handled via Adafruit IO username/key (see `secrets_example.h`). Real credentials are never hardcoded or committed to this repository.

## Course

3707ICT: Automation and IoT, Griffith University
Convenor: A/Prof Jun Jo. Tutor: Hung Vu

## Demo

[moodsync_demo.mp4](./moodsync_demo.mp4)
