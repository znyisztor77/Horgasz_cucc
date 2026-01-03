#include <HX711_ADC.h>
#include <FastLED.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include "Arduino.h"
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define NUM_LEDS 1
#define DATA_PIN 48 //19

// OLED display
#define I2C_SDA 1 //21
#define I2C_SCL 2 //22
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 32
#define OLED_RESET -1

//A panel led beállítása
CRGB leds[NUM_LEDS];
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// WiFi beállítások
const char* ssid = "ESP32_AP";
const char* password = "12345678";
String vevoIP = "192.168.4.1";
String serverUrl = "http://192.168.4.1/ping";

// HX711 súlymérő
const int HX711_dout = 3; //2
const int HX711_sck = 4; //4
HX711_ADC LoadCell(HX711_dout, HX711_sck);

// Gombok
const int tareButton = 5; //5
const int testButton = 6; //18

// Állapot változók
const float SULY_KUSZOB = 5.0;
bool sulyAllapot = false;
bool elozo_sulyAllapot = false;
float suly = 0;
int readWeight = 0;

//Gomb állapotok kezelése
bool gombAllapot = false;
bool elozo_gombAllapot = false;
bool kombinaltAllapot = false;
bool elozo_kombinaltAllapot = false;

// Időzítések
unsigned long utolsoPing = 0;
unsigned long utolsoKijelzoFrissites = 0;
unsigned long utolsoWiFiProba = 0;
const unsigned long PING_INTERVALLUM = 500;
const unsigned long KIJELZO_FRISSITES = 500;  // 500ms-enként frissül a kijelző
const unsigned long WIFI_UJRACSATLAKOZAS = 10000;  // 10mp-enként próbál újracsatlakozni

//Wifi kapcsolat 
bool wifiConnected = false;
int wifiReconnectAttempts = 0;
const int MAX_RECONNECT_ATTEMPTS = 3;

void setup() {
  Serial.begin(115200);
  Wire.begin(I2C_SDA, I2C_SCL);
  FastLED.addLeds<NEOPIXEL, DATA_PIN>(leds, NUM_LEDS);
  
  pinMode(tareButton, INPUT_PULLUP);
  pinMode(testButton, INPUT_PULLUP);

  // OLED inicializálás
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("OLED inicializálás sikertelen!"));
    while (1);
  }
  display.clearDisplay();
  display.setRotation(2); //A kijelző elforgatása, ha nincs forgatásra szükség,akkor csak kommentelni kell
  display.setTextColor(WHITE);
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println("Inicializalas...");
  display.display();

  // HX711 inicializálás
  Serial.println("HX711 inicializálása...");
  LoadCell.begin();
  float calibrationValue = 696.0;
  unsigned long stabilizingtime = 2000;
  boolean _tare = true;
  
  LoadCell.start(stabilizingtime, _tare);
  
  if (LoadCell.getTareTimeoutFlag()) {
    Serial.println("HX711 Timeout hiba!");
    display.clearDisplay();
    display.setCursor(0, 0);
    display.println("HX711 HIBA!");
    display.display();
    //while (1); // Ha ez benne van meg akasztja a teljes kódot.
  } else {
    LoadCell.setCalFactor(calibrationValue);
    Serial.println("HX711 inicializálva");
  }

  // WiFi csatlakozás (nem blokkoló)
  wifiCsatlakozas();
  
  Serial.println("Rendszer kész!");
}

void loop() {
  unsigned long currentMillis = millis();
  
  // WiFi állapot kezelése
  kezelWiFi(currentMillis);
  
  // Gomb olvasása
  gombAllapot = !digitalRead(testButton);
  
  // Súly mérése (a HX711_ADC könyvtár nem blokkoló)
  static boolean newDataReady = 0;
  
  if (LoadCell.update()) {
    newDataReady = true;
  }
  
  if (newDataReady) {
    suly = LoadCell.getData();
    readWeight = (int)suly;
    sulyAllapot = (suly > SULY_KUSZOB);
    
    // Állapotváltozás detektálása
    if (sulyAllapot != elozo_sulyAllapot) {
      Serial.print("Súly: ");
      Serial.print(suly);
      Serial.print(" g - Állapot: ");
      Serial.println(sulyAllapot ? "AKTÍV" : "INAKTÍV");
      elozo_sulyAllapot = sulyAllapot;
    }
    
    newDataReady = false;
  }
  
  // Tare gomb kezelése
  if (Serial.available() > 0) {
    char inByte = Serial.read();
    if (inByte == 't') LoadCell.tareNoDelay();
    Serial.println("Tare indítva...");
  }
  
  if (digitalRead(tareButton) == LOW) {
    LoadCell.tareNoDelay();
    Serial.println("Tare indítva...");
  }
  
  if (LoadCell.getTareStatus() == true) {
    Serial.println("Tare kész");
  }
  
  // Kombinált állapot
  kombinaltAllapot = gombAllapot || sulyAllapot;
  
  // Állapotváltozás esetén azonnali ping
  if (kombinaltAllapot != elozo_kombinaltAllapot) {
    elozo_kombinaltAllapot = kombinaltAllapot;
    
    Serial.print(">>> ÁLLAPOTVÁLTOZÁS: ");
    if (gombAllapot && sulyAllapot) {
      Serial.println("GOMB ÉS SÚLY AKTÍV");
    } else if (gombAllapot) {
      Serial.println("GOMB AKTÍV");
    } else if (sulyAllapot) {
      Serial.println("SÚLY AKTÍV");
    } else {
      Serial.println("INAKTÍV");
    }
    
    if (wifiConnected) {
      pingKuldes();
    }
  }
  
  // Rendszeres ping
  if (wifiConnected && (currentMillis - utolsoPing > PING_INTERVALLUM)) {
    pingKuldes();
  }
  
  // Kijelző frissítése
  if (currentMillis - utolsoKijelzoFrissites > KIJELZO_FRISSITES) {
    utolsoKijelzoFrissites = currentMillis;
    frissitKijelzo();
  }
  
  delay(10);  // Rövid delay a stabilitásért
}

void wifiCsatlakozas() {
  Serial.println("WiFi csatlakozás...");
  
  // WiFi power beállítások az ESP32-hez
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.persistent(true);
  
  WiFi.begin(ssid, password);
  
  int proba = 0;
  while (WiFi.status() != WL_CONNECTED && proba < 20) {
    delay(300);
    Serial.print(".");
    proba++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    wifiConnected = true;
    wifiReconnectAttempts = 0;
    Serial.println("\nWiFi csatlakozva!");
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());
    Serial.print("RSSI: ");
    Serial.println(WiFi.RSSI());
  } else {
    wifiConnected = false;
    Serial.println("\nWiFi csatlakozás sikertelen");
  }
}

void kezelWiFi(unsigned long currentMillis) {
  // Ellenőrizzük a WiFi állapotot
  if (WiFi.status() != WL_CONNECTED) {
    if (wifiConnected) {
      // Kapcsolat megszakadt
      wifiConnected = false;
      Serial.println("WiFi kapcsolat megszakadt!");
    }
    
    // Próbálunk újracsatlakozni
    if (currentMillis - utolsoWiFiProba > WIFI_UJRACSATLAKOZAS) {
      utolsoWiFiProba = currentMillis;
      
      if (wifiReconnectAttempts < MAX_RECONNECT_ATTEMPTS) {
        wifiReconnectAttempts++;
        Serial.print("WiFi újracsatlakozás kísérlet ");
        Serial.print(wifiReconnectAttempts);
        Serial.print("/");
        Serial.println(MAX_RECONNECT_ATTEMPTS);
        
        WiFi.disconnect();
        delay(100);
        WiFi.begin(ssid, password);
      }
    }
  } else {
    if (!wifiConnected) {
      // Kapcsolat helyreállt
      wifiConnected = true;
      wifiReconnectAttempts = 0;
      Serial.println("WiFi kapcsolat helyreállt!");
    }
  }
}

void frissitKijelzo() {
  display.clearDisplay();
  display.setRotation(2);
  display.setTextSize(1);
  display.setCursor(0, 0);
  
  // WiFi állapot
  if (wifiConnected) {
    display.print("WiFi: OK (");
    display.print(WiFi.RSSI());
    display.println("dBm)");
  } else {
    display.println("WiFi: NINCS");
  }
  
  // Súly megjelenítése
  display.print("Suly: ");
  display.print(suly, 1);
  display.println(" g");
  
  // Állapot
  display.print("Allapot: ");
  if (kombinaltAllapot) {
    display.println("AKTIV");
  } else {
    display.println("INAKTIV");
  }
  
  display.display();
  
  // LED frissítése
  if (wifiConnected) {
    if (kombinaltAllapot) {
      leds[0] = CRGB::Green;  // Zöld - kapcsolódva és aktív
    } else {
      leds[0] = CRGB::Blue;   // Kék - kapcsolódva, inaktív
    }
  } else {
    leds[0] = CRGB::Red;      // Piros - nincs kapcsolat
  }
  FastLED.show();
}

void pingKuldes() {
  if (!wifiConnected) return;
  
  utolsoPing = millis();
  
  HTTPClient http;
  http.setTimeout(2000);  // 2 másodperc timeout
  
  String url = serverUrl + "?button=" + String(kombinaltAllapot ? "1" : "0");
  http.begin(url);
  
  int httpCode = http.GET();
  
  if (httpCode > 0) {
    if (httpCode == HTTP_CODE_OK) {
      Serial.println("Ping OK");
    }
  } else {
    Serial.print("Ping hiba: ");
    Serial.println(http.errorToString(httpCode).c_str());
  }
  
  http.end();
}