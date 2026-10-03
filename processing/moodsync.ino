/*
  MoodSync — mood-adaptive smart room automation
  3707ICT Automation and IoT, Group Project

  Perception layer:  DHT22 (temp/humidity), PIR (motion)
  Processing layer:  ESP32 - this sketch
  Network layer:     MQTT over Wi-Fi, Adafruit IO broker
  Application layer: Adafruit IO dashboard

  Pin assignment (see report Section 3.2):
    GPIO 32  DHT22 data        (ADC1, sensor)
    GPIO 33  PIR output        (ADC1, sensor)
    GPIO 13  RGB LED red       (Safe Zone, actuator)
    GPIO 14  RGB LED green     (Safe Zone, actuator)
    GPIO 25  RGB LED blue      (Safe Zone, actuator)
    GPIO 26  Relay IN          (Safe Zone, actuator)
    GPIO 27  Buzzer            (Safe Zone, actuator)

  RGB LED is common cathode — no inversion needed.
*/

#include <WiFi.h>
#include <Adafruit_MQTT.h>
#include <Adafruit_MQTT_Client.h>
#include <DHT.h>
#include "secrets.h"

// ---------- Pins ----------
#define DHTPIN      32
#define DHTTYPE     DHT22
#define PIRPIN      33
#define LED_RED     13
#define LED_GREEN   14
#define LED_BLUE    25
#define RELAYPIN    26
#define BUZZERPIN   27

// ---------- Thresholds (Section 4) ----------
#define HEAT_BLOCK_ON    26.0   // Rule 2: block heating at/above this
#define HEAT_BLOCK_OFF   24.0   // Rule 2: re-enable heating below this
#define VENT_ON          70.0   // Rule 4: engage ventilation at/above this
#define VENT_OFF         65.0   // Rule 4: release ventilation below this
#define AWAY_TIMEOUT_MS  (5UL * 60UL * 1000UL)  // Rule 3: 5 minutes no motion

// ---------- Timing ----------
#define SENSOR_READ_INTERVAL_MS   2000
#define SENSOR_PUBLISH_INTERVAL_MS 5000

DHT dht(DHTPIN, DHTTYPE);

// ---------- Wi-Fi + MQTT ----------
WiFiClient client;
Adafruit_MQTT_Client mqtt(&client, "io.adafruit.com", 1883, IO_USERNAME, IO_KEY);

Adafruit_MQTT_Subscribe emotionFeed = Adafruit_MQTT_Subscribe(&mqtt, IO_USERNAME "/feeds/emotion");
Adafruit_MQTT_Publish   temperatureFeed = Adafruit_MQTT_Publish(&mqtt, IO_USERNAME "/feeds/temperature");
Adafruit_MQTT_Publish   humidityFeed    = Adafruit_MQTT_Publish(&mqtt, IO_USERNAME "/feeds/humidity");
Adafruit_MQTT_Publish   statusFeed      = Adafruit_MQTT_Publish(&mqtt, IO_USERNAME "/feeds/status");

// ---------- State (Section 4.3 - state memory) ----------
String currentMood = "calm";      // last mood selected
bool heatingBlocked = false;      // Rule 2 override state
bool ventilationOverride = false; // Rule 4 override state
bool awayMode = false;
unsigned long lastMotionMillis = 0;

float lastTemp = 0;
float lastHum = 0;
unsigned long lastSensorRead = 0;
unsigned long lastSensorPublish = 0;

String lastPublishedStatus = "";

// ---------- Setup ----------
void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("MoodSync starting...");

  pinMode(PIRPIN, INPUT);
  pinMode(LED_RED, OUTPUT);
  pinMode(LED_GREEN, OUTPUT);
  pinMode(LED_BLUE, OUTPUT);
  pinMode(RELAYPIN, OUTPUT);
  pinMode(BUZZERPIN, OUTPUT);

  dht.begin();
  setColor(0, 0, 0);
  digitalWrite(RELAYPIN, LOW);
  digitalWrite(BUZZERPIN, LOW);

  lastMotionMillis = millis(); // assume occupied at boot

  connectWiFi();
  mqtt.subscribe(&emotionFeed);
}

// ---------- Main loop ----------
void loop() {
  connectMQTT();
  if (!mqtt.ping()) {
    mqtt.disconnect();
  }

  checkEmotionSelection();

  if (millis() - lastSensorRead >= SENSOR_READ_INTERVAL_MS) {
    lastSensorRead = millis();
    readSensors();
    checkPIR();
    applyHeatingOverride();   // Rule 2
    applyVentilationOverride(); // Rule 4
    applyAwayState();        // Rule 3
    applyLighting();         // Rule 1 (lighting always reflects mood unless Away)
    publishStatusIfChanged();
  }

  if (millis() - lastSensorPublish >= SENSOR_PUBLISH_INTERVAL_MS) {
    lastSensorPublish = millis();
    publishSensorReadings();
  }
}

// ---------- Wi-Fi ----------
void connectWiFi() {
  Serial.print("Connecting to Wi-Fi");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWi-Fi connected, IP: " + WiFi.localIP().toString());
}

// ---------- MQTT ----------
void connectMQTT() {
  if (mqtt.connected()) return;

  Serial.print("Connecting to Adafruit IO MQTT...");
  int8_t ret;
  while ((ret = mqtt.connect()) != 0) {
    Serial.println(mqtt.connectErrorString(ret));
    mqtt.disconnect();
    delay(3000);
  }
  Serial.println("MQTT connected.");
}

// ---------- Rule 1: emotion selection -> lighting/heating mapping ----------
void checkEmotionSelection() {
  Adafruit_MQTT_Subscribe *subscription;
  while ((subscription = mqtt.readSubscription(0))) {
    if (subscription == &emotionFeed) {
      String value = String((char *)emotionFeed.lastread);
      value.toLowerCase();
      if (value == "calm" || value == "energise" || value == "cosy" || value == "focus") {
        currentMood = value;
        Serial.println("Mood selected: " + currentMood);
        beepConfirm();
        applyLighting();
        publishStatusIfChanged();
      }
    }
  }
}

// Maps mood -> RGB colour and whether it wants heating/cooling.
// Colour values are illustrative; final values should be tuned once
// hardware is running, this mapping is not the dashboard button colours,
// it is the actual room-light output, chosen for effect, not UI matching.
void applyLighting() {
  if (awayMode) {
    setColor(0, 0, 0);
    return;
  }

  if (currentMood == "calm") {
    setColor(60, 90, 140);        // soft, dim blue-white
  } else if (currentMood == "energise") {
    setColor(180, 220, 255);      // bright, cool white
  } else if (currentMood == "cosy") {
    setColor(255, 120, 40);       // warm amber-orange
  } else if (currentMood == "focus") {
    setColor(200, 200, 200);      // neutral bright white
  }
}

bool wantsHeating() {
  return currentMood == "cosy";
}

bool wantsCooling() {
  return currentMood == "calm" || currentMood == "energise";
}

// ---------- Rule 2: heating override with hysteresis + state memory ----------
void applyHeatingOverride() {
  if (!heatingBlocked && lastTemp >= HEAT_BLOCK_ON) {
    heatingBlocked = true;
    Serial.println("Rule 2: heating blocked, temp " + String(lastTemp, 1) + "C");
  } else if (heatingBlocked && lastTemp < HEAT_BLOCK_OFF) {
    heatingBlocked = false;
    Serial.println("Rule 2: heating re-enabled, temp " + String(lastTemp, 1) + "C");
  }
  updateRelay();
}

// ---------- Rule 4: humidity override with hysteresis + state memory ----------
void applyVentilationOverride() {
  if (!ventilationOverride && lastHum >= VENT_ON) {
    ventilationOverride = true;
    Serial.println("Rule 4: ventilation engaged, humidity " + String(lastHum, 0) + "%");
  } else if (ventilationOverride && lastHum < VENT_OFF) {
    ventilationOverride = false;
    Serial.println("Rule 4: ventilation released, humidity " + String(lastHum, 0) + "%");
  }
  updateRelay();
}

// Single relay, shared between heating and ventilation.
// Ventilation always takes priority over heating (Section 4.3):
// humidity affects comfort regardless of mood, heating is only
// ever relevant to Cosy specifically.
void updateRelay() {
  if (awayMode) {
    digitalWrite(RELAYPIN, LOW);
    return;
  }
  if (ventilationOverride) {
    digitalWrite(RELAYPIN, HIGH);   // relay ON, functioning as ventilation
  } else if (wantsHeating() && !heatingBlocked) {
    digitalWrite(RELAYPIN, HIGH);   // relay ON, functioning as heating
  } else {
    digitalWrite(RELAYPIN, LOW);
  }
}

// ---------- Rule 3: occupancy-based Away state ----------
void checkPIR() {
  if (digitalRead(PIRPIN) == HIGH) {
    if (awayMode) {
      Serial.println("Motion detected, returning from Away.");
    }
    lastMotionMillis = millis();
    awayMode = false;
  }
}

void applyAwayState() {
  if (!awayMode && (millis() - lastMotionMillis >= AWAY_TIMEOUT_MS)) {
    awayMode = true;
    Serial.println("Rule 3: no motion 5+ min, entering Away state.");
  }
  if (awayMode) {
    setColor(0, 0, 0);
    digitalWrite(RELAYPIN, LOW);
  } else {
    applyLighting();
    updateRelay();
  }
}

// ---------- Sensors ----------
void readSensors() {
  float t = dht.readTemperature();
  float h = dht.readHumidity();
  if (!isnan(t)) lastTemp = t;
  if (!isnan(h)) lastHum = h;
}

void publishSensorReadings() {
  if (!isnan(lastTemp)) temperatureFeed.publish(lastTemp);
  if (!isnan(lastHum)) humidityFeed.publish(lastHum);
}

// ---------- Status string (see report Section 6) ----------
void publishStatusIfChanged() {
  String status = buildStatusString();
  if (status != lastPublishedStatus) {
    statusFeed.publish(status.c_str());
    lastPublishedStatus = status;
    Serial.println("Status: " + status);
  }
}

String buildStatusString() {
  String mood = currentMood;
  mood.setCharAt(0, toupper(mood.charAt(0)));

  if (awayMode) {
    return "Away · all actuators off - no motion 5+ min";
  }
  if (ventilationOverride) {
    return mood + " · lighting only, ventilation override — humidity " + String(lastHum, 0) + "%";
  }
  if (wantsHeating() && heatingBlocked) {
    return mood + " · lighting only - heating blocked, room already warm (" + String(lastTemp, 1) + "C)";
  }
  if (wantsHeating()) {
    return mood + " · lighting + heating active";
  }
  if (wantsCooling()) {
    return mood + " · lighting + cooling active";
  }
  return mood + " · lighting active";
}

// ---------- Actuator helpers ----------
void setColor(int r, int g, int b) {
  analogWrite(LED_RED, r);
  analogWrite(LED_GREEN, g);
  analogWrite(LED_BLUE, b);
}

void beepConfirm() {
  digitalWrite(BUZZERPIN, HIGH);
  delay(150);
  digitalWrite(BUZZERPIN, LOW);
}
