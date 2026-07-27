#ifndef ACTUATORS_H
#define ACTUATORS_H

#include <LiquidCrystal_I2C.h>
#include "types.h"
#include "config.h"
#include "pins.h"

extern void setLEDColor(uint8_t r, uint8_t g, uint8_t b);
extern volatile bool networkOnline;
extern volatile bool networkBeeping;
extern volatile bool ledTesting;

class ActuatorsManager {
private:
  LiquidCrystal_I2C lcd;
  String            lastLCDLine0;
  String            lastLCDLine1;
  unsigned long     lastLCDWriteTime;

  // Menulis pembaruan LCD secara periodik jika teks berubah
  void updateLCD(const SensorReading& r, const SystemState& s) {
    char line0[17];
    char line1[17];

    const char* ovStr = (s.overallStatus == DANGER) ? "DGR"
                      : (s.overallStatus == WARNING) ? "WRN" : "NRM";

    if (s.overallStatus == DANGER) {
      bool flameD = (s.flameStatus == DANGER);
      if (flameD) {
        snprintf(line0, sizeof(line0), "!! FIRE ALARM !!");
        snprintf(line1, sizeof(line1), " EVACUATE NOW!! ");
      } else {
        snprintf(line0, sizeof(line0), "! GAS DETECTED !");
        snprintf(line1, sizeof(line1), "OPEN VENTILATION");
      }
    } else {
      snprintf(line0, sizeof(line0), "T:%2.0fC H:%2.0f%%  %3s",
               isnan(r.temperature) ? 0.0 : round(r.temperature),
               isnan(r.humidity) ? 0.0 : round(r.humidity), ovStr);

      char pressBuf[10];
      int pressLen = snprintf(pressBuf, sizeof(pressBuf), "P:%.0f", r.pressure);
      const char* statusStr = networkOnline ? "ONLINE" : "OFFLINE";
      int spacesNeeded = 16 - pressLen - strlen(statusStr);
      if (spacesNeeded < 0) spacesNeeded = 0;
      snprintf(line1, sizeof(line1), "%s%*s%s", pressBuf, spacesNeeded, "", statusStr);
    }

    String l0 = String(line0);
    String l1 = String(line1);

    // Kirim perintah I2C hanya jika string berubah (optimalisasi refresh rate)
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

  // Mengontrol kedipan LED RGB secara asinkron berdasarkan status bahaya
  void updateLED(const SystemState& s, unsigned long now) {
    if (ledTesting) return; // Skip update if LED test is running on Core 0
    switch (s.overallStatus) {
      case NORMAL:
        // Hijau solid
        setLEDColor(0, 255, 0);
        break;

      case WARNING: {
        // Kuning lambat
        bool on = (now / LED_BLINK_YELLOW_MS) % 2 == 0;
        setLEDColor(on ? LED_PWM_YELLOW_R : 0, on ? LED_PWM_YELLOW_G : 0, 0);
        break;
      }

      case DANGER: {
        bool gasD   = (s.gasStatus   == DANGER);
        bool flameD = (s.flameStatus == DANGER);

        if (gasD && flameD) {
          // Kombinasi: bergantian merah-oranye cepat
          bool toggle = (now / LED_FLASH_FAST_MS) % 2 == 0;
          setLEDColor(toggle ? 255 : LED_PWM_ORANGE_R, toggle ? 0 : LED_PWM_ORANGE_G, 0);
        } else if (flameD) {
          // Api saja: merah berkedip cepat
          bool on = (now / LED_BLINK_RED_MS) % 2 == 0;
          setLEDColor(on ? 255 : 0, 0, 0);
        } else {
          // Gas saja: oranye berkedip cepat
          bool on = (now / LED_BLINK_ORANGE_MS) % 2 == 0;
          setLEDColor(on ? LED_PWM_ORANGE_R : 0, on ? LED_PWM_ORANGE_G : 0, 0);
        }
        break;
      }
    }
  }

  // Mengaktifkan buzzer dengan pola non-blocking
  void updateBuzzer(const SystemState& s, unsigned long now) {
    if (networkBeeping && s.overallStatus != DANGER) return; // Menghindari interupsi nada koneksi kecuali kondisi DANGER
    
    if (s.overallStatus != DANGER || s.buzzerMuted) {
      digitalWrite(PIN_BUZZER, LOW);
      return;
    }

    bool gasD   = (s.gasStatus   == DANGER);
    bool flameD = (s.flameStatus == DANGER);

    if (gasD && flameD) {
      // Kebocoran ganda: aktif terus-menerus
      digitalWrite(PIN_BUZZER, HIGH);
    } else if (flameD) {
      // Kebakaran saja: tempo cepat
      unsigned long t = now % BUZZER_FLAME_PERIOD_MS;
      digitalWrite(PIN_BUZZER, (t < (BUZZER_FLAME_PERIOD_MS / 2)) ? HIGH : LOW);
    } else {
      // Kebocoran gas saja: tempo lambat
      unsigned long t = now % BUZZER_GAS_PERIOD_MS;
      digitalWrite(PIN_BUZZER, (t < (BUZZER_GAS_PERIOD_MS / 2)) ? HIGH : LOW);
    }
  }

public:
  ActuatorsManager() : 
    lcd(I2C_ADDR_LCD, 16, 2),
    lastLCDLine0(""), lastLCDLine1(""), lastLCDWriteTime(0) {}

  void begin() {
    lcd.init();
    lcd.backlight();
    
    // Inisialisasi pin aktuator
    pinMode(PIN_BUZZER, OUTPUT);
    pinMode(PIN_LED_R, OUTPUT);
    pinMode(PIN_LED_G, OUTPUT);
    pinMode(PIN_LED_B, OUTPUT);
    digitalWrite(PIN_BUZZER, LOW);
    
    // Default awal warna biru
    setLEDColor(0, 0, 255);
  }

  // Menulis pesan secara paksa/raw pada LCD (saat booting)
  void writeRaw(const char* line0, const char* line1) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print(line0);
    lcd.setCursor(0, 1);
    lcd.print(line1);
    lastLCDLine0 = String(line0);
    lastLCDLine1 = String(line1);
  }

  // Pemrosesan asinkron untuk LED, Buzzer, dan refresh rate LCD
  void process(unsigned long now, const SensorReading& r, const SystemState& s) {
    updateLED(s, now);
    updateBuzzer(s, now);

    if (now - lastLCDWriteTime >= LCD_REFRESH_INTERVAL_MS) {
      lastLCDWriteTime = now;
      updateLCD(r, s);
    }
  }
};

#endif
