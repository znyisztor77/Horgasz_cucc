#include <FastLED.h>

#include <HTTPClient.h>  //ESP32
#include <WiFi.h>        //ESP32
//#include <ESP8266WiFi.h>
//#include <ESP8266HTTPClient.h>
#include "Arduino.h"
#include "HX711.h"
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <SimpleTimer.h>

#define NUM_LEDS 1
#define DATA_PIN 48

#define HX711_DT 3
#define HX711_SCK 4
// for the OLED display
#define I2C_SDA 1
#define I2C_SCL 2
#define SCREEN_WIDTH 128  // OLED display width, in pixels
#define SCREEN_HEIGHT 32  // OLED display height, in pixels
// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
#define OLED_RESET -1  // Reset pin # (or -1 if sharing Arduino reset pin)
CRGB leds[NUM_LEDS];
HX711 scale(HX711_DT, HX711_SCK);
SimpleTimer timer;
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

const char* ssid = "ESP32_AP";      // ESP32 Access Point neve
const char* password = "12345678";  // ESP32 Access Point jelszava
String vevoIP = "192.168.4.1";
//const char* serverUrl = "http://192.168.4.1/update";  // Az ESP32 IP-címe (alapértelmezett 192.168.4.1)
String serverUrl = "http://192.168.4.1/ping";


String myString;  // Serial-ra kiírandó üzenet összeállítása.
String cmessage;  // Összeállított Serial üzenet
char buff[10];
float weight;
int readWeight;
float calibration_factor = 20;  // for me this vlaue works just perfect 1500
const int tareButton = 5;       // tare gomb, súlymérő nullázása

const float SULY_KUSZOB = 5.0;  // 5 gramm küszöbérték
bool sulyAllapot = false;
bool elozo_sulyAllapot = false;

/*static int prevState;
int currentState;*/

const int testButton = 6;  // ha nem működik a súlymérő akkor ezzel is lehet tesztelni.
bool gombAllapot = false;
bool elozo_gombAllapot = false;
// Kombinált állapot (gomb VAGY súly)
bool kombinaltAllapot = false;
bool elozo_kombinaltAllapot = false;


unsigned long utolsoPing = 0;
const unsigned long PING_INTERVALLUM = 500;  // 500 ms - gyorsabb frissítés a gomb miatt


void setup() {

  Serial.begin(115200);
  Wire.begin(I2C_SDA, I2C_SCL);
  FastLED.addLeds<NEOPIXEL, DATA_PIN>(leds, NUM_LEDS);
  Serial.print("Eszköz elindult.");

  pinMode(tareButton, INPUT_PULLUP);
  pinMode(testButton, INPUT_PULLUP);

  // HX711 inicializálása
  scale.begin(HX711_DT, HX711_SCK);
  if (scale.is_ready()) {
    Serial.println("HX711 készen áll");
    scale.set_scale(calibration_factor);
    scale.tare();  // Nullázás
    Serial.println("Súlymérő nullázva");
  } else {
    Serial.println("HIBA: HX711 nem elérhető!");
  }
  scale.set_scale();
  scale.tare();                             //Reset the scale to 0
  long zero_factor = scale.read_average();  //Get a baseline reading

  //WiFi csatlakozás
  WiFi.begin(ssid, password);
  int proba = 0;
  Serial.print("Csatlakozás az ESP32-höz...");
  while (WiFi.status() != WL_CONNECTED && proba < 20) {
    delay(500);
    Serial.print(".");
    proba++;
  }
  //Serial.println("\nCsatlakozva!");
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi kapcsolat létrejött!");
    Serial.print("IP cím: ");
    Serial.println(WiFi.localIP());
    //Serial.print("Vevő IP: ");
    //Serial.println(vevoIP);
  } else {
    Serial.println("\nNem sikerült csatlakozni a WiFi-hez!");
  }

  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  timer.setInterval(1000L, displayData);
  //displayData();
  display.clearDisplay();
  display.setTextColor(WHITE);
}

void loop() {

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi kapcsolat megszakadt, újracsatlakozás...");
    WiFi.begin(ssid, password);
    delay(5000);
    return;
  }
  timer.run();  // Initiates SimpleTimer a kijelző miatt kell

  gombAllapot = !digitalRead(testButton);

  // Súly mérése
  /*if (scale.is_ready()) {
    float suly = scale.get_units(15); // 15 mérés átlaga
    readWeight = (int)suly;  // olvasott érték Egész számmá alakítás a kijelzőn megjelenítés miatt
    
    // Súly alapú állapot meghatározása
    sulyAllapot = (suly > SULY_KUSZOB);
    
    // Debug kiírás
    if (sulyAllapot != elozo_sulyAllapot) {
      Serial.print("Súly: ");
      Serial.print(suly);
      Serial.print(" g - Állapot: ");
      Serial.println(sulyAllapot ? "AKTÍV (>5g)" : "INAKTÍV (<=5g)");
      elozo_sulyAllapot = sulyAllapot;
    }
  }*/


  float suly = scale.get_units(10);  // 15 mérés átlaga
  readWeight = (int)suly;            // olvasott érték Egész számmá alakítás a kijelzőn megjelenítés miatt

  // Súly alapú állapot meghatározása
  sulyAllapot = (suly > SULY_KUSZOB);

  // Debug kiírás
  if (sulyAllapot != elozo_sulyAllapot) {
    Serial.print("Súly: ");
    Serial.print(suly);
    Serial.print(" g - Állapot: ");
    Serial.println(sulyAllapot ? "AKTÍV (>5g)" : "INAKTÍV (<=5g)");
    elozo_sulyAllapot = sulyAllapot;
  }


  // Kombinált állapot: gomb VAGY súly (ha bármelyik aktív, akkor aktív)
  kombinaltAllapot = gombAllapot || sulyAllapot;

  // Ha megváltozott a kombinált állapot, azonnal küldjünk frissítést
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

    // Azonnali ping küldése
    pingKuldes();
  }

  /* Ez volt az első. Ezzel működött a súlymérés kapcsolása
  scale.set_scale(calibration_factor);     //Adjust to this calibration factor
  weight = scale.get_units(15);            //súly mérés,  15 mérés átlaga  
  myString = dtostrf(weight, 3, 3, buff);  // double to string (float) with format 
  cmessage = cmessage + "Weight" + ":" + myString + ","; // Serial szöveg összefűzése
  Serial.println(cmessage);
  cmessage = "";
  Serial.println();  
  readWeight = (int)weight;  // olvasott érték Egész számmá alakítás
  Serial.println("Olvasott érték: " + String(readWeight));
  if (readWeight >= 5 || digitalRead(testButton) == LOW) {
    Serial.println("Nagyobb mint öt/ Gomb benyomva!!!!");
    prevState = true;
    currentState = false;
    pingKuldes();
  } else {
    currentState = true;
    prevState = false;
    Serial.println("Kisebb mint öt/ Gomb elengedve!!!!");
  }
  Serial.println("Current állapot: " + String(currentState));
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
*/

  myString = dtostrf(suly, 3, 3, buff);  // double to string (float) with format

  if (digitalRead(tareButton) == LOW) {
    //scale.set_scale();
    scale.tare();  //Reset the scale to 0
    Serial.print("Tare ok!");
  }

  if (millis() - utolsoPing > PING_INTERVALLUM) {
    pingKuldes();
  }
  delay(100);
}

void displayData() {
  // Oled display
  display.clearDisplay();
  display.setRotation(2);
  display.setTextSize(1);
  display.setCursor(0, 0);  // column row
  display.print("Weight: ");
  display.setTextSize(1);
  display.print(myString);
  display.setTextSize(1);
  display.println();
  display.println();
  display.print("Allapot: ");
  if (readWeight >= 5 || digitalRead(testButton) == LOW) {
    display.print("Start");
  } else {
    display.print("Stop");
  }

  display.display();
}

void pingKuldes() {
  utolsoPing = millis();

  HTTPClient http;

  // URL összeállítása gomb állapottal
  String url = serverUrl + "?button=" + String(kombinaltAllapot ? "1" : "0");
  http.begin(url);

  int httpCode = http.GET();

  if (httpCode > 0) {
    if (httpCode == HTTP_CODE_OK) {
      //Csak debug célból, hogy lássuk a kommunikációt
      Serial.println("Ping sikeres");
      leds[0] = CRGB::Green;
      FastLED.show();
    }
  } else {
    Serial.printf("Ping sikertelen: %s\n", http.errorToString(httpCode).c_str());
    leds[0] = CRGB::Red;
    FastLED.show();
  }

  http.end();
}
