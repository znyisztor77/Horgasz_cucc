#include <WiFi.h>
#include <WebServer.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

int hours = 0, minutes = 0;
long int seconds = 0;
int hoursStop = 0, minutesStop = 0;
long int secondsStop = 0;
unsigned long previousMillis = 0;
const long interval = 1000;

const char* ssid = "ESP32_AP";
const char* password = "12345678";

WebServer server(80);
int switchState = HIGH; // Kapcsoló alapállapota

// Kapcsoló állapotának frissítése az ESP8266-tól
void handleUpdate() {
  if (server.hasArg("state")) {
    switchState = server.arg("state").toInt();
    server.send(200, "text/plain", "Kapcsoló állapot frissítve!");
  } else {
    server.send(400, "text/plain", "Hibás kérés!");
  }
}

// Weboldal megjelenítése
void handleRoot() {
  String html = "<html><head>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1.0'>";
  html += "<meta charset='UTF-8'>";
  html += "<script>function updateState(){";
  html += "fetch('/state').then(response => response.text()).then(data => {";
  html += "document.getElementById('switchState').innerHTML = data;";
  html += "}); setTimeout(updateState, 1000); }";
  html += "window.onload = updateState;";
  html += "</script></head>";
  html += "<body style='font-family: Arial; text-align: center; padding: 20px;'>";
  html += "<h2>ESP32 Kapcsoló Állapot</h2>";
  html += "<h1 id='switchState'>N/A</h1>";
  html += "</body></html>";
  
  server.send(200, "text/html", html);
}

// Az aktuális állapot JSON formátumban
void handleState() {
  server.send(200, "text/plain", switchState == HIGH ? "Nincs benyomva" : "Benyomva");
}

// OLED kijelző frissítése
void updateDisplay() {
  display.clearDisplay();
  display.setCursor(0, 0);
  //display.setTextSize(2);
  //display.setTextColor(SSD1306_WHITE);
  display.println("Kapcsolo:");
  display.println(switchState == HIGH ? "Nincs benyomva" : "Benyomva");
  //display.display();
}

// Stopperóra kijelzése
void displayStopWatch(int h, int m, long int s){
      if(h<10){
        display.print("0"+String(h)+ ":");
      }
      
      else{ display.print(String(h)+ ":");
      }
      if(m<10){
        display.print("0"+String(m)+ ":");
      }
      
      else{ display.print(String(m)+ ":");
      }
      if(s<10){
        display.print("0"+String(s));
      }
      
      else{ display.print(String(s));
      }
}

void setup() {
  Serial.begin(115200);

  // OLED kijelző inicializálása
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("OLED inicializálás sikertelen!");
    while (true);
  }
  /*display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("Kapcsolódás...");*/
  

  // Access Point indítása
  WiFi.softAP(ssid, password);
  Serial.println("Access Point létrehozva!");
  Serial.print("IP cím: ");
  Serial.println(WiFi.softAPIP());

  // Webszerver végpontok beállítása
  server.on("/", handleRoot);
  server.on("/state", handleState);
  server.on("/update", HTTP_POST, handleUpdate);

  server.begin();
  //updateDisplay();
}

void loop() {
  server.handleClient();

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(1);
  display.setCursor(0, 0);
  display.println("Kapcsolo:");
  display.println(switchState == HIGH ? "Nincs benyomva" : "Benyomva");
  //updateDisplay();
  // Stopperóra
  //------------------------------------------------------------------------------
//Stopwatch
  unsigned long currentMillis = millis();
  
  
  if(switchState == LOW){
    if(currentMillis - previousMillis >= interval){
      previousMillis = currentMillis;
      seconds++;
      
      if (seconds>59){ 
      seconds=0;   
      minutes++; 
      } 
      if (minutes>59){
      hours++; 
      minutes=0; 
      } 
      if(hours>23){ 
      hours=0; 
      }   
    }
  }
  
  displayStopWatch(hours, minutes, seconds);
 
  if ((hours > 0 || minutes > 0 || seconds > 0)&& switchState == HIGH)
  {    
    hoursStop = hours;
    minutesStop = minutes;
    secondsStop = seconds;    
    delay(200);
    hours=0;
    minutes=0;
    seconds=0;
  }
  display.println();
  display.println("Stoped...");
  displayStopWatch(hoursStop, minutesStop, secondsStop );  
 //-----------------------------------------------------------------------
 display.display();                  
}
