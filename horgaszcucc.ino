


/*
   -------------------------------------------------------------------------------------
   ESP32 mikrokontroller és HX711 súlymérő szenzorral épített időzítő horgászathoz
   Könyvtárak:
   -Adafruit SSD1306 /Kijelző
   -RTClib /RTC órához
   -HX711_ADC /mérleg szenzor
   -------------------------------------------------------------------------------------
*/

#include <splash.h>
#include <Adafruit_SSD1306.h>
#include <HX711_ADC.h>
#include <RTClib.h>
#include <Wire.h>
#include <gfxfont.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SPITFT.h>
#include <Adafruit_SPITFT_Macros.h>
#include <Adafruit_GrayOLED.h>

RTC_DS3231 rtc;

char daysOfTheWeek[7][12] = {"Vasarnap", "Hetfo", "Kedd", "Szerda", "Csutortok", "Pentek", "Szombat"};

#if defined(ESP8266)|| defined(ESP32) || defined(AVR)
#include <EEPROM.h>
#endif


#define SCREEN_WIDTH 128 // OLED display width, szélesség pixelben
#define SCREEN_HEIGHT 64 // OLED display height, magaság pixelben
// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
#define OLED_RESET     -1 // Reset pin # (or -1 if sharing Arduino reset pin)
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);


const int HX711_dout = 16; //mcu > HX711 dout pin, ESP32 GPIO16
const int HX711_sck = 4; //mcu > HX711 sck pin, ESP32 GPIO4

//HX711 constructor:
HX711_ADC LoadCell(HX711_dout, HX711_sck);

const int calVal_eepromAdress = 0;
unsigned long t = 0;
volatile boolean newDataReady;

int reading;
int lastReading;

//FishCount button debounce
const int fishCountButtonPin = 15;
int count = 0;
int fishCountButtonState;           
int fishCountlastButtonState = HIGH;
unsigned long lastDebounceTime = 0;  // the last time the output pin was toggled
unsigned long debounceDelay = 50;    // the debounce time; increase if the output flickers

/*void fishCountButton(int count){
  
  int reading2 = digitalRead(fishCountButtonPin);
  if (reading2 != fishCountlastButtonState) {
    lastDebounceTime = millis();
  }

  if ((millis() - lastDebounceTime) > debounceDelay) {
    if (reading2 != fishCountButtonState) {
      fishCountButtonState = reading2;

      if (fishCountButtonState == LOW) {
        count = count + 1;
      }
    }
  }
  
  fishCountlastButtonState = reading2;
}*/

void displayWeight(int weight){
  display.println();
  display.println("Suly meres:");
  display.setTextSize(2);  
  display.println(String(weight) + "g");
  
}
void displayFishcount(int fishcount){
  //display.println();  
  display.setTextSize(2);  
  display.println(String(fishcount)+ " db");
  
}
void displayTimeTemp(){
  DateTime now = rtc.now();
  float temp = rtc.getTemperature();
  display.println(String(now.year(), DEC) + "/" + String(now.month(), DEC) + "/" + String(now.day(), DEC)+ "  " +String(now.hour(), DEC) + ":" + String(now.minute(), DEC) + ":" + String(now.second(), DEC) );
  //display.println(daysOfTheWeek[now.dayOfTheWeek()]);
  //display.println(String(now.hour(), DEC) + ":" + String(now.minute(), DEC) + ":" + String(now.second(), DEC));
  display.println((String(temp)+" "+char(247)+"C"));
  
}
  
void setup() {
  Serial.begin(115200); delay(10);
  Serial.println();
  Serial.println("Starting...");  

  pinMode(2, INPUT_PULLUP);
  pinMode(15, INPUT_PULLUP);
  
  float calibrationValue; // calibration value
  calibrationValue = 32.47; // uncomment this if you want to set this value in the sketch
  
  #if defined(ESP8266) || defined(ESP32)
  //EEPROM.begin(512); // uncomment this if you use ESP8266 and want to fetch the value from eeprom
  #endif
  //EEPROM.get(calVal_eepromAdress, calibrationValue); // uncomment this if you want to fetch the value from eeprom

  LoadCell.begin();
  //LoadCell.setReverseOutput(); //ezt kell használni ha a bekötés miatt csak negatív értéket mutat
  unsigned long stabilizingtime = 2000; // tare preciscion can be improved by adding a few seconds of stabilizing time
  boolean _tare = true; //set this to false if you don't want tare to be performed in the next step
  LoadCell.start(stabilizingtime, _tare);
  if (LoadCell.getTareTimeoutFlag()) {
    Serial.println("Timeout, check MCU>HX711 wiring and pin designations");
    while (1);
  }
  else {
    LoadCell.setCalFactor(calibrationValue); // set calibration value (float)
    Serial.println("Startup is complete");
  }

  attachInterrupt(digitalPinToInterrupt(HX711_dout), dataReadyISR, FALLING);

  //OLED Display

  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { // Address 0x3D for 128x64
      Serial.println(F("SSD1306 allocation failed"));
      for(;;);
    }
  //RTC
   if (! rtc.begin()) {
    Serial.println("Could not find RTC! Check circuit.");
    while (1);
  }
}
  //interrupt routine:
void dataReadyISR() {
    if (LoadCell.update()) {
      newDataReady = 1;
    }
  }
  
void loop() {
  
  const int serialPrintInterval = 0; //increase value to slow down serial print activity
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(1);
  display.setCursor(0, 0);
  displayTimeTemp();
  // get smoothed value from the dataset:
  if (newDataReady) {
    if (millis() > t + serialPrintInterval) {
      reading = LoadCell.getData();
      newDataReady = 0;
      Serial.print("Load_cell output val: ");
      Serial.println(reading);
      //Serial.print("  ");
      //Serial.println(millis() - t);
      if (reading != lastReading){
          }
      lastReading = reading;
      t = millis();
    }
  }
  displayWeight(reading);
  
   
  // Tare button with debounce GPIO2
  boolean newState = digitalRead(2); // Get current button state.
  boolean oldState = HIGH;
  if((newState == LOW) && (oldState == HIGH)) { // Check if state changed from high to low (button press).
    delay(5);// Short delay to debounce button.
    newState = digitalRead(2);// Check if button is still low after debounce.
    if(newState == LOW) {      // Yes, still low
        LoadCell.tareNoDelay();
        if(count != 0){
          count = 0;
          }
     }
   }
  /* 
  // Fishcount button GPIO15 
  boolean newState2 = digitalRead(15); // Get current button state.
  boolean oldState2 = HIGH;
  if((newState2 == LOW) && (oldState2 == HIGH)) { // Check if state changed from high to low (button press).
    delay(50);// Short delay to debounce button.
    newState2 = digitalRead(15);// Check if button is still low after debounce.
    if(newState2 == LOW) {      // Yes, still low
        count = count + 1;
     }
   }*/
  int reading2 = digitalRead(fishCountButtonPin);
  if (reading2 != fishCountlastButtonState) {
    lastDebounceTime = millis();
  }

  if ((millis() - lastDebounceTime) > debounceDelay) {
    if (reading2 != fishCountButtonState) {
      fishCountButtonState = reading2;

      if (fishCountButtonState == LOW) {
        count = count + 1;
      }
    }
  }
  
  fishCountlastButtonState = reading2;

   //fishCountButton(count);
   displayFishcount(count);
  
  
  // receive command from serial terminal, send 't' to initiate tare operation: (a terminálra a t küldése után újra tárázik) 
  if (Serial.available() > 0) {
    char inByte = Serial.read();
    if (inByte == 't') LoadCell.tareNoDelay();
  }

  //check if last tare operation is complete
  if (LoadCell.getTareStatus() == true) {
    Serial.println("Tare complete");
  }
  display.display();
}
