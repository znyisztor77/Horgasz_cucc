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

#define DOUT 5
#define CLK 18
HX711 scale(DOUT, CLK);

const char* ssid = "ESP32_AP";      // ESP32 Access Point neve
const char* password = "12345678";  // ESP32 Access Point jelszava

const char* serverUrl = "http://192.168.4.1/update";  // Az ESP32 IP-címe (alapértelmezett 192.168.4.1)

const int tareButton = 2;  // this button will be used to reset the scale to 0.
String myString;
String cmessage;  // complete message
char buff[10];
float weight;
float calibration_factor = 20;  // for me this vlaue works just perfect 1500

//static int prevState = HIGH;
//int currentState = digitalRead(tareButton);
static int prevState;
int currentState;
int readWeight;

SimpleTimer timer;

// for the OLED display

#define SCREEN_WIDTH 128  // OLED display width, in pixels
#define SCREEN_HEIGHT 32  // OLED display height, in pixels

// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
#define OLED_RESET -1  // Reset pin # (or -1 if sharing Arduino reset pin)
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

void setup() {
  Serial.begin(115200);
  pinMode(tareButton, INPUT_PULLUP);

  WiFi.begin(ssid, password);
  Serial.print("Csatlakozás az ESP32-höz...");
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
  }
  Serial.println("\nCsatlakozva!");

  scale.set_scale();
  scale.tare();                             //Reset the scale to 0
  long zero_factor = scale.read_average();  //Get a baseline reading


  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  timer.setInterval(1000L, getSendData);
  display.clearDisplay();
  display.setTextColor(WHITE);
}

void loop() {


  timer.run();                          // Initiates SimpleTimer
  scale.set_scale(calibration_factor);  //Adjust to this calibration factor

  weight = scale.get_units(15);  //15
  myString = dtostrf(weight, 3, 3, buff); // double to string (float) with format
  cmessage = cmessage + "Weight" + ":" + myString + ",";
  Serial.println(cmessage);
  cmessage = "";

  Serial.println();

  

  //static int prevState = HIGH;
  //int currentState = digitalRead(tareButton);
  //int currentState = HIGH;

  //int readWeight;
  readWeight = (int)weight;
  Serial.println("Olvasott érték: " + String(readWeight));

  
  if (readWeight >= 5) {
    Serial.println("Nagyobb mint öt!!!!");
    prevState = true;
    currentState = false;
  }
  else{currentState = true; prevState = false;}
  

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
  if (digitalRead(tareButton) == LOW) {
    //scale.set_scale();
    scale.tare();  //Reset the scale to 0
    Serial.print("Tare ok!");
  }
  delay(100);
}

void getSendData() {
  // Oled display
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);  // column row
  display.print("Weight: ");

  display.setTextSize(1);
  //display.setCursor(0, 30);  
  display.print(myString);
  display.setTextSize(1);
  display.println();
  display.println();
  display.print("Allapot: ");
  if(readWeight >=5){
    display.print("Start");
  }else{display.print("Stop");}

  display.display();
}
