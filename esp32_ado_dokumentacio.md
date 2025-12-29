# ESP32 Adó Eszköz - Teljes Dokumentáció

## 📋 Tartalomjegyzék
1. [Áttekintés](#áttekintés)
2. [Hardver specifikáció](#hardver-specifikáció)
3. [Pin bekötések](#pin-bekötések)
4. [Szükséges alkatrészek](#szükséges-alkatrészek)
5. [Szoftver követelmények](#szoftver-követelmények)
6. [Telepítési útmutató](#telepítési-útmutató)
7. [Használat](#használat)
8. [Funkciók](#funkciók)
9. [Kijelző információk](#kijelző-információk)
10. [LED jelzések](#led-jelzések)
11. [Hibaelhárítás](#hibaelhárítás)
12. [Kalibráció](#kalibráció)

---

## 🎯 Áttekintés

Az **ESP32 Adó** egy vezeték nélküli súlymérő és gomb állapot küldő eszköz, amely WiFi-n keresztül kommunikál a Vevő egységgel. A készülék folyamatosan méri a súlyt HX711 Load Cell segítségével, és azonnal jelzi az állapotváltozásokat.

### Fő jellemzők:
- ✅ **Valós idejű súlymérés** (HX711 Load Cell)
- ✅ **Kézi gomb bemenet** (teszt/manuális mód)
- ✅ **128x32 OLED kijelző** helyi megjelenítéshez
- ✅ **WiFi kommunikáció** ESP32 Vevővel
- ✅ **RGB LED visszajelzés** (WS2812 NeoPixel)
- ✅ **Automatikus újracsatlakozás** WiFi elvesztése esetén
- ✅ **Nullázó funkció** (tare) a súlymérőhöz

---

## 🔧 Hardver specifikáció

### Fő modul
- **Mikrokontroller**: ESP32-S3 SuperMini
- **Flash memória**: 4MB
- **WiFi**: 2.4 GHz 802.11 b/g/n
- **USB**: USB-C (natív USB CDC)
- **Tápellátás**: 5V USB vagy 3.3V direct

### Perifériák
| Komponens | Típus | Interfész |
|-----------|-------|-----------|
| OLED kijelző | SSD1306 128x32 | I2C |
| Súlymérő | HX711 + Load Cell | Digital |
| LED | WS2812 RGB | Single-wire |
| Gomb 1 | Nyomógomb (Tare) | GPIO Input |
| Gomb 2 | Nyomógomb (Test) | GPIO Input |

---

## 📌 Pin bekötések

### ESP32-S3 SuperMini GPIO kiosztás:

```
┌─────────────────────────────────────┐
│      ESP32-S3 SuperMini             │
├─────────────────────────────────────┤
│ GPIO 48 ──────────► NeoPixel LED    │
│ GPIO 8  ──────────► OLED SDA (I2C)  │
│ GPIO 9  ──────────► OLED SCL (I2C)  │
│ GPIO 4  ──────────► HX711 DT         │
│ GPIO 5  ──────────► HX711 SCK        │
│ GPIO 6  ──────────► Tare Button      │
│ GPIO 7  ──────────► Test Button      │
│ 3.3V    ──────────► OLED VCC         │
│ GND     ──────────► OLED GND         │
│ 3.3V    ──────────► HX711 VCC        │
│ GND     ──────────► HX711 GND        │
└─────────────────────────────────────┘
```

### HX711 Load Cell bekötés:

```
┌──────────────┐         ┌──────────────┐
│  Load Cell   │         │    HX711     │
├──────────────┤         ├──────────────┤
│ Piros  (E+)  ├─────────┤ E+           │
│ Fekete (E-)  ├─────────┤ E-           │
│ Fehér  (A+)  ├─────────┤ A+           │
│ Zöld   (A-)  ├─────────┤ A-           │
└──────────────┘         │              │
                         │ DT  ─────────┤──► GPIO 4
                         │ SCK ─────────┤──► GPIO 5
                         │ VCC ─────────┤──► 3.3V
                         │ GND ─────────┤──► GND
                         └──────────────┘
```

### OLED kijelző bekötés (I2C):

```
┌──────────────┐
│ SSD1306 OLED │
├──────────────┤
│ VCC ─────────┤──► 3.3V
│ GND ─────────┤──► GND
│ SCL ─────────┤──► GPIO 9
│ SDA ─────────┤──► GPIO 8
└──────────────┘
```

### Gombok bekötése:

```
GPIO 6 ────┤ Tare Button ├──── GND
           (Belső pull-up)

GPIO 7 ────┤ Test Button ├──── GND
           (Belső pull-up)
```

**Megjegyzés**: Mindkét gomb `INPUT_PULLUP` módban működik, így csak egy nyomógomb és GND kell.

---

## 🛒 Szükséges alkatrészek

| # | Alkatrész | Mennyiség | Megjegyzés |
|---|-----------|-----------|------------|
| 1 | ESP32-S3 SuperMini | 1 db | Mikrokontroller |
| 2 | SSD1306 OLED 128x32 | 1 db | I2C interfész, 0.91" |
| 3 | HX711 modul | 1 db | 24-bit ADC |
| 4 | Load Cell (mérőcella) | 1 db | 0-5kg vagy nagyobb |
| 5 | Nyomógomb | 2 db | 6x6mm vagy hasonló |
| 6 | Breadboard / PCB | 1 db | Prototípushoz |
| 7 | Jumper kábelek | ~15 db | Bekötésekhez |
| 8 | USB-C kábel | 1 db | Táp + programozás |

### Opcionális:
- Doboz/ház a készülék védelmére
- Ellenállások (ha nincs belső pull-up)
- Kondenzátorok (tápszűréshez)

---

## 💻 Szoftver követelmények

### Arduino IDE beállítások:

```
Arduino IDE verzió: 1.8.19 vagy újabb (vagy 2.x)
ESP32 Board csomag: 2.0.0 vagy újabb
```

### Board Manager URL:
```
https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
```

### Board beállítások:
```
Board: "ESP32S3 Dev Module"
Upload Speed: "921600"
USB CDC On Boot: "Enabled"
Flash Mode: "QIO 80MHz"
Flash Size: "4MB (32Mb)"
Partition Scheme: "Default 4MB with spiffs"
Core Debug Level: "None"
Erase All Flash: "Disabled"
```

### Szükséges könyvtárak (Libraries):

```cpp
// Alapkönyvtárak (beépített)
#include <WiFi.h>
#include <HTTPClient.h>
#include <Wire.h>

// Telepítendő könyvtárak:
#include <HX711_ADC.h>          // HX711 súlymérő
#include <FastLED.h>            // NeoPixel LED
#include <Adafruit_GFX.h>       // Grafikus alap
#include <Adafruit_SSD1306.h>   // OLED driver
```

### Könyvtárak telepítése:

**Sketch → Include Library → Manage Libraries**

1. **HX711_ADC** (by Olav Kallhovd)
2. **FastLED** (by Daniel Garcia)
3. **Adafruit GFX Library**
4. **Adafruit SSD1306**

---

## 📥 Telepítési útmutató

### 1. Arduino IDE beállítása

1. Nyisd meg az Arduino IDE-t
2. **File → Preferences**
3. Add hozzá az ESP32 Board Manager URL-t
4. **Tools → Board → Boards Manager**
5. Telepítsd az "esp32" csomagot
6. Válaszd ki a **"ESP32S3 Dev Module"** board-ot

### 2. Könyvtárak telepítése

```
Sketch → Include Library → Manage Libraries
Keress rá és telepítsd:
- HX711_ADC
- FastLED
- Adafruit GFX
- Adafruit SSD1306
```

### 3. Hardver összeállítása

1. Kösd be a komponenseket a pin-kiosztás szerint
2. Ellenőrizd a tápfeszültséget (3.3V!)
3. Csatlakoztasd az USB-C kábelt

### 4. Kód feltöltése

1. Nyisd meg az ESP32 Adó kódot
2. **Tools → Port** → Válaszd ki az USB portot
3. Kattints az **Upload** gombra (→)
4. Várj a feltöltés befejezésére

### 5. Első indítás ellenőrzése

1. **Tools → Serial Monitor** (115200 baud)
2. Ellenőrizd a kiírt üzeneteket:
   - HX711 inicializálása
   - WiFi csatlakozás
   - IP cím megszerzése

---

## 🎮 Használat

### Bekapcsolás

1. Csatlakoztasd az USB kábelt
2. A készülék automatikusan elindul
3. Az OLED kijelző megjelenik
4. A LED piros, amíg nincs WiFi kapcsolat

### WiFi csatlakozás

A készülék automatikusan csatlakozik a beállított WiFi hálózathoz:
```cpp
const char* ssid = "ESP32_AP";
const char* password = "12345678";
```

**Sikeres csatlakozás jelei:**
- Serial Monitor: "WiFi csatlakozva!"
- OLED: "WiFi: OK (-XXdBm)"
- LED: Kék (kapcsolódva, inaktív)

### Súlymérés

1. **Nullázás (Tare)**: Nyomd meg a **Tare gombot** (GPIO 6)
   - Vagy írj `t` betűt a Serial Monitorba
2. **Mérés**: Helyezd a tárgyat a Load Cell-re
3. Ha a súly **> 5g**, akkor aktív állapot
4. Az OLED megmutatja az aktuális súlyt

### Teszt mód

**Test gomb** (GPIO 7) megnyomásával:
- Manuálisan aktiválhatod az állapotot
- Hasznos, ha a súlymérő nem elérhető
- Kombinálja a gomb ÉS a súly állapotot

---

## ⚙️ Funkciók

### 1. Súlymérés (HX711)
- **Felbontás**: 24-bit
- **Mintavétel**: 3 mérés átlaga
- **Küszöb**: 5 gramm
- **Frissítés**: Folyamatos (nem blokkoló)

### 2. WiFi kommunikáció
- **Protokoll**: HTTP GET
- **Ping intervallum**: 500 ms
- **Timeout**: 2 másodperc
- **Újracsatlakozás**: Automatikus (max 3 próbálkozás)

### 3. Állapot küldése
```
URL: http://192.168.4.1/ping?button=X
X = 0: Inaktív állapot
X = 1: Aktív állapot (gomb VAGY súly)
```

### 4. Kijelző frissítés
- **Intervallum**: 500 ms
- **Működés**: WiFi-től független
- **Tartalom**: WiFi állapot, súly, állapot

---

## 📺 Kijelző információk

### OLED kijelző formátum (128x32):

```
┌────────────────────────────┐
│ WiFi: OK (-65dBm)          │  ← WiFi állapot + jelerősség
│ Suly: 12.5 g               │  ← Mért súly
│ Allapot: AKTIV             │  ← Kombinált állapot
└────────────────────────────┘
```

### Állapot szövegek:

| WiFi | Súly | Kijelző |
|------|------|---------|
| ✅ OK | - | `WiFi: OK (-XXdBm)` |
| ❌ Nincs | - | `WiFi: NINCS` |
| - | 0-5g | `Allapot: INAKTIV` |
| - | >5g | `Allapot: AKTIV` |

---

## 💡 LED jelzések

### NeoPixel RGB LED (GPIO 48):

| Szín | Jelentés |
|------|----------|
| 🔴 **Piros** | Nincs WiFi kapcsolat |
| 🔵 **Kék** | WiFi OK, állapot inaktív |
| 🟢 **Zöld** | WiFi OK, állapot aktív (súly vagy gomb) |

### LED állapotgép:

```
[INDÍTÁS] → 🔴 Piros (WiFi keresés)
              ↓
         WiFi kapcsolódás
              ↓
         🔵 Kék (Inaktív)
              ↓
         Súly > 5g VAGY gomb
              ↓
         🟢 Zöld (Aktív)
```

---

## 🔧 Hibaelhárítás

### Probléma: OLED nem jelenik meg

**Lehetséges okok:**
- ❌ Rossz I2C cím (próbáld: 0x3C vagy 0x3D)
- ❌ Rossz pin-ek (SDA/SCL felcserélve?)
- ❌ Nincs tápfeszültség

**Megoldás:**
```cpp
// I2C szkennelő:
Wire.begin(I2C_SDA, I2C_SCL);
Wire.beginTransmission(0x3C);
if (Wire.endTransmission() == 0) {
  Serial.println("OLED found at 0x3C");
}
```

### Probléma: HX711 nem működik

**Lehetséges okok:**
- ❌ Load Cell nincs bekötve
- ❌ DT/SCK pin-ek rosszul vannak
- ❌ Nincs kalibráció

**Megoldás:**
1. Ellenőrizd a bekötéseket
2. Nézd meg a Serial Monitor-t: `"HX711 Timeout hiba!"`
3. Végezz kalibrációt (lásd lentebb)

### Probléma: WiFi nem csatlakozik

**Lehetséges okok:**
- ❌ Rossz SSID/jelszó
- ❌ A Vevő nincs bekapcsolva
- ❌ Túl messze van

**Megoldás:**
```cpp
// Serial Monitor kimenet:
WiFi csatlakozás...
....................  // Ha sok pont, nincs kapcsolat
```
- Ellenőrizd az SSID-t és jelszót
- Kapcsold be a Vevőt először
- Közelítsd az eszközöket

### Probléma: Folyamatosan újracsatlakozik

**Lehetséges okok:**
- ❌ Gyenge WiFi jel
- ❌ Ping nem ér célba

**Megoldás:**
- Csökkentsd a távolságot
- Növeld a TIMEOUT értéket
- Ellenőrizd a Vevő IP címét

---

## 📏 Kalibráció

### HX711 Load Cell kalibrálása:

1. **Nullázás (Tare)**:
```cpp
// Üres Load Cell-lel:
LoadCell.tare();
```

2. **Kalibráció ismert súllyal**:

```cpp
// Helyezz egy ismert súlyú tárgyat (pl. 100g)
float calibrationValue = 696.0;  // Kezdő érték

// Nézd meg a mért értéket Serial Monitor-ban
// Ha 100g helyett 150g-ot mutat:
// új_kalibráció = 696.0 * (150 / 100) = 1044.0

LoadCell.setCalFactor(calibrationValue);
```

3. **Tesztelés**:
- Helyezz különböző súlyú tárgyakat
- Ellenőrizd a pontosságot
- Finomhangold a `calibrationValue`-t

### Küszöbérték beállítása:

```cpp
const float SULY_KUSZOB = 5.0;  // gramm

// Ha érzékenyebb kell: csökkentsd (pl. 2.0)
// Ha kevésbé érzékeny: növeld (pl. 10.0)
```

---

## 📊 Műszaki adatok

| Paraméter | Érték |
|-----------|-------|
| Mérési tartomány | 0 - 5000g (Load Cell függő) |
| Felbontás | ~0.1g |
| Frissítési sebesség | 10 Hz (100ms) |
| WiFi hatótáv | ~50m (akadályoktól függően) |
| Tápfeszültség | 5V USB / 3.3V direct |
| Áramfelvétel | ~150mA (aktív WiFi) |
| Kijelző | 128x32 monochrome OLED |
| LED | 1x WS2812 RGB |

---

## 📝 Verzióinformációk

| Verzió | Dátum | Változások |
|--------|-------|------------|
| 1.0 | 2024-12 | Kezdeti verzió |
| 1.1 | 2024-12 | WiFi optimalizálás, nem blokkoló működés |
| 1.2 | 2024-12 | ESP32-S3 SuperMini támogatás |

---

## 📞 Támogatás

**Gyakori hibák és megoldások:**
- Serial Monitor: 115200 baud
- Reset gomb: Ha lefagy az eszköz
- Újratöltés: Ha a kód nem megfelelő

**Fejlesztési lehetőségek:**
- MQTT protokoll WiFi helyett
- Akkumulátoros működés (deep sleep)
- SD kártya naplózás
- Több Load Cell támogatása

---

## ⚖️ Licenc és jogi nyilatkozat

Ez a dokumentáció és a hozzá tartozó kód oktatási célra készült. 
Használat saját felelősségre!

**Figyelem**: A súlymérő pontossága függ a Load Cell minőségétől és a kalibrációtól. Kereskedelmi mérlegként NEM használható!

---

**Utolsó frissítés**: 2024. December
**Készítette**: [Te]
**Kapcsolat**: [Email/GitHub]