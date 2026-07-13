/*
 * mqtt_pub.ino - High-performance MQTT publication module.
 * Eliminates ArduinoJson library overhead by using manual snprintf JSON formatting,
 * reducing CPU cycles and avoiding dynamic memory allocation.
 */

#include <PubSubClient.h>
#include "types.h"
#include "config.h"

extern PubSubClient mqttClient;
String statusToString(Status s);

// ====================== PUBLISH DATA (MANUAL FORMAT) ======================
bool publishDataManual(const SensorReading& r, const SystemState& s) {
  char buffer[256];
  
  // Format JSON manual dengan keys pendek untuk menghemat bandwidth
  snprintf(buffer, sizeof(buffer),
           "{\"t\":%.1f,\"h\":%.1f,\"p\":%.1f,\"g\":%d,\"ts\":\"%s\",\"hs\":\"%s\",\"ps\":\"%s\",\"gs\":\"%s\",\"os\":\"%s\",\"ms\":%lu}",
           isnan(r.temperature) ? 0.0f : r.temperature,
           isnan(r.humidity) ? 0.0f : r.humidity,
           isnan(r.pressure) ? 0.0f : r.pressure,
           r.gasADC,
           statusToString(s.tempStatus).c_str(),
           statusToString(s.humStatus).c_str(),
           statusToString(s.presStatus).c_str(),
           statusToString(s.gasStatus).c_str(),
           statusToString(s.overallStatus).c_str(),
           millis());

  return mqttClient.publish(TOPIC_DATA, buffer);
}

// ====================== PUBLISH ALERT (MANUAL FORMAT) ======================
bool publishAlertManual(const SensorReading& r, Status overall) {
  char buffer[320];
  
  snprintf(buffer, sizeof(buffer),
           "{\"overall_status\":\"%s\",\"temperature\":%.1f,\"humidity\":%.1f,\"pressure\":%.1f,\"gas_adc\":%d,\"message\":\"Kondisi DANGER terdeteksi, periksa ruangan segera!\",\"timestamp\":%lu}",
           statusToString(overall).c_str(),
           isnan(r.temperature) ? 0.0f : r.temperature,
           isnan(r.humidity) ? 0.0f : r.humidity,
           isnan(r.pressure) ? 0.0f : r.pressure,
           r.gasADC,
           millis());

  return mqttClient.publish(TOPIC_ALERT, buffer);
}

// ====================== PUBLISH HEARTBEAT (MANUAL FORMAT) ======================
bool publishHeartbeatManual() {
  char buffer[64];
  
  snprintf(buffer, sizeof(buffer),
           "{\"status\":\"online\",\"uptime_ms\":%lu}",
           millis());

  return mqttClient.publish(TOPIC_HEARTBEAT, buffer);
}
