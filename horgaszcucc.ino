

/*
   -------------------------------------------------------------------------------------
   ESP32 mikrokontroller és HX711 súlymérő szenzorral épített időzítő horgászathoz
   
   -------------------------------------------------------------------------------------
*/

#include <splash.h>
#include <Adafruit_SSD1306.h>
#include <HX711_ADC.h>
#if defined(ESP8266)|| defined(ESP32) || defined(AVR)
#include <EEPROM.h>
#endif


#define SCREEN_WIDTH 128 // OLED display width, szélesség pixelben
#define SCREEN_HEIGHT 32 // OLED display height, magaság pixelben
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

float reading;
float lastReading;

void displayWeight(float weight){
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0, 0);
  // Display static text
  display.println("Suly meres:");
  display.display();
  display.setCursor(0, 10);
  display.setTextSize(2);
  display.print(weight);
  display.print(" ");
  display.print("g");
  display.display();  
}

void setup() {
  Serial.begin(115200); delay(10);
  Serial.println();
  Serial.println("Starting...");  
  
  float calibrationValue; // calibration value
  calibrationValue = 32.47; // uncomment this if you want to set this value in the sketch
#if defined(ESP8266) || defined(ESP32)
  //EEPROM.begin(512); // uncomment this if you use ESP8266 and want to fetch the value from eeprom
#endif
  //EEPROM.get(calVal_eepromAdress, calibrationValue); // uncomment this if you want to fetch the value from eeprom

  LoadCell.begin();
  //LoadCell.setReverseOutput();
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
}
//interrupt routine:
void dataReadyISR() {
    if (LoadCell.update()) {
      newDataReady = 1;
    }
  }
  
void loop() {
  const int serialPrintInterval = 0; //increase value to slow down serial print activity

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
      displayWeight(reading); // Adatok kiírása a kijelzőre
    }
    lastReading = reading;
    t = millis();
    
    }
  }

  // receive command from serial terminal, send 't' to initiate tare operation: (a terminálra a t küldése után újra tárázik)
  if (Serial.available() > 0) {
    char inByte = Serial.read();
    if (inByte == 't') LoadCell.tareNoDelay();
  }

  //check if last tare operation is complete
  if (LoadCell.getTareStatus() == true) {
    Serial.println("Tare complete");
  }

}
