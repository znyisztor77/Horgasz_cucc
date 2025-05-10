#include <WebServer.h>
#include <WiFi.h>
#include <LVGL_CYD.h>

const char *ssid = "ESP32_AP";
const char *password = "12345678";
bool switchState = true;
WebServer server(80);

//lv_obj_t *btn_exit;
bool isTransmitterConnected = false;
static uint32_t count = 1;


// Timer pointerek
static lv_timer_t *shared_wifi_status_timer = nullptr;  // Közös timer a WiFi állapothoz
static lv_timer_t *receiver_stopper_timer = nullptr;    // A go_receiverStopper saját timere
static lv_timer_t *receiver_timer = nullptr;            // A go_receiverTimer saját timere
static lv_timer_t *stopper_timer = nullptr;             // A go_stopper saját timere
static lv_timer_t *timer_timer = nullptr;               // A go_timer saját timere
static uint32_t elapsed_receiver_ms = 0;
static lv_obj_t *lbl_time_receiver_timer = nullptr;
static lv_obj_t *lbl_time_receiver = nullptr;
static lv_obj_t *lbl_time_timer = nullptr;
static lv_obj_t *lbl_wifi_state = nullptr;
static uint32_t receiver_timer_sec = 0;
static bool running = false;
static uint32_t elapsed_ms = 0;
static lv_obj_t *lbl_time_stopper = nullptr;
static uint32_t timer_duration_sec = 0;
static bool timer_running = false;


void handleUpdate() {
  if (server.hasArg("state")) {
    switchState = server.arg("state").toInt();
    server.send(200, "text/plain", "Kapcsoló állapot frissítve!");
  } else {
    server.send(400, "text/plain", "Hibás kérés!");
  }
}

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

void handleState() {
  server.send(200, "text/plain", switchState ? "Nincs benyomva" : "Benyomva");
}

void setup() {
  WiFi.softAP(ssid, password);
  server.on("/", handleRoot);
  server.on("/state", handleState);
  server.on("/update", HTTP_POST, handleUpdate);
  server.begin();

  LVGL_CYD::begin(USB_LEFT);
  main_screen();
}

//////////////////// Main Menu////////////////////////////
void main_screen() {
  lv_obj_clean(lv_screen_active());

  lv_obj_t *main_text = lv_label_create(lv_screen_active());
  lv_obj_set_style_text_font(main_text, &lv_font_montserrat_18, LV_PART_MAIN);
  lv_label_set_text(main_text, "FishingTimer v1.0.");
  lv_obj_align(main_text, LV_ALIGN_TOP_MID, 0, 10);

  auto create_nav_btn = [](const char *txt, lv_coord_t y_offset, lv_event_cb_t cb) {
    lv_obj_t *btn = lv_button_create(lv_screen_active());
    lv_obj_set_size(btn, 300, 30);
    lv_obj_align(btn, LV_ALIGN_TOP_MID, 0, y_offset);
    lv_obj_t *lbl = lv_label_create(btn);
    lv_label_set_text(lbl, txt);
    lv_obj_center(lbl);
    lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, NULL);
  };

  create_nav_btn("Vevo stopper", 60, [](lv_event_t *e) {
    lv_obj_clean(lv_screen_active());
    go_receiverStopper();
  });
  create_nav_btn("Vevo idozito", 100, [](lv_event_t *e) {
    lv_obj_clean(lv_screen_active());
    go_receiverTimer();
  });
  create_nav_btn("Stopper", 140, [](lv_event_t *e) {
    lv_obj_clean(lv_screen_active());
    go_stopper();
  });
  create_nav_btn("Idozito", 180, [](lv_event_t *e) {
    lv_obj_clean(lv_screen_active());
    go_timer();
  });

  String halSzoveg = "Fogott hal: " + String((int)count - 1);
  lv_obj_t *halDb_text = lv_label_create(lv_screen_active());
  lv_obj_set_style_text_font(halDb_text, &lv_font_montserrat_14, LV_PART_MAIN);
  lv_label_set_text(halDb_text, halSzoveg.c_str());
  lv_obj_align(halDb_text, LV_ALIGN_TOP_MID, 0, 220);
}

//////////////////// Timerek törlése ////////////////////////////
void cleanup_all_screen_specific_timers() {
  // WiFi státusz timer törlése
  if (shared_wifi_status_timer) {
    lv_timer_del(shared_wifi_status_timer);
    shared_wifi_status_timer = nullptr;
  }
  // Receiver Stopper timer törlése
  if (receiver_stopper_timer) {
    lv_timer_del(receiver_stopper_timer);
    receiver_stopper_timer = nullptr;
    // Itt resetelheted a receiver_stopperhez tartozó állapotokat is, ha kell
    elapsed_receiver_ms = 0;
    if (lbl_time_receiver) lv_label_set_text(lbl_time_receiver, "00:00:00");
  }
  // Receiver Timer timer törlése
  if (receiver_timer) {
    lv_timer_del(receiver_timer);
    receiver_timer = nullptr;
    // Itt resetelheted a receiver_timerhez tartozó állapotokat is, ha kell
    receiver_timer_sec = 0;
    if (lbl_time_receiver_timer) lv_label_set_text(lbl_time_receiver_timer, "00:00:00");
  }
  // Stopper timer törlése
  if (stopper_timer) {
    lv_timer_del(stopper_timer);
    stopper_timer = nullptr;
    // Itt resetelheted a stopperhez tartozó állapotokat is, ha kell
    running = false;
    elapsed_ms = 0;
    if (lbl_time_stopper) lv_label_set_text(lbl_time_stopper, "00:00:00");
  }
  // Timer (Időzítő) timer törlése
  if (timer_timer) {
    lv_timer_del(timer_timer);
    timer_timer = nullptr;
    // Itt resetelheted az időzítőhöz tartozó állapotokat is, ha kell
    timer_running = false;
    timer_duration_sec = 0;
    if (lbl_time_timer) lv_label_set_text(lbl_time_timer, "00:00:00");
  }
  
}
//////////////////// Exit Button////////////////////////////
/*lv_obj_t *createExitButton() {
  lv_obj_t *exit_btn = lv_obj_create(lv_screen_active());
  lv_obj_clear_flag(exit_btn, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(exit_btn, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_style_bg_opa(exit_btn, LV_OPA_TRANSP, LV_PART_MAIN);
  lv_obj_set_style_border_width(exit_btn, 0, LV_PART_MAIN);
  lv_obj_set_size(exit_btn, 40, 40);
  lv_obj_align(exit_btn, LV_ALIGN_TOP_RIGHT, 0, 0);
  lv_obj_add_event_cb(
    exit_btn, [](lv_event_t *e) -> void {
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
*/

lv_obj_t *createExitButton() {  // Visszatérési típusa lv_obj_t*
  lv_obj_t *exit_btn = lv_obj_create(lv_screen_active());
  lv_obj_clear_flag(exit_btn, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(exit_btn, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_style_bg_opa(exit_btn, LV_OPA_TRANSP, LV_PART_MAIN);
  lv_obj_set_style_border_width(exit_btn, 0, LV_PART_MAIN);
  lv_obj_set_size(exit_btn, 40, 40);
  lv_obj_align(exit_btn, LV_ALIGN_TOP_RIGHT, 0, 0);

  lv_obj_add_event_cb(
    exit_btn, [](lv_event_t *e) -> void {
      cleanup_all_screen_specific_timers();  // <- FONTOS: Timerek törlése itt
      lv_obj_clean(lv_screen_active());      // Aktuális képernyő objektumainak törlése
      main_screen();                         // Főképernyő újraépítése
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
//static lv_timer_t *receiver_stopper_timer = nullptr;
//static lv_obj_t *lbl_time_receiver = nullptr;
//static uint32_t elapsed_receiver_ms = 0;

//static lv_obj_t *lbl_wifi_state = nullptr;
static bool previousConnectionState = false;

void updateWiFiConnectionState() {
  isTransmitterConnected = (WiFi.softAPgetStationNum() > 0);
}

static void wifi_status_update_cb(lv_timer_t *t) {
  updateWiFiConnectionState();

  if (isTransmitterConnected != previousConnectionState) {
    previousConnectionState = isTransmitterConnected;

    if (isTransmitterConnected) {
      lv_label_set_text(lbl_wifi_state, LV_SYMBOL_WIFI);
      lv_obj_set_style_text_color(lbl_wifi_state, lv_color_hex(0x00ff00), LV_PART_MAIN);
    } else {
      lv_label_set_text(lbl_wifi_state, "Nincs kapcsolat az adoval!");
      lv_obj_set_style_text_color(lbl_wifi_state, lv_color_hex(0xff0000), LV_PART_MAIN);
    }
  }
}

static void update_receiver_stopper_cb(lv_timer_t *t) {
  if (!switchState) {
    elapsed_receiver_ms += 1000;
    uint32_t sec = elapsed_receiver_ms / 1000;
    uint32_t min = sec / 60;
    uint32_t hour = min / 60;
    sec %= 60;
    char buf[16];
    sprintf(buf, "%02u:%02u:%02u", hour, min % 60, sec);
    lv_label_set_text(lbl_time_receiver, buf);
  }
}

void go_receiverStopper(void) {
  //btn_exit = createExitButton();
  lv_obj_t *exit_button = createExitButton();  // Kilépés gomb létrehozása (a cleanup már benne van)


  /*lbl_wifi_state = lv_label_create(lv_screen_active());
  lv_obj_set_style_text_font(lbl_wifi_state, &lv_font_montserrat_18, LV_PART_MAIN);
  lv_obj_set_style_text_align(lbl_wifi_state, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_align(lbl_wifi_state, LV_ALIGN_TOP_MID, 0, 10);*/

  // WiFi állapot label létrehozása (minden alkalommal újra kell, mert az lv_obj_clean törli)
  lbl_wifi_state = lv_label_create(lv_screen_active());
  lv_obj_set_style_text_font(lbl_wifi_state, &lv_font_montserrat_18, LV_PART_MAIN);
  lv_obj_set_style_text_align(lbl_wifi_state, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_align(lbl_wifi_state, LV_ALIGN_TOP_MID, 0, 10);
  // Kezdeti WiFi állapot kijelzés
  updateWiFiConnectionState();  // Frissíti az isTransmitterConnected változót
  if (isTransmitterConnected) {
    lv_label_set_text(lbl_wifi_state, LV_SYMBOL_WIFI);
    lv_obj_set_style_text_color(lbl_wifi_state, lv_color_hex(0x00ff00), LV_PART_MAIN);
  } else {
    lv_label_set_text(lbl_wifi_state, "Nincs kapcsolat az adoval!");
    lv_obj_set_style_text_color(lbl_wifi_state, lv_color_hex(0xff0000), LV_PART_MAIN);
  }
  previousConnectionState = isTransmitterConnected;  // Fontos a wifi_status_update_cb-nek

  /*
  // Első frissítés
  if (isTransmitterConnected) {
    lv_label_set_text(lbl_wifi_state, LV_SYMBOL_WIFI);
    lv_obj_set_style_text_color(lbl_wifi_state, lv_color_hex(0x00ff00), LV_PART_MAIN);
  } else {
    lv_label_set_text(lbl_wifi_state, "Nincs kapcsolat az adoval!");
    lv_obj_set_style_text_color(lbl_wifi_state, lv_color_hex(0xff0000), LV_PART_MAIN);
  }
  */
  lbl_time_receiver = lv_label_create(lv_screen_active());
  lv_obj_set_style_text_font(lbl_time_receiver, &lv_font_montserrat_36, LV_PART_MAIN);
  //lv_label_set_text(lbl_time_receiver, "00:00:00");
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

  lv_obj_t *btn_reset = lv_button_create(lv_screen_active());
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

  receiver_stopper_timer = lv_timer_create(update_receiver_stopper_cb, 1000, NULL);
  lv_timer_create(wifi_status_update_cb, 1000, NULL);
}

//////////////////// Receiver Timer////////////////////////////
//static lv_timer_t *receiver_timer = nullptr;
//static lv_obj_t *lbl_time_receiver_timer = nullptr;
//static uint32_t receiver_timer_sec = 0;

static void update_receiver_timer_cb(lv_timer_t *) {
  if (!switchState && receiver_timer_sec > 0) {
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
  //btn_exit = createExitButton();
  lv_obj_t* exit_button = createExitButton();

  lbl_wifi_state = lv_label_create(lv_screen_active());
  lv_obj_set_style_text_font(lbl_wifi_state, &lv_font_montserrat_18, LV_PART_MAIN);
  lv_obj_set_style_text_align(lbl_wifi_state, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_align(lbl_wifi_state, LV_ALIGN_TOP_MID, 0, 10);
  
  updateWiFiConnectionState(); 
  // Első frissítés
  if (isTransmitterConnected) {
    lv_label_set_text(lbl_wifi_state, LV_SYMBOL_WIFI);
    lv_obj_set_style_text_color(lbl_wifi_state, lv_color_hex(0x00ff00), LV_PART_MAIN);
  } else {
    lv_label_set_text(lbl_wifi_state, "Nincs kapcsolat az adoval!");
    lv_obj_set_style_text_color(lbl_wifi_state, lv_color_hex(0xff0000), LV_PART_MAIN);
  }

  lbl_time_receiver_timer = lv_label_create(lv_screen_active());
  lv_obj_set_style_text_font(lbl_time_receiver_timer, &lv_font_montserrat_36, LV_PART_MAIN);
  //lv_label_set_text(lbl_time_receiver_timer, "00:00:00");
  char buf_init[16];
  uint32_t sec_init = receiver_timer_sec / 1000;
  uint32_t min_init = sec_init / 60;
  uint32_t hour_init = min_init / 60;
  sprintf(buf_init, "%02u:%02u:%02u", hour_init, min_init % 60, sec_init % 60);
  lv_label_set_text(lbl_time_receiver, buf_init);
  lv_obj_align(lbl_time_receiver_timer, LV_ALIGN_TOP_MID, 0, 60);

  if (!shared_wifi_status_timer) {
        shared_wifi_status_timer = lv_timer_create(wifi_status_update_cb, 1000, NULL);
    }

  lv_obj_t *btn_reset = lv_button_create(lv_screen_active());
  lv_obj_set_pos(btn_reset, 100, 120);
  lv_obj_set_size(btn_reset, 120, 40);
  lv_obj_t *lbl_reset = lv_label_create(btn_reset);
  lv_label_set_text(lbl_reset, "Reset");
  lv_obj_center(lbl_reset);
  lv_obj_add_event_cb(
    btn_reset, [](lv_event_t *e) {
      receiver_timer_sec = 0;
      lv_label_set_text(lbl_time_receiver_timer, "00:00:00");
    },
    LV_EVENT_CLICKED, NULL);

  lv_obj_t *btn_plus = lv_button_create(lv_screen_active());
  lv_obj_set_pos(btn_plus, 20, 120);
  lv_obj_set_size(btn_plus, 60, 40);
  lv_obj_t *lbl_plus = lv_label_create(btn_plus);
  lv_label_set_text(lbl_plus, "+1 perc");
  lv_obj_center(lbl_plus);
  lv_obj_add_event_cb(
    btn_plus, [](lv_event_t *e) {
      receiver_timer_sec += 60;
      uint32_t sec = receiver_timer_sec % 60;
      uint32_t min = (receiver_timer_sec / 60) % 60;
      uint32_t hour = receiver_timer_sec / 3600;
      char buf[16];
      sprintf(buf, "%02u:%02u:%02u", hour, min, sec);
      lv_label_set_text(lbl_time_receiver_timer, buf);
    },
    LV_EVENT_CLICKED, NULL);

  lv_obj_t *btn_minus = lv_button_create(lv_screen_active());
  lv_obj_set_pos(btn_minus, 240, 120);
  lv_obj_set_size(btn_minus, 60, 40);
  lv_obj_t *lbl_minus = lv_label_create(btn_minus);
  lv_label_set_text(lbl_minus, "-1 perc");
  lv_obj_center(lbl_minus);
  lv_obj_add_event_cb(
    btn_minus, [](lv_event_t *e) {
      if (receiver_timer_sec >= 60) receiver_timer_sec -= 60;
      uint32_t sec = receiver_timer_sec % 60;
      uint32_t min = (receiver_timer_sec / 60) % 60;
      uint32_t hour = receiver_timer_sec / 3600;
      char buf[16];
      sprintf(buf, "%02u:%02u:%02u", hour, min, sec);
      lv_label_set_text(lbl_time_receiver_timer, buf);
    },
    LV_EVENT_CLICKED, NULL);

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

  receiver_timer = lv_timer_create(update_receiver_timer_cb, 1000, NULL);
  //lv_timer_create(wifi_status_update_cb, 1000, NULL);
}

//////////////////// Stopper////////////////////////////
//static lv_timer_t *stopper_timer = nullptr;
//static lv_obj_t *lbl_time_stopper = nullptr;
//static bool running = false;
//static uint32_t elapsed_ms = 0;
//static uint32_t count = 1;
static lv_obj_t *lbl_hal = nullptr;

static void update_stopper_cb(lv_timer_t *t) {
  if (running) {
    elapsed_ms += 1000;
    uint32_t sec = elapsed_ms / 1000;
    uint32_t min = sec / 60;
    uint32_t hour = min / 60;
    sec %= 60;
    char buf[16];
    sprintf(buf, "%02u:%02u:%02u", hour, min, sec);
    lv_label_set_text(lbl_time_stopper, buf);
  }
}

void go_stopper(void) {
  //btn_exit = createExitButton();
  lv_obj_t *exit_btn_local = createExitButton();  // Lehet lokális is
  lv_obj_add_event_cb(
    exit_btn_local, [](lv_event_t *e) {
      if (stopper_timer) {  // Ellenőrizzük, hogy létezik-e a timer
        lv_timer_del(stopper_timer);
        stopper_timer = nullptr;  // Fontos a nullázás
      }
      // Ha a 'running' állapotot is resetelni kell kilépéskor:
      running = false;
      elapsed_ms = 0;  // Ha az időt is nullázni kell

      lv_obj_clean(lv_screen_active());
      main_screen();
    },
    LV_EVENT_CLICKED, NULL);

  //gombokat középre
  lbl_time_stopper = lv_label_create(lv_screen_active());
  lv_obj_set_style_text_font(lbl_time_stopper, &lv_font_montserrat_36, LV_PART_MAIN);
  lv_label_set_text(lbl_time_stopper, "00:00:00");
  running = false; // Ha itt kell nullázni az állapotot
  elapsed_ms = 0;
  lv_obj_align(lbl_time_stopper, LV_ALIGN_TOP_MID, 0, 30);

  //Gomb konténer
  /*lv_obj_t *btn_container = lv_obj_create(lv_screen_active());
  lv_obj_set_size(btn_container, 300, 120);
  lv_obj_center(btn_container);
  lv_obj_align(btn_container, LV_ALIGN_TOP_MID, 0, 100);       // képernyő közepére
  lv_obj_set_flex_flow(btn_container, LV_FLEX_FLOW_ROW_WRAP);  // vízszintes elrendezés töréssel
  lv_obj_set_style_pad_row(btn_container, 10, 0);
  */
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
  //lv_timer_ready(stopper_timer);
}

//////////////////// Timer ////////////////////////////
//static lv_timer_t *timer_timer = nullptr;
//static lv_obj_t *lbl_time_timer = nullptr;
//static uint32_t timer_duration_sec = 0;
//static bool timer_running = false;

static void update_timer_cb(lv_timer_t *) {
  //if (timer_running && timer_duration_sec > 0) {
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


  //btn_exit = createExitButton();
  lv_obj_t *exit_btn_local = createExitButton();  // Lehet lokális is
  lv_obj_add_event_cb(
    exit_btn_local, [](lv_event_t *e) {
      if (timer_timer) {  // Ellenőrizzük, hogy létezik-e a timer
        lv_timer_del(timer_timer);
        timer_timer = nullptr;  // Fontos a nullázás
      }
      // Ha a 'running' állapotot is resetelni kell kilépéskor:
      timer_running = false;
      timer_duration_sec = 0;  // Ha az időt is nullázni kell
      lv_obj_clean(lv_screen_active());
      main_screen();
    },
    LV_EVENT_CLICKED, NULL);

  lbl_time_timer = lv_label_create(lv_screen_active());
  lv_obj_set_style_text_font(lbl_time_timer, &lv_font_montserrat_36, LV_PART_MAIN);
  timer_running = false; // Ha itt kell nullázni
  timer_duration_sec = 0;
  lv_label_set_text(lbl_time_timer, "00:00:00");
  lv_obj_align(lbl_time_timer, LV_ALIGN_TOP_MID, 0, 30);

  // Gomb konténer létrehozása
  /*
  lv_obj_t *btn_container = lv_obj_create(lv_screen_active());
  lv_obj_set_size(btn_container, 300, 120);
  lv_obj_center(btn_container);
  lv_obj_align(btn_container, LV_ALIGN_TOP_MID, 0, 100);       // képernyő közepére
  lv_obj_set_flex_flow(btn_container, LV_FLEX_FLOW_ROW_WRAP);  // vízszintes elrendezés töréssel
  lv_obj_set_style_pad_row(btn_container, 10, 0);
  */

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
  //lv_timer_ready(timer_timer);
}

void loop() {
  Serial.println(WiFi.softAPgetHostname());
  server.handleClient();
  lv_task_handler();
  lv_tick_inc(5);
  delay(5);
}