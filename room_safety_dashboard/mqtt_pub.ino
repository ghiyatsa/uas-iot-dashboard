/*
 * mqtt_pub.ino - High-performance MQTT publication module.
 * Menggunakan ArduinoJson. Payload mencakup flame sensor dan NTP timestamp.
 */

#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <time.h>
#include "types.h"
#include "config.h"

extern PubSubClient mqttClient;
String statusToString(Status s);

// Ambil Unix timestamp dari NTP (0 jika belum sync)
static time_t getUnixTime() {
  time_t now = 0;
  struct tm ti;
  if (getLocalTime(&ti, 10)) {
    now = mktime(&ti);
  }
  return now;
}

// ====================== PUBLISH DATA ======================
bool publishDataManual(const SensorReading& r, const SystemState& s) {
  StaticJsonDocument<320> doc;
  
  doc["t"]  = isnan(r.temperature) ? 0.0f : r.temperature;
  doc["h"]  = isnan(r.humidity)    ? 0.0f : r.humidity;
  doc["p"]  = isnan(r.pressure)    ? 0.0f : r.pressure;
  doc["g"]  = r.gasADC;
  doc["f"]  = r.flameADC;                       // flame ADC (active-low)
  doc["ts"] = statusToString(s.tempStatus);
  doc["hs"] = statusToString(s.humStatus);
  doc["ps"] = statusToString(s.presStatus);
  doc["gs"] = statusToString(s.gasStatus);
  doc["fs"] = statusToString(s.flameStatus);    // flame status
  doc["os"] = statusToString(s.overallStatus);
  doc["se"] = s.sensorError;                    // sensor error flag
  doc["bm"] = s.buzzerMuted;                    // buzzer muted flag
  doc["hf"] = (int)ESP.getFreeHeap();           // free heap size
  doc["ms"] = millis();
  doc["ts_unix"] = (long)getUnixTime();

  char buffer[320];
  serializeJson(doc, buffer);

  return mqttClient.publish(TOPIC_DATA, buffer);
}

// ====================== PUBLISH ALERT ======================
bool publishAlertManual(const SensorReading& r, Status overall) {
  StaticJsonDocument<384> doc;
  
  doc["overall_status"] = statusToString(overall);
  doc["temperature"]    = isnan(r.temperature) ? 0.0f : r.temperature;
  doc["humidity"]       = isnan(r.humidity)    ? 0.0f : r.humidity;
  doc["pressure"]       = isnan(r.pressure)    ? 0.0f : r.pressure;
  doc["gas_adc"]        = r.gasADC;
  doc["flame_adc"]      = r.flameADC;
  doc["message"]        = "Kondisi DANGER terdeteksi, periksa ruangan segera!";
  doc["timestamp"]      = millis();
  doc["ts_unix"]        = (long)getUnixTime();

  char buffer[384];
  serializeJson(doc, buffer);

  return mqttClient.publish(TOPIC_ALERT, buffer);
}

// ====================== PUBLISH HEARTBEAT ======================
bool publishHeartbeatManual() {
  StaticJsonDocument<128> doc;
  
  doc["status"]    = "online";
  doc["uptime_ms"] = millis();
  doc["heap_free"] = (int)ESP.getFreeHeap();
  doc["ts_unix"]   = (long)getUnixTime();

  char buffer[128];
  serializeJson(doc, buffer);

  return mqttClient.publish(TOPIC_HEARTBEAT, buffer, true); // Set retain = true agar broker menyimpan status online ter-update
}
