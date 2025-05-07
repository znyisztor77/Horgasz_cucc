#include <HTTPClient.h> //ESP32
#include <WiFi.h> //ESP32
//#include <ESP8266WiFi.h>
//#include <ESP8266HTTPClient.h>


const char* ssid = "ESP32_AP";       // ESP32 Access Point neve
const char* password = "12345678";  // ESP32 Access Point jelszava

const char* serverUrl = "http://192.168.4.1/update"; // Az ESP32 IP-címe (alapértelmezett 192.168.4.1)
const int switchPin = 2; // Kapcsoló GPIO pinje

void setup() {
  Serial.begin(115200);
  pinMode(switchPin, INPUT_PULLUP);

  WiFi.begin(ssid, password);
  Serial.print("Csatlakozás az ESP32-höz...");
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
  }
  Serial.println("\nCsatlakozva!");
}

void loop() {
  static int prevState = HIGH;
  int currentState = digitalRead(switchPin);

  if (currentState != prevState) {
    prevState = currentState;

    if (WiFi.status() == WL_CONNECTED) {
      WiFiClient client;
      HTTPClient http;
      http.begin(client, serverUrl);

      String postData = "state=" + String(currentState);
      http.addHeader("Content-Type", "application/x-www-form-urlencoded");
      int httpCode = http.POST(postData);

      if (httpCode > 0) {
        Serial.printf("HTTP válaszkód: %d\n", httpCode);
      } else {
        Serial.printf("HTTP hiba: %s\n", http.errorToString(httpCode).c_str());
      }
      http.end();
    }
  }
  delay(100);
}
