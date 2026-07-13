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
#define MQTT_PORT       8883

// MQTT Topics
#define TOPIC_DATA      "iot/room-safety/data"
#define TOPIC_ALERT     "iot/room-safety/alert"
#define TOPIC_HEARTBEAT "iot/room-safety/heartbeat"

// ====================== TIMING (ms) ======================
#define PUBLISH_INTERVAL_MS     1000    // Data publish interval
#define HEARTBEAT_INTERVAL_MS   15000   // Online heartbeat interval
#define MQ2_WARMUP_MS           30000   // Sensor warm-up duration
#define NOTIF_COOLDOWN_MS       60000   // Telegram spam protection cooldown
#define WIFI_RECONNECT_MS       30000   // WiFi reconnect attempt interval
#define MQTT_RECONNECT_MS       10000   // MQTT reconnect attempt interval
#define ADC_SAMPLE_COUNT        10      // Multi-sample count to filter noise
#define DANGER_CONFIRM_COUNT    2       // Consecutive reads before triggering DANGER

// ====================== THRESHOLDS ======================
// Temperature (°C)
#define TEMP_DANGER_MIN   0.0    // beku — DANGER
#define TEMP_NORMAL_MIN   20.0   // zona nyaman
#define TEMP_NORMAL_MAX   25.0
#define TEMP_WARNING_MIN  18.0   // batas aman lebih lebar
#define TEMP_WARNING_MAX  28.0

// Humidity (%RH) — hanya NORMAL/WARNING
#define HUM_NORMAL_MIN    40.0
#define HUM_NORMAL_MAX    60.0
#define HUM_WARNING_MIN   30.0
#define HUM_WARNING_MAX   70.0

// Pressure (hPa) — hanya NORMAL/WARNING
#define PRES_NORMAL_MIN   950.0
#define PRES_NORMAL_MAX   1020.0

// MQ-2 Gas (ADC, 0–4095)
#define GAS_NORMAL_MAX    1000   // < 1000 = NORMAL
#define GAS_WARNING_MAX   1500   // 1000–1500 = WARNING, > 1500 = DANGER

// Flame Sensor (ADC, active-low — nilai rendah = api terdeteksi)
#define FLAME_NORMAL_MIN   3000
#define FLAME_WARNING_MIN  1500

// ====================== RGB LED CALIBRATION ======================
#define LED_PWM_YELLOW_R  175
#define LED_PWM_YELLOW_G  75
#define LED_PWM_ORANGE_R  220
#define LED_PWM_ORANGE_G  55

#endif  // CONFIG_H