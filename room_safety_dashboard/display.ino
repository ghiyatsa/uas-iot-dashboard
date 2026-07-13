/*
 * display.ino - Asynchronous actuator control (LCD 16x2, RGB LED, Buzzer).
 * Implements non-blocking toggles and LCD writing optimized to execute only on state/text changes.
 */

#include <LiquidCrystal_I2C.h>

// Instantiate LCD
LiquidCrystal_I2C lcd(I2C_ADDR_LCD, 16, 2);

extern SystemState   localState;
extern SensorReading localReading;
extern volatile bool networkOnline;

// Static buffers for tracking changes
static String lastLCDLine0 = "";
static String lastLCDLine1 = "";
static unsigned long lastLCDWriteTime = 0;

// ====================== INIT DISPLAY ======================
void initDisplay() {
  lcd.init();
  lcd.backlight();
}

// ====================== LCD WRITE RAW (Setup/Boot) ======================
void lcdWriteRaw(const char* line0, const char* line1) {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(line0);
  lcd.setCursor(0, 1);
  lcd.print(line1);
  // Simpan state agar updateLCD berikutnya tahu text saat ini
  lastLCDLine0 = String(line0);
  lastLCDLine1 = String(line1);
}

// ====================== UPDATE LCD ======================
void updateLCD(const SensorReading& r, Status overall) {
  char line0[17];
  char line1[17];

  // Singkatan 3-char untuk status keseluruhan
  const char* ovStr = (overall == DANGER) ? "DGR"
                    : (overall == WARNING) ? "WRN" : "NRM";

  if (overall == DANGER) {
    bool gasD   = (localState.gasStatus   == DANGER);
    bool flameD = (localState.flameStatus == DANGER);

    if (flameD) {
      snprintf(line0, sizeof(line0), "!! FIRE ALARM !!");
      snprintf(line1, sizeof(line1), " EVACUATE NOW!! ");
    } else {
      snprintf(line0, sizeof(line0), "! GAS DETECTED !");
      snprintf(line1, sizeof(line1), "OPEN VENTILATION");
    }
  } else {
    // NORMAL / WARNING
    // Baris 0: "T:28.5C H:65% NRM" (16 karakter)
    snprintf(line0, sizeof(line0), "T:%2.0fC H:%2.0f%%  %3s",
             isnan(r.temperature) ? 0.0 : round(r.temperature),
             isnan(r.humidity) ? 0.0 : round(r.humidity), ovStr);

    // Baris 1: "P:1012hPa  ONLINE" atau "P:1012hPa OFFLINE" (16 karakter)
    char pressBuf[10];
    int pressLen = snprintf(pressBuf, sizeof(pressBuf), "P:%.0f", r.pressure);
    const char* statusStr = networkOnline ? "ONLINE" : "OFFLINE";
    int spacesNeeded = 16 - pressLen - strlen(statusStr);
    
    // Pastikan spacesNeeded tidak negatif
    if (spacesNeeded < 0) spacesNeeded = 0;
    snprintf(line1, sizeof(line1), "%s%*s%s", pressBuf, spacesNeeded, "", statusStr);
  }

  String l0 = String(line0);
  String l1 = String(line1);

  // Optimasi: Tulis hanya jika text berubah
  if (l0 != lastLCDLine0) {
    lcd.setCursor(0, 0);
    lcd.print(l0);
    lastLCDLine0 = l0;
  }
  if (l1 != lastLCDLine1) {
    lcd.setCursor(0, 1);
    lcd.print(l1);
    lastLCDLine1 = l1;
  }
}

// ====================== UPDATE LED RGB ======================
void updateLED(Status overall, unsigned long now) {
  switch (overall) {
    case NORMAL:
      // Hijau SOLID — semua aman
      analogWrite(PIN_LED_R, 0);
      analogWrite(PIN_LED_G, 255);
      analogWrite(PIN_LED_B, 0);
      break;

    case WARNING: {
      // Kuning BLINK lambat (500ms)
      bool on = (now / 500) % 2 == 0;
      analogWrite(PIN_LED_R, on ? LED_PWM_YELLOW_R : 0);
      analogWrite(PIN_LED_G, on ? LED_PWM_YELLOW_G : 0);
      analogWrite(PIN_LED_B, 0);
      break;
    }

    case DANGER: {
      bool gasD   = (localState.gasStatus   == DANGER);
      bool flameD = (localState.flameStatus == DANGER);

      if (gasD && flameD) {
        // Gas + Api: bergantian Merah ↔ Oranye cepat (150ms)
        bool toggle = (now / 150) % 2 == 0;
        analogWrite(PIN_LED_R, toggle ? 255 : LED_PWM_ORANGE_R);
        analogWrite(PIN_LED_G, toggle ? 0   : LED_PWM_ORANGE_G);
        analogWrite(PIN_LED_B, 0);
      } else if (flameD) {
        // Api saja: Merah BLINK cepat (200ms)
        bool on = (now / 200) % 2 == 0;
        analogWrite(PIN_LED_R, on ? 255 : 0);
        analogWrite(PIN_LED_G, 0);
        analogWrite(PIN_LED_B, 0);
      } else {
        // Gas saja: Oranye BLINK cepat (200ms)
        bool on = (now / 200) % 2 == 0;
        analogWrite(PIN_LED_R, on ? LED_PWM_ORANGE_R : 0);
        analogWrite(PIN_LED_G, on ? LED_PWM_ORANGE_G : 0);
        analogWrite(PIN_LED_B, 0);
      }
      break;
    }
  }
}

// ====================== UPDATE BUZZER ======================
void updateBuzzer(Status overall, unsigned long now) {
  if (overall != DANGER) {
    digitalWrite(PIN_BUZZER, LOW);
    return;
  }

  bool gasD   = (localState.gasStatus   == DANGER);
  bool flameD = (localState.flameStatus == DANGER);

  if (gasD && flameD) {
    // Gas + Api: menyala terus menerus
    digitalWrite(PIN_BUZZER, HIGH);
  } else if (flameD) {
    // Api saja: 100ms ON / 100ms OFF
    unsigned long t = now % 200UL;
    digitalWrite(PIN_BUZZER, (t < 100) ? HIGH : LOW);
  } else {
    // Gas saja: 400ms ON / 400ms OFF
    unsigned long t = now % 800UL;
    digitalWrite(PIN_BUZZER, (t < 400) ? HIGH : LOW);
  }
}

// ====================== UPDATE ACTUATORS NON-BLOCKING ======================
void updateActuatorsNonBlocking(unsigned long now) {
  // Update LED & Buzzer secara kontinu untuk menjamin kelancaran blink/nada
  updateLED(localState.overallStatus, now);
  updateBuzzer(localState.overallStatus, now);

  // Update LCD setiap 250ms saja untuk menghindari overloading I2C
  if (now - lastLCDWriteTime >= 250UL) {
    lastLCDWriteTime = now;
    updateLCD(localReading, localState.overallStatus);
  }
}
