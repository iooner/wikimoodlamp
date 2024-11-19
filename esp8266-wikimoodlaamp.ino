#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <Adafruit_NeoPixel.h>

// Configuration Wi-Fi
const char* ssid = "";
const char* password = "";

// Configuration MQTT
const char* mqtt_server = "";
const char* mqtt_login = "";
const char* mqtt_pass = "";

const char* mqtt_topic = "wikipedia/raw";


// Configuration des LEDs
int brightness = 120;
int fadesteps = 50;
int fadedelay = 10;
#define LED_PIN D2
#define NUM_LEDS 100
Adafruit_NeoPixel strip = Adafruit_NeoPixel(NUM_LEDS, LED_PIN, NEO_BGR + NEO_KHZ800);

WiFiClient espClient;
PubSubClient client(espClient);

void fadeToNewState(uint32_t targetColors[NUM_LEDS], int fadeSteps, int delayMs) {
  for (int step = 0; step <= fadeSteps; step++) {
    for (int i = 0; i < NUM_LEDS; i++) {
      uint32_t currentColor = strip.getPixelColor(i);
      uint8_t rCurrent = (currentColor >> 16) & 0xFF;
      uint8_t gCurrent = (currentColor >> 8) & 0xFF;
      uint8_t bCurrent = currentColor & 0xFF;

      uint32_t targetColor = targetColors[i];
      uint8_t rTarget = (targetColor >> 16) & 0xFF;
      uint8_t gTarget = (targetColor >> 8) & 0xFF;
      uint8_t bTarget = targetColor & 0xFF;

      uint8_t rNew = rCurrent + ((rTarget - rCurrent) * step / fadeSteps);
      uint8_t gNew = gCurrent + ((gTarget - gCurrent) * step / fadeSteps);
      uint8_t bNew = bCurrent + ((bTarget - bCurrent) * step / fadeSteps);

      strip.setPixelColor(i, strip.Color(rNew, gNew, bNew));
    }
    strip.show();
    delay(delayMs);
  }
}

void shiftLEDsWithFade(uint32_t newColor) {
  uint32_t targetColors[NUM_LEDS];
  for (int i = NUM_LEDS - 1; i > 0; i--) {
    targetColors[i] = strip.getPixelColor(i - 1);
  }
  targetColors[0] = newColor;
  fadeToNewState(targetColors, fadesteps, fadedelay);
}

void breathingEffect(uint32_t color, int cycles, int delayMs) {
  for (int cycle = 0; cycle < cycles; cycle++) {
    for (int step = 0; step <= 255; step += 5) {
      uint8_t r = ((color >> 16) & 0xFF) * step / 255;
      uint8_t g = ((color >> 8) & 0xFF) * step / 255;
      uint8_t b = (color & 0xFF) * step / 255;
      for (int i = 0; i < NUM_LEDS; i++) {
        strip.setPixelColor(i, strip.Color(r, g, b));
      }
      strip.show();
      delay(delayMs);
    }
    for (int step = 255; step >= 0; step -= 5) {
      uint8_t r = ((color >> 16) & 0xFF) * step / 255;
      uint8_t g = ((color >> 8) & 0xFF) * step / 255;
      uint8_t b = (color & 0xFF) * step / 255;
      for (int i = 0; i < NUM_LEDS; i++) {
        strip.setPixelColor(i, strip.Color(r, g, b));
      }
      strip.show();
      delay(delayMs);
    }
  }
}

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message reçu [");
  Serial.print(topic);
  Serial.print("] : ");
  String message;
  for (int i = 0; i < length; i++) {
    message += (char)payload[i];
  }
  Serial.println(message);

  uint32_t color;
  if (message == "plusminor") {
    color = strip.Color(180, 235, 0);
  } else if (message == "moinsminor") {
    color = strip.Color(180, 30, 0);
  } else if (message == "plus") {
    color = strip.Color(30, 255, 0);
  } else if (message == "moins") {
    color = strip.Color(255, 0, 0);
  } else if (message == "new") {
    color = strip.Color(255, 20, 147);
  } else if (message == "user") {
    color = strip.Color(30, 180, 255);
  } else if (message == "bot") {
    color = strip.Color(255, 165, 0);
  } else if (message == "vandal") {
    breathingEffect(strip.Color(255, 0, 0), 1, 25);
    strip.clear();
    strip.show();
    return;
  } else {
    Serial.println("Message non reconnu.");
    return;
  }
  shiftLEDsWithFade(color);
}

void reconnect() {
  while (!client.connected()) {
    Serial.print("Tentative de connexion au serveur MQTT...");
    String clientId = "ESP8266Client-";
    clientId += String(random(0xffff), HEX);
    if (client.connect(clientId.c_str(), mqtt_login, mqtt_pass)) {
      Serial.println("Connecté au serveur MQTT");
      client.subscribe(mqtt_topic);
    } else {
      Serial.print("Échec de connexion, statut : ");
      Serial.print(client.state());
      Serial.println(" - nouvelle tentative dans 5 secondes");
      delay(5000);
    }
  }
}

void setup_wifi() {
  delay(10);
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nWiFi connected");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}

void setup() {
  Serial.begin(115200);
  strip.begin();
  strip.show();
  strip.clear();
  strip.setBrightness(brightness); 
  Serial.println("LEDs initialisées");

  setup_wifi();
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();
}