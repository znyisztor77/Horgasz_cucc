#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <RotaryEncoder.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET     -1

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

#define ENCODER_A 32
#define ENCODER_B 33
#define ENCODER_BUTTON 26

RotaryEncoder encoder(ENCODER_A, ENCODER_B, RotaryEncoder::LatchMode::TWO03);

enum MenuItemType { NORMAL, EXIT, BACK };

typedef struct {
    String name;
    int parent;
    MenuItemType type;
    int children[3];
} MenuItem;

MenuItem menu[] = {
    {"Fomenu",       -1, NORMAL, {1, 2, 3}},
    {"Almenu 1",      0, NORMAL, {4, 5, 6}},
    {"Almenu 2",      0, NORMAL, {7, 8, 9}},
    {"Almenu 3",      0, NORMAL, {10, 11, 12}},
    
    {"Almenu 1.1",    1, NORMAL, {-1, -1, 13}}, // BACK hozzáadva
    {"Almenu 1.2",    1, NORMAL, {-1, -1, 14}}, // BACK hozzáadva
    {"Exit",          1, EXIT,   {0}},      // Exit from Almenu 1
    
    {"Almenu 2.1",    2, NORMAL, {-1, -1, 15}}, // BACK hozzáadva
    {"Almenu 2.2",    2, NORMAL, {-1, -1, 16}}, // BACK hozzáadva
    {"Exit",          2, EXIT,   {0}},      // Exit from Almenu 2
    
    {"Almenu 3.1",    3, NORMAL, {-1, -1, 17}}, // BACK hozzáadva
    {"Almenu 3.2",    3, NORMAL, {-1, -1, 18}}, // BACK hozzáadva
    {"Exit",          3, EXIT,   {0}},      // Exit from Almenu 3

   
    
    {"Back", 5, BACK, {-1, -1, 1}}, // Back from Almenu 1.2 // Új menüpont
    {"Back", 7, BACK, {-1, -1, 2}}, // Back from Almenu 2.1 // Új menüpont
    {"Back", 8, BACK, {-1, -1, 2}}, // Back from Almenu 2.2 // Új menüpont
    {"Back", 10, BACK, {-1, -1, 3}}, // Back from Almenu 3.1 // Új menüpont
    {"Back", 11, BACK, {-1, -1, 3}}  // Back from Almenu 3.2 // Új menüpont

};

void bootAnimation() {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(20, 20);
    display.println("Fishing Timer!");
    display.display();
    delay(1000);
    display.clearDisplay();
    display.display();
}
int currentMenu = 0;
int selectedIndex = 0;

void setup() {
    Serial.begin(115200);
    Wire.begin();
    display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
    bootAnimation();
    display.clearDisplay();
    pinMode(ENCODER_BUTTON, INPUT_PULLUP);
    drawMenu();
}


void loop() {
    encoder.tick();
    static int lastPos = 0;
    int newPos = encoder.getPosition();

    int maxIndex = 2; // Alapértelmezett maximum index

    if(currentMenu == 4 || currentMenu == 5 || currentMenu == 7 || currentMenu == 8 || currentMenu == 10 || currentMenu == 11) {
        maxIndex = 1;
    }

    if (newPos > lastPos) {
        selectedIndex = min(maxIndex, selectedIndex + 1);
        drawMenu();
    }
    if (newPos < lastPos) {
        selectedIndex = max(0, selectedIndex - 1);
        drawMenu();
    }
    lastPos = newPos;

    if (digitalRead(ENCODER_BUTTON) == LOW) {
        unsigned long startTime = millis(); // Elmentjük a gombnyomás kezdetének idejét

        while (digitalRead(ENCODER_BUTTON) == LOW) {
            if (millis() - startTime > 1000) { // 1 másodperc eltelt
                currentMenu = 0; // Vissza a főmenübe
                selectedIndex = 0;
                delay(50);
                drawMenu();
                while (digitalRead(ENCODER_BUTTON) == LOW); // Várjuk meg, amíg elengedik a gombot
                return; // Kilépünk a loop-ból
            }
        }

        int nextMenu = menu[currentMenu].children[selectedIndex];
        if (nextMenu != -1) {
            if (menu[nextMenu].type == EXIT) {
                currentMenu = 0; // Vissza a főmenübe
            } else if (menu[nextMenu].type == BACK) {
                currentMenu = menu[currentMenu].parent;
                if(currentMenu == -1) currentMenu = 0; // Megakadályozza az érvénytelen menübe lépést
            }
            else {
                currentMenu = nextMenu;
            }
            selectedIndex = 0;
        }
        drawMenu();
        // Gomb debouncing
        delay(50);
        while (digitalRead(ENCODER_BUTTON) == LOW);
        delay(50);
    }
}

void drawMenu() {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    display.println(menu[currentMenu].name);

    int childCount = 0;
    for (int i = 0; i < 3; i++) {
        int childIndex = menu[currentMenu].children[i];
        if (childIndex != -1) {
            childCount++;
            if (i == selectedIndex) display.print("> "); else display.print("  ");
            display.println(menu[childIndex].name);
            
        }
    }

    display.display();
}
