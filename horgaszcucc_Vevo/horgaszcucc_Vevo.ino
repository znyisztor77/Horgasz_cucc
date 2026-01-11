//lv_conf.h
//Configuration file for v9.2.2
//ESP32 Dev Modul
//https://github.com/tinkering4fun/LVGL_CYD_Framework/tree/redesign

#include <WebServer.h>
#include <WiFi.h>
#include <LVGL_CYD.h>  // https://github.com/ropg/LVGL_CYD  origi
//https://github.com/tinkering4fun/LVGL_CYD_Framework/tree/redesign
#include <DS3231.h>
#include <Wire.h>

const char *ssid = "ESP32_AP";
const char *password = "12345678";
WebServer server(80);
//RTC pins
//#define rtc_sda 21
//#define rtc_scl 22

#define SCREEN_ORIENTATION LVGL_CYD::usbLeft
LVGL_CYD cyd = LVGL_CYD();


// Kapcsolat állapot
bool adoKapcsolva = false;
bool gombAllapot = false;
bool pecaAllapot = false;
unsigned long utolsoUzenet = 0;
const unsigned long TIMEOUT = 5000;  // 5 másodperc timeout

// HTML oldal
const char webpage[] PROGMEM = R"=====(
<!DOCTYPE html>
<html>
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>ESP32 Kapcsolat Monitor</title>
  <style>
    body {
      font-family: Arial, sans-serif;
      display: flex;
      justify-content: center;
      align-items: center;
      min-height: 100vh;
      margin: 0;
      background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
    }
    .container {
      background: white;
      padding: 40px;
      border-radius: 20px;
      box-shadow: 0 10px 40px rgba(0,0,0,0.2);
      text-align: center;
      position: relative;
      min-width: 350px;
    }
    .status-indicator {
      position: absolute;
      top: 20px;
      right: 20px;
      width: 20px;
      height: 20px;
      border-radius: 50%;
      transition: background-color 0.3s;
    }
    .status-indicator.connected {
      background-color: #4CAF50;
      box-shadow: 0 0 10px #4CAF50;
    }
    .status-indicator.disconnected {
      background-color: #f44336;
      box-shadow: 0 0 10px #f44336;
    }
    h1 {
      color: #333;
      margin-bottom: 30px;
      font-size: 24px;
    }
    .status-text {
      font-size: 22px;
      font-weight: bold;
      padding: 20px;
      border-radius: 10px;
      margin-top: 20px;
      transition: all 0.3s;
    }
    .status-text.connected {
      background-color: #e8f5e9;
      color: #2e7d32;
    }
    .status-text.disconnected {
      background-color: #ffebee;
      color: #c62828;
    }
    .button-status {
      font-size: 28px;
      font-weight: bold;
      padding: 30px;
      border-radius: 15px;
      margin-top: 30px;
      transition: all 0.3s;
      border: 3px solid;
    }
    .button-status.pressed {
      background-color: #fff3e0;
      color: #e65100;
      border-color: #e65100;
      transform: scale(1.05);
    }
    .button-status.released {
      background-color: #f5f5f5;
      color: #757575;
      border-color: #bdbdbd;
    }
  </style>
</head>
<body>
  <div class="container">
    <div class="status-indicator" id="indicator"></div>
    <h1>ESP32 Kapcsolat Monitor</h1>
    <div class="status-text" id="status">Várakozás kapcsolatra...</div>
    <div class="button-status" id="buttonStatus">-</div>
  </div>
  
  <script>
    function frissitAllapot() {
      fetch('/status')
        .then(response => response.json())
        .then(data => {
          const indicator = document.getElementById('indicator');
          const status = document.getElementById('status');
          const buttonStatus = document.getElementById('buttonStatus');
          
          if (data.connected) {
            indicator.className = 'status-indicator connected';
            status.className = 'status-text connected';
            status.textContent = 'Kapcsolat létrejött';
            
            if (data.buttonPressed) {
              buttonStatus.className = 'button-status pressed';
              buttonStatus.textContent = 'Gomb benyomva';
            } else {
              buttonStatus.className = 'button-status released';
              buttonStatus.textContent = 'Gomb elengedve';
            }
          } else {
            indicator.className = 'status-indicator disconnected';
            status.className = 'status-text disconnected';
            status.textContent = 'Kapcsolat megszakadt';
            buttonStatus.className = 'button-status released';
            buttonStatus.textContent = '-';
          }
        })
        .catch(error => console.error('Hiba:', error));
    }
    
    setInterval(frissitAllapot, 100);
    frissitAllapot();
  </script>
</body>
</html>
)=====";

bool isTransmitterConnected = false;
static uint32_t count = 1;

// Timer pointerek
static lv_timer_t *main_screen_wifi_timer = nullptr;  // Main screen WiFi timer
static lv_timer_t *shared_wifi_status_timer = nullptr;
static lv_timer_t *receiver_stopper_timer = nullptr;
static lv_timer_t *receiver_timer = nullptr;
static lv_timer_t *stopper_timer = nullptr;
static lv_timer_t *timer_timer = nullptr;
static lv_obj_t *lbl_time_stopper = nullptr;
static lv_obj_t *lbl_time_receiver_timer = nullptr;
static lv_obj_t *lbl_time_receiver = nullptr;
static lv_obj_t *lbl_time_timer = nullptr;
static lv_obj_t *lbl_wifi_state = nullptr;
static lv_obj_t *main_wifi_icon = nullptr;  // Main screen WiFi ikon

// Timer áltozók
static uint32_t receiver_timer_sec = 0;
static uint32_t elapsed_receiver_ms = 0;
static uint32_t elapsed_ms = 0;
static uint32_t timer_duration_sec = 0;

//Alap állapotok
static bool running = false;        // stopper
static bool timer_running = false;  // időzítő
static bool previousConnectionState = false;
static bool elozo_gombAllapot_receiver = false;

//Betöltő képernyő
LV_IMG_DECLARE(emberes_2);  // Kép betöltése
lv_obj_t *loading_label;
lv_obj_t *bg_image;

//////////////////// Végpontok létrehozása ////////////////////////////
void handleRoot() {
  server.send(200, "text/html", webpage);
}

// Állapotok ellenörzése
void handleStatus() {
  if (millis() - utolsoUzenet > TIMEOUT) {
    adoKapcsolva = false;
  }

  String json = "{\"connected\":" + String(adoKapcsolva ? "true" : "false") + ",\"buttonPressed\":" + String(gombAllapot ? "true" : "false") + "}";
  server.send(200, "application/json", json);
}
// Kapcsolat ellenörzése
void handlePing() {
  utolsoUzenet = millis();
  adoKapcsolva = true;

  if (server.hasArg("button")) {
    gombAllapot = (server.arg("button") == "1");
    server.send(200, "text/plain", "OK");
  } else {
    server.send(400, "text/plain", "Hibás kérés!");
  }
}
//////////////////// Loading page ////////////////////////////
void loading_animation(lv_timer_t *timer) {
  static uint8_t dots = 0;
  dots++;

  String text = "Fishing Timer";
  for (int i = 0; i < dots && i < 5; i++) {
    text += ".";
  }

  lv_label_set_text(loading_label, text.c_str());

  // 5 pont után (5 x 500ms = 2.5 másodperc) töröljük mindent
  if (dots >= 5) {
    lv_timer_del(timer);

    // 500ms késleltetés után töröljük a képet és szöveget
    lv_timer_create([](lv_timer_t *t) -> void {
      lv_obj_del(bg_image);
      lv_obj_del(loading_label);
      main_screen();  //A főképernyő indítása
      lv_timer_del(t);
    },
                    500, NULL);
  }
}

void show_loading_text(lv_timer_t *timer) {
  // Loading szöveg megjelenítése
  loading_label = lv_label_create(lv_scr_act());
  lv_label_set_text(loading_label, "Fishing Timer");
  lv_obj_align(loading_label, LV_ALIGN_CENTER, 0, 0);

  // Szöveg stílus
  lv_obj_set_style_text_color(loading_label, lv_color_hex(0xFFFFFF), 0);
  lv_obj_set_style_text_font(loading_label, &lv_font_montserrat_18, 0);

  // Opcionális: sötét háttér a szöveg mögé
  lv_obj_set_style_bg_color(loading_label, lv_color_hex(0x000000), 0);
  lv_obj_set_style_bg_opa(loading_label, LV_OPA_50, 0);
  lv_obj_set_style_pad_all(loading_label, 10, 0);
  lv_obj_set_style_radius(loading_label, 5, 0);

  // Pont animáció indítása
  lv_timer_create(loading_animation, 800, NULL);

  lv_timer_del(timer);
}

//////////////////// Settings ////////////////////////////
void setup() {
  Serial.begin(115200);

 // Wire,begin(rtc_sda, rtc_scl);
  WiFi.softAP(ssid, password);  // A vevő AccessPoint-ként beállítása
  IPAddress IP = WiFi.softAPIP();

  //Szerver végpontok
  server.on("/", handleRoot);
  server.on("/status", handleStatus);
  server.on("/ping", handlePing);

  server.begin();
  Serial.println("Web szerver elindult");
  Serial.println("Nyisd meg böngészőben: http://" + IP.toString());

  //Kijelző és érintőpad inicializálása, A képernyő elforgatása.
  //LVGL_CYD::begin(USB_LEFT); 320x240
  cyd.begin(SCREEN_ORIENTATION); //480X320
  // Emberes háttérkép
  bg_image = lv_img_create(lv_scr_act());
  lv_img_set_src(bg_image, &emberes_2);
  lv_obj_align(bg_image, LV_ALIGN_CENTER, 0, 0);

  // 1 másodperc múlva jelenjen meg a szöveg
  lv_timer_create(show_loading_text, 1000, NULL);
  //LVGL_CYD::led(255, 0, 0);  //A panelen lővő led bekapcsolása 320x240
  cyd.led(255, 0, 0);

  //main_screen();  //A főképernyő indítása
}

//////////////////// Timerek törlése ////////////////////////////
/*A timerek törlésére azért van szükség,
hogy a különbözö ablakok közötti lépkedéskor ne ragadjanak be a memóriába
ezzel is gyorsítva az eszköz működését.*/
void cleanup_all_screen_specific_timers() {
  // Main screen WiFi timer törlése
  if (main_screen_wifi_timer) {
    lv_timer_del(main_screen_wifi_timer);
    main_screen_wifi_timer = nullptr;
  }

  if (shared_wifi_status_timer) {
    lv_timer_del(shared_wifi_status_timer);
    shared_wifi_status_timer = nullptr;
  }

  if (receiver_stopper_timer) {
    lv_timer_del(receiver_stopper_timer);
    receiver_stopper_timer = nullptr;
    elapsed_receiver_ms = 0;
    if (lbl_time_receiver) lv_label_set_text(lbl_time_receiver, "00:00:00");
  }

  if (receiver_timer) {
    lv_timer_del(receiver_timer);
    receiver_timer = nullptr;
    receiver_timer_sec = 0;
    if (lbl_time_receiver_timer) lv_label_set_text(lbl_time_receiver_timer, "00:00:00");
  }

  if (stopper_timer) {
    lv_timer_del(stopper_timer);
    stopper_timer = nullptr;
    running = false;
    elapsed_ms = 0;
    if (lbl_time_stopper) lv_label_set_text(lbl_time_stopper, "00:00:00");
  }

  if (timer_timer) {
    lv_timer_del(timer_timer);
    timer_timer = nullptr;
    timer_running = false;
    timer_duration_sec = 0;
    if (lbl_time_timer) lv_label_set_text(lbl_time_timer, "00:00:00");
  }
}

//////////////////// Connection Status ////////////////////////////
/*Wifi kapcsolat állapotának ellenörzése. 
Ezt a függvényt használja az összes kijelző az állapot ellenörzésére
*/
void updateWiFiConnectionState() {
  isTransmitterConnected = adoKapcsolva;
  if (isTransmitterConnected) {
    //LVGL_CYD::led(0, 255, 0);  // Zöld LED 320X240
    cyd.led(0, 255, 0); //480x320
  } else {
    //LVGL_CYD::led(0, 0, 255);  // Kék LED 320X240
    cyd.led(0, 255, 0); //480X320
  }
}

// Main screen(Fő képernyő) WiFi kapcsolat frissítő
static void main_screen_wifi_update_cb(lv_timer_t *t) {
  updateWiFiConnectionState();

  if (isTransmitterConnected != previousConnectionState) {
    previousConnectionState = isTransmitterConnected;

    if (main_wifi_icon) {
      if (isTransmitterConnected) {
        lv_label_set_text(main_wifi_icon, LV_SYMBOL_WIFI);
        lv_obj_set_style_text_color(main_wifi_icon, lv_color_hex(0x00ff00), LV_PART_MAIN);
      } else {
        lv_label_set_text(main_wifi_icon, LV_SYMBOL_WIFI);
        lv_obj_set_style_text_color(main_wifi_icon, lv_color_hex(0xff0000), LV_PART_MAIN);
      }
    }
  }
}

// Alképernyők WiFi kapcsolat állapot frissítése
static void wifi_status_update_cb(lv_timer_t *t) {
  updateWiFiConnectionState();

  if (isTransmitterConnected != previousConnectionState) {
    previousConnectionState = isTransmitterConnected;

    if (lbl_wifi_state) {
      if (isTransmitterConnected) {
        lv_label_set_text(lbl_wifi_state, LV_SYMBOL_WIFI);
        lv_obj_set_style_text_color(lbl_wifi_state, lv_color_hex(0x00ff00), LV_PART_MAIN);
      } else {
        lv_label_set_text(lbl_wifi_state, "Nincs kapcsolat az adoval!");
        lv_obj_set_style_text_color(lbl_wifi_state, lv_color_hex(0xff0000), LV_PART_MAIN);
      }
    }
  }
}

//////////////////// Main Screen ////////////////////////////
void main_screen() {
  lv_obj_clean(lv_screen_active());
  lv_obj_set_style_bg_color(lv_scr_act(), lv_color_hex(0x000000), 0); //Háttérszín beállítása feketére.  Átállítás fehérre ki kell komentelni.

  lv_obj_t *main_text = lv_label_create(lv_screen_active());
  lv_obj_set_style_text_font(main_text, &lv_font_montserrat_18, LV_PART_MAIN);
  lv_obj_set_style_text_color(main_text, lv_color_hex(0xFFFFFF), 0);
  lv_label_set_text(main_text, "FishingTimer v1.0.");
  lv_obj_align(main_text, LV_ALIGN_TOP_LEFT, 0, 10);

  // WiFi ikon létrehozása a main screen-en
  main_wifi_icon = lv_label_create(lv_screen_active());
  lv_obj_set_style_text_font(main_wifi_icon, &lv_font_montserrat_18, LV_PART_MAIN);
  lv_obj_align(main_wifi_icon, LV_ALIGN_TOP_RIGHT, 0, 10);

  // Kezdeti WiFi állapot beállítása
  updateWiFiConnectionState();
  if (isTransmitterConnected) {
    lv_label_set_text(main_wifi_icon, LV_SYMBOL_WIFI);
    lv_obj_set_style_text_color(main_wifi_icon, lv_color_hex(0x00ff00), LV_PART_MAIN);
  } else {
    lv_label_set_text(main_wifi_icon, LV_SYMBOL_WIFI);
    lv_obj_set_style_text_color(main_wifi_icon, lv_color_hex(0xff0000), LV_PART_MAIN);
  }
  previousConnectionState = isTransmitterConnected;

  // Main screen WiFi timer létrehozása
  if (!main_screen_wifi_timer) {
    main_screen_wifi_timer = lv_timer_create(main_screen_wifi_update_cb, 1000, NULL);
  }
  // Főképernyő vezérlőgombok létrehozása elhelyezése
  /*auto create_nav_btn = [](const char *txt, lv_coord_t y_offset, lv_event_cb_t cb) {
    lv_obj_t *btn = lv_button_create(lv_screen_active());
    lv_obj_set_size(btn, 300, 30);
    lv_obj_align(btn, LV_ALIGN_TOP_MID, 0, y_offset);
    lv_obj_t *lbl = lv_label_create(btn);
    lv_label_set_text(lbl, txt);
    lv_obj_center(lbl);
    lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, NULL);
  };

  create_nav_btn("Vevo stopper", 60, [](lv_event_t *e) {
    cleanup_all_screen_specific_timers();
    lv_obj_clean(lv_screen_active());
    go_receiverStopper();
  });
  create_nav_btn("Vevo idozito", 100, [](lv_event_t *e) {
    cleanup_all_screen_specific_timers();
    lv_obj_clean(lv_screen_active());
    go_receiverTimer();
  });
  create_nav_btn("Stopper", 140, [](lv_event_t *e) {
    cleanup_all_screen_specific_timers();
    lv_obj_clean(lv_screen_active());
    go_stopper();
  });
  create_nav_btn("Idozito", 180, [](lv_event_t *e) {
    cleanup_all_screen_specific_timers();
    lv_obj_clean(lv_screen_active());
    go_timer();
  });
  */

  //lv_obj_set_style_bg_color(lv_scr_act(), lv_color_hex(0x000000), 0);

  // gombok új elrendezése

  int btn_width = 140;
  int btn_height = 60;
  int gap = 15;

  // Open Receiver Stopper Screen Button
  lv_obj_t *btn1 = lv_button_create(lv_scr_act());
  // Erdőzöld
  lv_obj_set_style_bg_color(btn1, lv_color_hex(0x228B22), LV_PART_MAIN);
  lv_obj_set_size(btn1, btn_width, btn_height);
  lv_obj_align(btn1, LV_ALIGN_CENTER, -(btn_width / 2 + gap / 2), -(btn_height / 2 + gap / 2));
  lv_obj_t *lbl1 = lv_label_create(btn1);
  lv_label_set_text(lbl1, "Vevo Stopper");
  lv_obj_center(lbl1);
  lv_obj_add_event_cb(btn1, [](lv_event_t *e) -> void {
      cleanup_all_screen_specific_timers();
      lv_obj_clean(lv_screen_active());
      go_receiverStopper();  // Meghívjuk az ablakot
    },
    LV_EVENT_CLICKED, NULL);

  // Open Receiver Timer Screen Button
  lv_obj_t *btn2 = lv_button_create(lv_scr_act());
  // Erdőzöld
  lv_obj_set_style_bg_color(btn2, lv_color_hex(0x228B22), LV_PART_MAIN);
  lv_obj_set_size(btn2, btn_width, btn_height);
  lv_obj_align(btn2, LV_ALIGN_CENTER, (btn_width / 2 + gap / 2), -(btn_height / 2 + gap / 2));
  lv_obj_t *lbl2 = lv_label_create(btn2);
  lv_label_set_text(lbl2, "Vevo Idozito");
  lv_obj_center(lbl2);
  lv_obj_add_event_cb(btn2, [](lv_event_t *e) -> void {
      cleanup_all_screen_specific_timers();
      lv_obj_clean(lv_screen_active());
      go_receiverTimer();  // Meghívjuk az ablakot
    },
    LV_EVENT_CLICKED, NULL);

  // Open Stopper Screen Button
  lv_obj_t *btn3 = lv_button_create(lv_scr_act());
  // Erdőzöld
  lv_obj_set_style_bg_color(btn3, lv_color_hex(0x228B22), LV_PART_MAIN);
  lv_obj_set_size(btn3, btn_width, btn_height);
  lv_obj_align(btn3, LV_ALIGN_CENTER, -(btn_width / 2 + gap / 2), (btn_height / 2 + gap / 2));
  lv_obj_t *lbl3 = lv_label_create(btn3);
  lv_label_set_text(lbl3, "Stopper");
  lv_obj_center(lbl3);
  lv_obj_add_event_cb(btn3, [](lv_event_t *e) -> void {
      cleanup_all_screen_specific_timers();
      lv_obj_clean(lv_screen_active());
      go_stopper();  // Meghívjuk az ablakot
    },
    LV_EVENT_CLICKED, NULL);

  // Open Timer Screen Button
  lv_obj_t *btn4 = lv_button_create(lv_scr_act());
  // Erdőzöld
  lv_obj_set_style_bg_color(btn4, lv_color_hex(0x228B22), LV_PART_MAIN);
  lv_obj_set_size(btn4, btn_width, btn_height);
  lv_obj_align(btn4, LV_ALIGN_CENTER, (btn_width / 2 + gap / 2), (btn_height / 2 + gap / 2));
  lv_obj_t *lbl4 = lv_label_create(btn4);
  lv_label_set_text(lbl4, "Idozito");
  lv_obj_center(lbl4);
  lv_obj_add_event_cb(btn4, [](lv_event_t *e) -> void {
      cleanup_all_screen_specific_timers();
      lv_obj_clean(lv_screen_active());
      go_timer();  // Meghívjuk az ablakot
    },
    LV_EVENT_CLICKED, NULL);


  //A fogott halak számlálása gomb
  String fishText = "Fogott hal: " + String((int)count - 1);
  lv_obj_t *halDb_text = lv_label_create(lv_screen_active());
  lv_obj_set_style_text_font(halDb_text, &lv_font_montserrat_14, LV_PART_MAIN);
  lv_obj_set_style_text_color(halDb_text, lv_color_hex(0xFFFFFF), 0);
  lv_label_set_text(halDb_text, fishText.c_str());
  lv_obj_align(halDb_text, LV_ALIGN_TOP_MID, 0, 220);
}

//////////////////// Exit Button ////////////////////////////
/*A kilépés gomb globálisan létrehozott gomb,
így nem kell minden funkcióban megírni csak, a függvényt kell meghívni.*/
lv_obj_t *createExitButton() {
  lv_obj_t *exit_btn = lv_obj_create(lv_screen_active());
  lv_obj_set_style_text_color(exit_btn, lv_color_hex(0xFFFFFF), 0); //Fehér kilépés gomb
  lv_obj_clear_flag(exit_btn, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(exit_btn, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_style_bg_opa(exit_btn, LV_OPA_TRANSP, LV_PART_MAIN);
  lv_obj_set_style_border_width(exit_btn, 0, LV_PART_MAIN);
  lv_obj_set_size(exit_btn, 40, 40);
  lv_obj_align(exit_btn, LV_ALIGN_TOP_RIGHT, 0, 0);

  lv_obj_add_event_cb(
    exit_btn, [](lv_event_t *e) -> void {
      cleanup_all_screen_specific_timers();
      lv_obj_clean(lv_screen_active());
      main_screen();
    },
    LV_EVENT_CLICKED, NULL);

  lv_obj_t *lbl_exit_symbol = lv_label_create(exit_btn);
  lv_obj_set_style_text_font(lbl_exit_symbol, &lv_font_montserrat_18, LV_PART_MAIN);
  lv_obj_set_style_text_align(lbl_exit_symbol, LV_TEXT_ALIGN_RIGHT, 0);
  lv_label_set_text(lbl_exit_symbol, LV_SYMBOL_CLOSE);
  lv_obj_align(lbl_exit_symbol, LV_ALIGN_TOP_RIGHT, 5, -10);

  return exit_btn;
}

//////////////////// Receiver Stopper ////////////////////////////
static void update_receiver_stopper_cb(lv_timer_t *t) {
  // Jelenlegi állapot
  bool jelenlegiGombAllapot = gombAllapot;

  // Állapotváltozás detektálása
  if (jelenlegiGombAllapot != elozo_gombAllapot_receiver) {
    if (jelenlegiGombAllapot) {
      // Inaktívról aktívra váltott -> NULLÁZÁS
      elapsed_receiver_ms = 0;
      Serial.println("Receiver Stopper: NULLÁZVA és INDUL");
    } else {
      // Aktívról inaktívra váltott -> MEGÁLL
      Serial.println("Receiver Stopper: MEGÁLL");
    }
    elozo_gombAllapot_receiver = jelenlegiGombAllapot;
  }

  //Csak akkor fut, ha van kapcsolat és a gomb aktív
  if (adoKapcsolva && gombAllapot) {
    elapsed_receiver_ms += 1000;
    uint32_t sec = elapsed_receiver_ms / 1000;
    uint32_t min = (sec / 60) % 60;
    uint32_t hour = sec / 3600;
    sec %= 60;
    char buf[16];
    sprintf(buf, "%02u:%02u:%02u", hour, min, sec);
    lv_label_set_text(lbl_time_receiver, buf);
  }
}

void go_receiverStopper(void) {
  lv_obj_set_style_bg_color(lv_scr_act(), lv_color_hex(0x000000), 0); //Háttérszín beállítása feketére.  Átállítás fehérre ki kell komentelni.
  lv_obj_t *exit_button = createExitButton();

  lbl_wifi_state = lv_label_create(lv_screen_active());
  lv_obj_set_style_text_font(lbl_wifi_state, &lv_font_montserrat_18, LV_PART_MAIN);
  lv_obj_set_style_text_align(lbl_wifi_state, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_align(lbl_wifi_state, LV_ALIGN_TOP_MID, 0, 10);

  updateWiFiConnectionState();
  if (isTransmitterConnected) {
    lv_label_set_text(lbl_wifi_state, LV_SYMBOL_WIFI);
    lv_obj_set_style_text_color(lbl_wifi_state, lv_color_hex(0x00ff00), LV_PART_MAIN); // Wifi jel zöld színnel
  } else {
    lv_label_set_text(lbl_wifi_state, "Nincs kapcsolat az adoval!"); // Nincs kapcsolat piros színnel.
    //lv_obj_set_style_text_color(lbl_wifi_state, lv_color_hex(0xff0000), LV_PART_MAIN);//Piros szín
    lv_obj_set_style_text_color(lbl_wifi_state, lv_color_hex(0xffffff), LV_PART_MAIN);//Fehér szín szín
  }
  previousConnectionState = isTransmitterConnected;

  //A stopper létrehozása
  lbl_time_receiver = lv_label_create(lv_screen_active());
  lv_obj_set_style_text_font(lbl_time_receiver, &lv_font_montserrat_36, LV_PART_MAIN);  //A stopper számainak méretezése
  lv_obj_set_style_text_color(lbl_time_receiver, lv_color_hex(0xFFFFFF), 0); // Fehér szín beállítása a stopperhez.
  char buf_init[16];
  uint32_t sec_init = elapsed_receiver_ms / 1000;
  uint32_t min_init = sec_init / 60;
  uint32_t hour_init = min_init / 60;
  sprintf(buf_init, "%02u:%02u:%02u", hour_init, min_init % 60, sec_init % 60);
  lv_label_set_text(lbl_time_receiver, buf_init);
  lv_obj_align(lbl_time_receiver, LV_ALIGN_TOP_MID, 0, 60);

  if (!shared_wifi_status_timer) {
    shared_wifi_status_timer = lv_timer_create(wifi_status_update_cb, 1000, NULL);
  }
  // Nullázó gomb
  lv_obj_t *btn_reset = lv_button_create(lv_screen_active());
  lv_obj_set_style_bg_color(btn_reset, lv_color_hex(0x228B22), LV_PART_MAIN);
  lv_obj_set_pos(btn_reset, 100, 120);
  lv_obj_set_size(btn_reset, 120, 40);
  lv_obj_t *lbl_reset = lv_label_create(btn_reset);
  lv_label_set_text(lbl_reset, "Reset");
  lv_obj_center(lbl_reset);
  lv_obj_add_event_cb(
    btn_reset, [](lv_event_t *e) {
      elapsed_receiver_ms = 0;
      lv_label_set_text(lbl_time_receiver, "00:00:00");
    },
    LV_EVENT_CLICKED, NULL);

  //Hal számláló gomb
  lv_obj_t *btn_fishCount = lv_button_create(lv_screen_active());
  lv_obj_set_style_bg_color(btn_fishCount, lv_color_hex(0x228B22), LV_PART_MAIN);
  lv_obj_set_pos(btn_fishCount, 100, 180);
  lv_obj_set_size(btn_fishCount, 120, 40);
  lv_obj_t *label_fish = lv_label_create(btn_fishCount);
  lv_label_set_text(label_fish, "Hal ?");
  lv_obj_center(label_fish);
  lv_obj_add_event_cb(
    btn_fishCount,
    [](lv_event_t *e) {
      lv_obj_t *label = (lv_obj_t *)lv_event_get_user_data(e);
      lv_label_set_text_fmt(label, "%" LV_PRIu32, count);
      count++;
    },
    LV_EVENT_CLICKED, label_fish);

  // Állapot inicializálása
  elozo_gombAllapot_receiver = gombAllapot;

  receiver_stopper_timer = lv_timer_create(update_receiver_stopper_cb, 1000, NULL);
}

//////////////////// Receiver Timer ////////////////////////////
static uint32_t saved_receiver_timer_sec = 0;  // Beállított kezdeti érték
static bool prev_receiver_timer_active = false;

static void update_receiver_timer_cb(lv_timer_t *) {
  bool currentActive = (adoKapcsolva && gombAllapot && receiver_timer_sec > 0);

  // Állapotváltozás detektálása
  if (currentActive && !prev_receiver_timer_active) {
    // Inaktívról aktívra -> Visszaállítás a KEZDETI ÉRTÉKRE (csak indításkor!)
    receiver_timer_sec = saved_receiver_timer_sec;
    Serial.print("Receiver Timer: START - Visszaállítva ");
    Serial.print(receiver_timer_sec);
    Serial.println(" mp-re");

    // Kijelző frissítése az új értékkel
    uint32_t sec = receiver_timer_sec % 60;
    uint32_t min = (receiver_timer_sec / 60) % 60;
    uint32_t hour = receiver_timer_sec / 3600;
    char buf[16];
    sprintf(buf, "%02u:%02u:%02u", hour, min, sec);
    lv_label_set_text(lbl_time_receiver_timer, buf);
  } else if (!currentActive && prev_receiver_timer_active) {
    // Aktívról inaktívra -> csak STOP (nem nulláz!)
    Serial.print("Receiver Timer: STOP ");
    Serial.print(receiver_timer_sec);
    Serial.println(" mp-nél");
    // NEM állítjuk vissza az értéket, megmarad ahol megállt!
  }

  prev_receiver_timer_active = currentActive;

  /* Csak akkor számol lefelé, ha aktív állapotban van.
    Az előzőleg beállított értékről indul.
   */
  if (currentActive) {
    receiver_timer_sec--;
    uint32_t sec = receiver_timer_sec % 60;
    uint32_t min = (receiver_timer_sec / 60) % 60;
    uint32_t hour = receiver_timer_sec / 3600;
    char buf[16];
    sprintf(buf, "%02u:%02u:%02u", hour, min, sec);
    lv_label_set_text(lbl_time_receiver_timer, buf);
  }
}

void go_receiverTimer(void) {
  lv_obj_set_style_bg_color(lv_scr_act(), lv_color_hex(0x000000), 0); //Háttérszín beállítása feketére.  Átállítás fehérre ki kell komentelni.
  lv_obj_t *exit_button = createExitButton();

  lbl_wifi_state = lv_label_create(lv_screen_active());
  lv_obj_set_style_text_font(lbl_wifi_state, &lv_font_montserrat_18, LV_PART_MAIN);
  lv_obj_set_style_text_align(lbl_wifi_state, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_align(lbl_wifi_state, LV_ALIGN_TOP_MID, 0, 10);

  updateWiFiConnectionState();
  if (isTransmitterConnected) {
    lv_label_set_text(lbl_wifi_state, LV_SYMBOL_WIFI);
    lv_obj_set_style_text_color(lbl_wifi_state, lv_color_hex(0x00ff00), LV_PART_MAIN);
  } else {
    lv_label_set_text(lbl_wifi_state, "Nincs kapcsolat!");
    lv_obj_set_style_text_color(lbl_wifi_state, lv_color_hex(0xff0000), LV_PART_MAIN);
  }

  lbl_time_receiver_timer = lv_label_create(lv_screen_active());
  lv_obj_set_style_text_font(lbl_time_receiver_timer, &lv_font_montserrat_36, LV_PART_MAIN);
  char buf_init[16];
  uint32_t sec_init = receiver_timer_sec % 60;
  uint32_t min_init = (receiver_timer_sec / 60) % 60;
  uint32_t hour_init = receiver_timer_sec / 3600;
  sprintf(buf_init, "%02u:%02u:%02u", hour_init, min_init, sec_init);
  lv_label_set_text(lbl_time_receiver_timer, buf_init);
  lv_obj_align(lbl_time_receiver_timer, LV_ALIGN_TOP_MID, 0, 60);

  if (!shared_wifi_status_timer) {
    shared_wifi_status_timer = lv_timer_create(wifi_status_update_cb, 1000, NULL);
  }
  //Nullázó gomb
  lv_obj_t *btn_reset = lv_button_create(lv_screen_active());
  lv_obj_set_pos(btn_reset, 100, 120);
  lv_obj_set_size(btn_reset, 120, 40);
  lv_obj_t *lbl_reset = lv_label_create(btn_reset);
  lv_label_set_text(lbl_reset, "Reset");
  lv_obj_center(lbl_reset);
  lv_obj_add_event_cb(
    btn_reset, [](lv_event_t *e) {
      receiver_timer_sec = 0;
      saved_receiver_timer_sec = 0;
      lv_label_set_text(lbl_time_receiver_timer, "00:00:00");
      Serial.println("Receiver Timer: Reset");
    },
    LV_EVENT_CLICKED, NULL);
  //Érték növelés 1 percenként
  lv_obj_t *btn_plus = lv_button_create(lv_screen_active());
  lv_obj_set_pos(btn_plus, 20, 120);
  lv_obj_set_size(btn_plus, 60, 40);
  lv_obj_t *lbl_plus = lv_label_create(btn_plus);
  lv_label_set_text(lbl_plus, "+1 perc");
  lv_obj_center(lbl_plus);
  lv_obj_add_event_cb(
    btn_plus, [](lv_event_t *e) {
      saved_receiver_timer_sec += 60;
      receiver_timer_sec = saved_receiver_timer_sec;
      uint32_t sec = receiver_timer_sec % 60;
      uint32_t min = (receiver_timer_sec / 60) % 60;
      uint32_t hour = receiver_timer_sec / 3600;
      char buf[16];
      sprintf(buf, "%02u:%02u:%02u", hour, min, sec);
      lv_label_set_text(lbl_time_receiver_timer, buf);
    },
    LV_EVENT_CLICKED, NULL);
  //Érték csökkentés 1 percenként, ha van beállított érték.
  lv_obj_t *btn_minus = lv_button_create(lv_screen_active());
  lv_obj_set_pos(btn_minus, 240, 120);
  lv_obj_set_size(btn_minus, 60, 40);
  lv_obj_t *lbl_minus = lv_label_create(btn_minus);
  lv_label_set_text(lbl_minus, "-1 perc");
  lv_obj_center(lbl_minus);
  lv_obj_add_event_cb(
    btn_minus, [](lv_event_t *e) {
      if (saved_receiver_timer_sec >= 60) {
        saved_receiver_timer_sec -= 60;
        receiver_timer_sec = saved_receiver_timer_sec;
      }
      uint32_t sec = receiver_timer_sec % 60;
      uint32_t min = (receiver_timer_sec / 60) % 60;
      uint32_t hour = receiver_timer_sec / 3600;
      char buf[16];
      sprintf(buf, "%02u:%02u:%02u", hour, min, sec);
      lv_label_set_text(lbl_time_receiver_timer, buf);
    },
    LV_EVENT_CLICKED, NULL);
  //Hal számláló gomb
  lv_obj_t *btn_fishCount = lv_button_create(lv_screen_active());
  lv_obj_set_pos(btn_fishCount, 100, 180);
  lv_obj_set_size(btn_fishCount, 120, 40);
  lv_obj_t *label_fish = lv_label_create(btn_fishCount);
  lv_label_set_text(label_fish, "Hal ?");
  lv_obj_center(label_fish);
  lv_obj_add_event_cb(
    btn_fishCount,
    [](lv_event_t *e) {
      lv_obj_t *label = (lv_obj_t *)lv_event_get_user_data(e);
      lv_label_set_text_fmt(label, "%" LV_PRIu32, count);
      count++;
    },
    LV_EVENT_CLICKED, label_fish);

  // Állapot inicializálása
  saved_receiver_timer_sec = receiver_timer_sec;
  prev_receiver_timer_active = false;

  receiver_timer = lv_timer_create(update_receiver_timer_cb, 1000, NULL);
}
//////////////////// Stopper ////////////////////////////
static void update_stopper_cb(lv_timer_t *t) {
  if (running) {
    elapsed_ms += 1000;
    uint32_t sec = elapsed_ms / 1000;
    uint32_t min = (sec / 60) % 60;
    uint32_t hour = sec / 3600;
    sec %= 60;
    char buf[16];
    sprintf(buf, "%02u:%02u:%02u", hour, min, sec);
    lv_label_set_text(lbl_time_stopper, buf);
  }
}

void go_stopper(void) {
  lv_obj_set_style_bg_color(lv_scr_act(), lv_color_hex(0x000000), 0); //Háttérszín beállítása feketére.  Átállítás fehérre ki kell komentelni.
  lv_obj_t *exit_button = createExitButton();
  // Stopper létrehozása
  lbl_time_stopper = lv_label_create(lv_screen_active());
  lv_obj_set_style_text_font(lbl_time_stopper, &lv_font_montserrat_36, LV_PART_MAIN);
  lv_label_set_text(lbl_time_stopper, "00:00:00");
  running = false;
  elapsed_ms = 0;
  lv_obj_align(lbl_time_stopper, LV_ALIGN_TOP_MID, 0, 30);

  //A stopper indítása és leállítása gomb
  lv_obj_t *btn_start_stop = lv_button_create(lv_screen_active());
  lv_obj_set_pos(btn_start_stop, 20, 120);
  lv_obj_set_size(btn_start_stop, 120, 40);
  lv_obj_t *lbl_start_stop = lv_label_create(btn_start_stop);
  lv_label_set_text(lbl_start_stop, "Start");
  lv_obj_center(lbl_start_stop);
  lv_obj_add_event_cb(
    btn_start_stop,
    [](lv_event_t *e) {
      running = !running;
      lv_obj_t *btn = (lv_obj_t *)lv_event_get_target(e);
      lv_obj_t *label = lv_obj_get_child(btn, 0);
      lv_label_set_text(label, running ? "Stop" : "Start");
    },
    LV_EVENT_CLICKED, nullptr);
  //Nullázás gomb
  lv_obj_t *btn_reset = lv_button_create(lv_screen_active());
  lv_obj_set_pos(btn_reset, 180, 120);
  lv_obj_set_size(btn_reset, 120, 40);
  lv_obj_t *lbl_r = lv_label_create(btn_reset);
  lv_label_set_text(lbl_r, "Reset");
  lv_obj_center(lbl_r);
  lv_obj_add_event_cb(
    btn_reset,
    [](lv_event_t *) {
      elapsed_ms = 0;
      lv_label_set_text(lbl_time_stopper, "00:00:00");
    },
    LV_EVENT_CLICKED, nullptr);
  //Hal számláló gomb.
  lv_obj_t *btn_fishCount = lv_button_create(lv_screen_active());
  lv_obj_set_pos(btn_fishCount, 100, 180);
  lv_obj_set_size(btn_fishCount, 120, 40);
  lv_obj_t *label_fish = lv_label_create(btn_fishCount);
  lv_label_set_text(label_fish, "Hal ?");
  lv_obj_center(label_fish);
  lv_obj_add_event_cb(
    btn_fishCount,
    [](lv_event_t *e) {
      lv_obj_t *label = (lv_obj_t *)lv_event_get_user_data(e);
      lv_label_set_text_fmt(label, "%" LV_PRIu32, count);
      count++;
    },
    LV_EVENT_CLICKED, label_fish);

  stopper_timer = lv_timer_create(update_stopper_cb, 1000, nullptr);
}

//////////////////// Timer ////////////////////////////
static void update_timer_cb(lv_timer_t *) {
  if (timer_running && timer_duration_sec > 0) {
    timer_duration_sec--;
    uint32_t sec = timer_duration_sec % 60;
    uint32_t min = (timer_duration_sec / 60) % 60;
    uint32_t hour = timer_duration_sec / 3600;
    char buf[16];
    sprintf(buf, "%02u:%02u:%02u", hour, min, sec);
    lv_label_set_text(lbl_time_timer, buf);
  }
}

void go_timer(void) {
  lv_obj_set_style_bg_color(lv_scr_act(), lv_color_hex(0x000000), 0); //Háttérszín beállítása feketére.  Átállítás fehérre ki kell komentelni.
  lv_obj_t *exit_button = createExitButton();
  //Időzítő létrehozása
  lbl_time_timer = lv_label_create(lv_screen_active());
  lv_obj_set_style_text_font(lbl_time_timer, &lv_font_montserrat_36, LV_PART_MAIN);
  timer_running = false;
  timer_duration_sec = 0;
  lv_label_set_text(lbl_time_timer, "00:00:00");
  lv_obj_align(lbl_time_timer, LV_ALIGN_TOP_MID, 0, 30);
  //Az időzitő indítása és leállítása gomb
  lv_obj_t *btn_start_stop = lv_button_create(lv_screen_active());
  lv_obj_set_pos(btn_start_stop, 20, 80);
  lv_obj_set_size(btn_start_stop, 120, 40);
  lv_obj_t *lbl_start = lv_label_create(btn_start_stop);
  lv_label_set_text(lbl_start, "Start");
  lv_obj_center(lbl_start);
  lv_obj_add_event_cb(
    btn_start_stop, [](lv_event_t *e) {
      timer_running = !timer_running;
      lv_obj_t *btn = (lv_obj_t *)lv_event_get_target(e);
      lv_obj_t *label = lv_obj_get_child(btn, 0);
      lv_label_set_text(label, timer_running ? "Stop" : "Start");
    },
    LV_EVENT_CLICKED, NULL);
  //Nullázás gomb
  lv_obj_t *btn_reset = lv_button_create(lv_screen_active());
  lv_obj_set_pos(btn_reset, 180, 80);
  lv_obj_set_size(btn_reset, 120, 40);
  lv_obj_t *lbl_reset = lv_label_create(btn_reset);
  lv_label_set_text(lbl_reset, "Reset");
  lv_obj_center(lbl_reset);
  lv_obj_add_event_cb(
    btn_reset, [](lv_event_t *e) {
      timer_running = false;
      timer_duration_sec = 0;
      lv_label_set_text(lbl_time_timer, "00:00:00");
    },
    LV_EVENT_CLICKED, NULL);
  //Érték növelés 1 percenként
  lv_obj_t *btn_plus = lv_button_create(lv_screen_active());
  lv_obj_set_pos(btn_plus, 20, 130);
  lv_obj_set_size(btn_plus, 120, 40);
  lv_obj_t *lbl_plus = lv_label_create(btn_plus);
  lv_label_set_text(lbl_plus, "+1 perc");
  lv_obj_center(lbl_plus);
  lv_obj_add_event_cb(
    btn_plus, [](lv_event_t *e) {
      timer_duration_sec += 60;
      uint32_t sec = timer_duration_sec % 60;
      uint32_t min = (timer_duration_sec / 60) % 60;
      uint32_t hour = timer_duration_sec / 3600;
      char buf[16];
      sprintf(buf, "%02u:%02u:%02u", hour, min, sec);
      lv_label_set_text(lbl_time_timer, buf);
    },
    LV_EVENT_CLICKED, NULL);
  //Érték növelés 1 percenként, ha van beállított érték
  lv_obj_t *btn_minus = lv_button_create(lv_screen_active());
  lv_obj_set_pos(btn_minus, 180, 130);
  lv_obj_set_size(btn_minus, 120, 40);
  lv_obj_t *lbl_minus = lv_label_create(btn_minus);
  lv_label_set_text(lbl_minus, "-1 perc");
  lv_obj_center(lbl_minus);
  lv_obj_add_event_cb(
    btn_minus, [](lv_event_t *e) {
      if (timer_duration_sec >= 60) timer_duration_sec -= 60;
      uint32_t sec = timer_duration_sec % 60;
      uint32_t min = (timer_duration_sec / 60) % 60;
      uint32_t hour = timer_duration_sec / 3600;
      char buf[16];
      sprintf(buf, "%02u:%02u:%02u", hour, min, sec);
      lv_label_set_text(lbl_time_timer, buf);
    },
    LV_EVENT_CLICKED, NULL);
  //Hal számláló gomb
  lv_obj_t *btn_fishCount = lv_button_create(lv_screen_active());
  lv_obj_set_pos(btn_fishCount, 100, 180);
  lv_obj_set_size(btn_fishCount, 120, 40);
  lv_obj_t *label_fish = lv_label_create(btn_fishCount);
  lv_label_set_text(label_fish, "Hal ?");
  lv_obj_center(label_fish);
  lv_obj_add_event_cb(
    btn_fishCount,
    [](lv_event_t *e) {
      lv_obj_t *label = (lv_obj_t *)lv_event_get_user_data(e);
      lv_label_set_text_fmt(label, "%" LV_PRIu32, count);
      count++;
    },
    LV_EVENT_CLICKED, label_fish);

  timer_timer = lv_timer_create(update_timer_cb, 1000, nullptr);
}

void loop() {
  server.handleClient();  //Szerver nditása

  // Timeout ellenőrzés
  if (adoKapcsolva && (millis() - utolsoUzenet > TIMEOUT)) {
    adoKapcsolva = false;
    Serial.println("Adó kapcsolat megszakadt (timeout)");
  }

  lv_task_handler();  //Az LVGL könyvtár futtatása
  //lv_tick_inc(5);// A képernyő váltás késleltetése.
  delay(5);  //Az új képernyő megjelenések késleltetése a újra rajzolás segítése miatt.
}