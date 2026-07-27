/*
 * config.h - System timing, thresholds, and calibration constants.
 *
 * Credentials (WiFi, MQTT, Telegram) dipindah ke config_secret.h
 * yang TIDAK di-commit ke Git. Salin config_secret.h.example →
 * config_secret.h lalu isi nilainya sebelum compile.
 */

#ifndef CONFIG_H
#define CONFIG_H

#include "config_secret.h"

// ====================== MQTT ======================
constexpr uint16_t MQTT_PORT = 8883;

// MQTT Topics
constexpr const char* TOPIC_DATA      = "iot/room-safety/data";
constexpr const char* TOPIC_ALERT     = "iot/room-safety/alert";
constexpr const char* TOPIC_HEARTBEAT = "iot/room-safety/heartbeat";
constexpr const char* TOPIC_CMD       = "iot/room-safety/cmd/#";
constexpr const char* TOPIC_CMD_MUTE  = "iot/room-safety/cmd/mute";
constexpr const char* TOPIC_CMD_LED   = "iot/room-safety/cmd/test_led";
constexpr const char* TOPIC_CMD_TG    = "iot/room-safety/cmd/telegram";

// ====================== NTP ======================
constexpr const char* NTP_SERVER      = "pool.ntp.org";
constexpr long        GMT_OFFSET_SEC  = 7 * 3600; // WIB UTC+7
constexpr int         DST_OFFSET_SEC  = 0;

// ====================== OTA ======================
constexpr const char* OTA_HOSTNAME    = "esp32-room-safety";
constexpr const char* OTA_PASSWORD    = "iot2026";

// ====================== TIMING (ms) ======================
constexpr unsigned long PUBLISH_INTERVAL_MS   = 500;    // Data publish interval
constexpr unsigned long HEARTBEAT_INTERVAL_MS = 15000;  // Online heartbeat interval
constexpr unsigned long MQ2_WARMUP_MS         = 30000;  // Sensor warm-up duration
constexpr unsigned long NOTIF_COOLDOWN_MS     = 60000;  // Telegram spam protection cooldown
constexpr unsigned long WIFI_RECONNECT_MS     = 30000;  // WiFi reconnect attempt interval
constexpr unsigned long MQTT_RECONNECT_MS     = 10000;  // MQTT reconnect attempt interval
constexpr int           ADC_SAMPLE_COUNT      = 10;     // Multi-sample count to filter noise
constexpr int           DANGER_CONFIRM_COUNT  = 2;      // Consecutive reads before triggering DANGER

// Actuator & Network Timing (ms)
constexpr unsigned long LCD_REFRESH_INTERVAL_MS     = 250;
constexpr unsigned long TELEGRAM_POLL_INTERVAL_MS   = 5000;
constexpr unsigned long MQTT_ALERT_COOLDOWN_MS      = 30000;

// LED Blink Periods (ms)
constexpr unsigned long LED_BLINK_YELLOW_MS = 500;
constexpr unsigned long LED_BLINK_ORANGE_MS = 200;
constexpr unsigned long LED_BLINK_RED_MS    = 200;
constexpr unsigned long LED_FLASH_FAST_MS   = 150;

// Buzzer Patterns (ms)
constexpr unsigned long BUZZER_FLAME_PERIOD_MS = 200;
constexpr unsigned long BUZZER_GAS_PERIOD_MS   = 800;

// ====================== THRESHOLDS ======================
// Temperature (°C) — disesuaikan iklim tropis Indonesia
constexpr float TEMP_DANGER_MIN  = 10.0;   // terlalu dingin (AC rusak/ekstrem) — DANGER
constexpr float TEMP_DANGER_MAX  = 40.0;   // over-heat nyata di ruangan — DANGER
constexpr float TEMP_NORMAL_MIN  = 24.0;   // zona nyaman ber-AC
constexpr float TEMP_NORMAL_MAX  = 30.0;   // zona nyaman tanpa AC
constexpr float TEMP_WARNING_MIN = 20.0;   // AC terlalu dingin
constexpr float TEMP_WARNING_MAX = 36.0;   // panas tapi belum bahaya

// Humidity (%RH) — disesuaikan iklim lembap tropis Indonesia
constexpr float HUM_NORMAL_MIN   = 50.0;   // batas bawah normal tropis
constexpr float HUM_NORMAL_MAX   = 80.0;   // batas atas normal tropis
constexpr float HUM_WARNING_MIN  = 35.0;   // sangat kering (AC berlebihan)
constexpr float HUM_WARNING_MAX  = 90.0;   // sangat lembap (risiko jamur/kondensasi)

// Pressure (hPa) — hanya NORMAL/WARNING
constexpr float PRES_NORMAL_MIN  = 950.0;
constexpr float PRES_NORMAL_MAX  = 1015.0; // realistis untuk dataran rendah Indonesia

// MQ-2 Gas (ADC, 0–4095)
constexpr int GAS_NORMAL_MAX  = 1000;   // < 1000 = NORMAL
constexpr int GAS_WARNING_MAX = 1500;   // 1000–1500 = WARNING, > 1500 = DANGER

// Flame Sensor (ADC, active-low — nilai rendah = api terdeteksi)
constexpr int FLAME_NORMAL_MIN  = 3000;
constexpr int FLAME_WARNING_MIN = 1500;

// ====================== RGB LED CALIBRATION ======================
constexpr uint8_t LED_PWM_YELLOW_R = 175;
constexpr uint8_t LED_PWM_YELLOW_G = 75;
constexpr uint8_t LED_PWM_ORANGE_R = 220;
constexpr uint8_t LED_PWM_ORANGE_G = 55;

#endif  // CONFIG_H