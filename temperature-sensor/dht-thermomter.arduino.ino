// Example testing sketch for various DHT humidity/temperature sensors
// Written by ladyada, public domain

// REQUIRES the following Arduino libraries:
// - DHT Sensor Library: https://github.com/adafruit/DHT-sensor-library
// - ESP8266WiFi Library
// - PubSubClient Library: https://github.com/knolleary/pubsubclient

#include "DHT.h"
#include <ESP8266WiFi.h>
#include <PubSubClient.h>


// MQTT broket credentials and topic configuration
#define CLIENT_ID "<client id>"

#define MQTT_USER "<mqtt user>"
#define MQTT_PASSWORD "<mqtt password>"

#define TEMPERATURE_TOPIC "sensors/temperature/livingroom"
#define HUMIDITY_TOPIC "sensors/humidity/livingroom"

// Configure home network connection: board IP, gateway and subnet
IPAddress ip(192, 168, 1, 100); 
IPAddress gateway(192, 168, 1, 1); 
IPAddress subnet(255, 255, 255, 0);

// Configure your WiFi connection and MQTT broker 
const char* ssid = "<your wifi here>";
const char* password = "<wifi password>";
const char* mqtt_server = "<mqtt broker ip>";
const int mqtt_port = 1883;

// Digital pin connected to the DHT sensor (in my case, pin 14 (D5))
#define DHTPIN 14
// Type of sensor (sensor DHT22)
#define DHTTYPE DHT22

// Initialize DHT sensor.
DHT dht(DHTPIN, DHTTYPE);

// Initialize WiFi and MQTT clients
WiFiClient espClient;
PubSubClient client(espClient);

void setup() {
  Serial.begin(115200);
  setup_wifi();
  client.setServer(mqtt_server, mqtt_port);

  // Start DHT sensor
  dht.begin();
}

/** 
 * Function that connects to the configured WiFi network
 */
void setup_wifi() {
  delay(10);
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.config(ip, gateway, subnet);
  WiFi.mode(WIFI_STA); 
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

/** 
 * Try to reconnect to the MQTT broker in case of missing connection.
 * It tries each 5 seconds.
 */ 
void reconnect() {
  // Loop until we're reconnected
  digitalWrite(LED_BUILTIN, LOW);
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (client.connect(CLIENT_ID, MQTT_USER, MQTT_PASSWORD)) {
      Serial.println("connected");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

/**
 * Checks if the difference between two floats is less than
 * a defined threshold 
 */
bool checkBound(float newValue, float prevValue, float threshold) {
  return !isnan(newValue) &&
         (newValue < prevValue - threshold || newValue > prevValue + threshold);
}


// Initialize temp and humidity to negative values to force to read 
// (and publish) them at boot time
long lastMsg = 0;
float lastTemp = -1.0;
float lastHumidity = -1.0;

// Define the thresholds to publish a new temperature/humidity to avoid too much oscillation
float humidity_diff = 0.3;
float temperture_diff = 0.2;

/**
 * Read and publish temperature and humidity
 */
void loop() {

  // reconect to MQTT in case of lost connection
  if (!client.connected()) {
    reconnect();
  }

  client.loop();
  delay(10000);

  long now = millis();
  if (now - lastMsg > 30000) {
    lastMsg = now;
    
    // Reading temperature or humidity takes about 250 milliseconds!
    // Sensor readings may also be up to 2 seconds 'old' (its a very slow sensor)
    float currentHumidity = dht.readHumidity();
    // Read temperature as Celsius (the default)
    float currentTemp = dht.readTemperature();
  
    // Check if any reads failed and exit early (to try again).
    if (isnan(currentHumidity) || isnan(currentTemp)) {
      Serial.println(F("Failed to read from DHT sensor!"));
      return;
    }

    // When the variation of temp or humidity is more than the defined threshold
    // publish them and update old values
    if (checkBound(currentTemp, lastTemp, temperture_diff)) {
      lastTemp = currentTemp;
      Serial.print("Pushing temperature ");
      Serial.print(lastTemp);
      Serial.println("to temperature topic");
      client.publish(TEMPERATURE_TOPIC, String(lastTemp).c_str(), true);
    }

    if (checkBound(currentHumidity, lastHumidity, humidity_diff)) {
      lastHumidity = currentHumidity;
      Serial.print("Pushing humidity ");
      Serial.print(lastHumidity);
      Serial.println("to humidity topic");
      client.publish(HUMIDITY_TOPIC, String(lastHumidity).c_str(), true);
    }
  }
}
