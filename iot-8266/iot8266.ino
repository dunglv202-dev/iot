#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <DHT.h>

#define DHTTYPE DHT11
#define MIN_VALUE 25
#define MAX_VALUE 800

// Allow arduino access and manage port: sudo chmod a+rw /dev/ttyUSB0

// Devices
const char* LED_TOPIC = "esp8266/led";
const char* LED_STATE_TOPIC = "esp8266/led/state";
const int LED_PORT = D1;

const char* FAN_TOPIC = "esp8266/fan";
const char* FAN_STATE_TOPIC = "esp8266/fan/state";
const int FAN_PORT = D7;

const int WARNING_LED_PORT = D5;
const char* WARNING_LED_STATE_TOPIC = "esp8266/warning_led/state";

const char* DHT_TOPIC = "esp8266/dht";
const int DHT_PORT = D2;

// WiFi
const char *SSID = "dunglv";
const char *PASSWORD = "mmmmmmmmm";

// MQTT Broker
const char* MQTT_BROKER = "192.168.0.159";
const char* MQTT_USERNAME = "dung";
const char* MQTT_PASSWORD = "admin";
const int MQTT_PORT = 1883;

const int Analog_Pin = 0; // Analog pin A0

// Init
bool isLedOn = false;
bool isFanOn = false;
bool isWarning = false;

WiFiClient espClient;
PubSubClient mqttClient(espClient);
DHT dht(DHT_PORT, DHTTYPE);
unsigned long lastUpdate = millis();


void setup() {
    Serial.begin(9600);
    delay(2000);

    // Set LED, FAN pin as output, start DHT
    pinMode(LED_PORT, OUTPUT);
    digitalWrite(LED_PORT, LOW);
    pinMode(FAN_PORT, OUTPUT);
    digitalWrite(FAN_PORT, LOW);
    pinMode(WARNING_LED_PORT, OUTPUT);
    digitalWrite(WARNING_LED_PORT, LOW);
    dht.begin();

    // Setup connection
    setupWifiConnection();
    setupMqttBrokerConnection();
}

void loop() {
    if (!mqttClient.connected()) {
      setupMqttBrokerConnection();
    }

    if (isWarning) {
        // blink leds
        bool state = !isLedOn;
        setLedState(state);
        setFanState(state);
        setWarningLedState(state);
    }

    // handle message coming from subscribed topics
    mqttClient.loop();

    unsigned long now = millis();
    // publish data every 2 seconds
    if (now - lastUpdate >= 2000) {
        float temperature = dht.readTemperature();
        float humidity = dht.readHumidity();
        int lightSensorValue = analogRead(Analog_Pin);
        int lux = conversion(lightSensorValue);
        float dustLevel = random(70, 100);

        // update warning state
        Serial.println("Dust level: " + String(dustLevel));
        bool isNowWarning = dustLevel >= 80;
        if (!isWarning && isNowWarning) {
            isLedOn = false;
        } else if (isWarning && !isNowWarning) {
            // stop warning => reset (turn off all)
            setLedState(false);
            setFanState(false);
            setWarningLedState(false);
        }
        isWarning = isNowWarning;

        // publish sensor data
        String dhtPayload = "{\"temperature\":" + String(temperature) + ",\"humidity\":" + String(humidity) + ",\"lighting\":" + String(lux) + ",\"dustLevel\":" + String(dustLevel) + "}";
        char* buffer = new char[dhtPayload.length()+1];
        dhtPayload.toCharArray(buffer, dhtPayload.length()+1);
        mqttClient.publish(DHT_TOPIC, buffer);
        lastUpdate = now;
    }

    delay(500);
}

void setupWifiConnection() {
    WiFi.begin(SSID, PASSWORD);
    WiFi.setAutoReconnect(true);
    WiFi.persistent(true);
    Serial.println("Connecting to WiFi...");
    while (WiFi.status() != WL_CONNECTED) {
        delay(1000);
    }
    Serial.println("Wifi connected.");
}

void setupMqttBrokerConnection() {
    mqttClient.setServer(MQTT_BROKER, MQTT_PORT);
    mqttClient.setCallback(callback);
    while (!mqttClient.connected()) {
        Serial.println("Connecting to MQTT broker...");
        if (mqttClient.connect("iot-hw-esp8266", MQTT_USERNAME, MQTT_PASSWORD)) {
            Serial.println("MQTT broker connected.");
        } else {
            Serial.print("Failed with state ");
            Serial.println(mqttClient.state());
            delay(2000);
        }
    }
    mqttClient.subscribe(LED_TOPIC);
    mqttClient.subscribe(FAN_TOPIC);
}

int conversion(int sensorValue) {
    return 1024 - sensorValue;
}

void setLedState(bool isOn) {
    digitalWrite(LED_PORT, isOn ? HIGH : LOW);
    isLedOn = isOn;
    mqttClient.publish(LED_STATE_TOPIC, isOn ? "on" : "off");
}

void handleLedCommand(String command) {
    if (isWarning) return;
    if (command == "on" && !isLedOn) {
        setLedState(true);
    } else if (command == "off" && isLedOn) {
        setLedState(false);
    }
}

void setFanState(bool isOn) {
    digitalWrite(FAN_PORT, isOn ? HIGH : LOW);
    isFanOn = isOn;
    mqttClient.publish(FAN_STATE_TOPIC, isOn ? "on" : "off");
}

void handleFanCommand(String command) {
    if (isWarning) return;
    if (command == "on" && !isFanOn) {
        setFanState(true);
    } else if (command == "off" && isFanOn) {
        setFanState(false);
    }
}

void setWarningLedState(bool isOn) {
    digitalWrite(WARNING_LED_PORT, isOn ? HIGH : LOW);
    mqttClient.publish(WARNING_LED_STATE_TOPIC, isOn ? "on" : "off");
}

void callback(char* topic, byte* payload, unsigned int length) {
    Serial.print("Message arrived in topic: ");
    Serial.println(topic);
    Serial.println(topic == LED_TOPIC);
    String message;
    for (int i = 0; i < length; i++) {
        message += (char) payload[i];  // Convert *byte to string
    }
    Serial.print("Message: ");
    Serial.println(message);

    if (strcmp(topic, LED_TOPIC) == 0) {
        handleLedCommand(message);
    } else if (strcmp(topic, FAN_TOPIC) == 0) {
        handleFanCommand(message);
    }

    Serial.println();
    Serial.println("-----------------------");
}