#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
//#include <RotaryEncoder.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET     -1

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

#define encoder_A 33
#define encoder_B 32
#define encoder_BUTTON 26

const unsigned char bitmap_icon_battery [] PROGMEM = {
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3f, 0xf8, 0x40, 0x04, 0x5b, 0x66, 0x5b, 0x66, 
  0x5b, 0x66, 0x40, 0x04, 0x3f, 0xf8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

int encoderCount = 0;
int encoder_ALast;
int value_A;
boolean bCW;
int menuIndex = 0;

boolean oldState = HIGH;

int mode = 0;

boolean menu1 = false;
boolean menu2 = false;
boolean menu3 = false;
boolean menu4 = false;



void bootAnimation(){
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(20,20);
  display.println("Fishing Timer");
  display.display();
  delay(1000);
  display.clearDisplay();
  display.display();  
}

void header(){
  display.setCursor(50,0);
  display.println("Menu");
  display.drawBitmap(112, 0, bitmap_icon_battery, 16, 16, 1);
  display.drawLine(0,13,128,13, WHITE);
}

void displayEncoderCounter(int count){
  display.setTextSize(1);
  display.setCursor(20,20);
  display.println(String(count));
  
}

void menuSzoveg(String szoveg){
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(20, 20);
    display.println(String(szoveg));
    display.display();
}

void vevoStopper(){
  while(menu1){    
    menuSzoveg("Vevo Stopper"); // függvény hívás    
    if (digitalRead(encoder_BUTTON) == LOW) { //LongPress funkció
        unsigned long startTime = millis();
        while (digitalRead(encoder_BUTTON) == LOW) {
            if (millis() - startTime > 1000) { 
                encoderCount = 0;               
                delay(1000);             
                while (digitalRead(encoder_BUTTON) == LOW);
                return; // Kilépünk a loop-ból
             }
          }
      }   
   }
}

void vevoIdozito(){
  while(menu2){    
    menuSzoveg("Vevo Idozito"); //függvény hívás
    if (digitalRead(encoder_BUTTON) == LOW) { //LongPress funkció
        unsigned long startTime = millis();
        while (digitalRead(encoder_BUTTON) == LOW) {
            if (millis() - startTime > 1000) { 
                encoderCount = 0;               
                delay(1000);             
                while (digitalRead(encoder_BUTTON) == LOW);
                return; // Kilépünk a loop-ból
             }
         }
      }         
   }
}

void Stopper(){
  while(menu3){    
    menuSzoveg("Stopper"); // függvény hívás
    if (digitalRead(encoder_BUTTON) == LOW) { //LongPress funkció
        unsigned long startTime = millis();
        while (digitalRead(encoder_BUTTON) == LOW) {
            if (millis() - startTime > 1000) { 
                encoderCount = 0;               
                delay(1000);             
                while (digitalRead(encoder_BUTTON) == LOW);
                return; // Kilépünk a loop-ból
             }
         }
      }       
   }
}

void Idozito(){
  while(menu4){    
    menuSzoveg("Idozito"); // függvény hívás
    if (digitalRead(encoder_BUTTON) == LOW) { //LongPress funkció
        unsigned long startTime = millis();
        while (digitalRead(encoder_BUTTON) == LOW) {
            if (millis() - startTime > 1000) { 
                encoderCount = 0;               
                delay(1000);             
                while (digitalRead(encoder_BUTTON) == LOW);
                return; // Kilépünk a loop-ból
             }
         }
      }       
   }
}

void setup() {
  
  Serial.begin(115200);
  Wire.begin();
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  bootAnimation();
  display.clearDisplay();
  pinMode (encoder_A,INPUT);
  pinMode (encoder_B,INPUT);
  pinMode(encoder_BUTTON, INPUT_PULLUP);
  encoder_ALast = digitalRead(encoder_A);
    
}

void loop() {
  display.clearDisplay();
  
//encoder kezelése---------------------
 value_A = digitalRead(encoder_A);
  if (value_A != encoder_ALast) { 
    if (digitalRead(encoder_B) != value_A) {
      encoderCount ++;
      bCW = true;      
    } else {
      bCW = false;
      encoderCount--;     
    }
    Serial.print ("Rotated: ");
    if (bCW) {
      Serial.println ("clockwise");
    } else {
      Serial.println("counterclockwise");
    }    
    Serial.println(encoderCount);
  }
  encoder_ALast = value_A;

  if(encoderCount > 3 || encoderCount < 0) encoderCount = 0;
//encoder kezelés vége---------------------------------------
  
boolean newState = digitalRead(encoder_BUTTON);
/*if((newState == LOW) && (oldState == HIGH)){
  delay(20);
  newState = digitalRead(encoder_BUTTON);
  if(newState == LOW){
    if(++mode > 2) mode = 0;
    
    }
  }

  oldState = newState;*/
//displayEncoderCounter(encoderCount);
  switch (encoderCount) {
      case 0:
        header();
        display.setCursor(0, 20);
        display.println("> Vevo Stopper");
        display.println("  Vevo Idozito");
        display.println("  Stopper");
        display.println("  Idozito");
        if(newState == LOW){
        menu1 = true;
        vevoStopper();
        }
        break;
      case 1:
        header();
        display.setCursor(0, 20);
        display.println("  Vevo Stopper");
        display.println("> Vevo Idozito");
        display.println("  Stopper");
        display.println("  Idozito");
        if(newState == LOW){
        menu2 = true;
        vevoIdozito();
        }
        break;
      case 2:
        header();
        display.setCursor(0, 20);
        display.println("  Vevo Stopper");
        display.println("  Vevo Idozito");
        display.println("> Stopper");
        display.println("  Idozito");
        if(newState == LOW){
        menu3 = true;
        Stopper();
        }
        break;
      case 3:
        header();
        display.setCursor(0, 20);
        display.println("  Vevo Stopper");
        display.println("  Vevo Idozito");
        display.println("  Stopper");
        display.println("> Idozito");
        if(newState == LOW){
        menu4 = true;
        Idozito();
        }
        break;
  }

  
  display.display();
}
