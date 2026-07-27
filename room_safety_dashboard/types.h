/*
 * types.h - Shared data structures and enums for the safety dashboard
 */

#ifndef TYPES_H
#define TYPES_H

// ====================== STATUS ======================
enum Status { NORMAL, WARNING, DANGER };

// ====================== SENSOR READING ======================
struct SensorReading {
  float temperature;  // °C  (AHT20)
  float humidity;     // %RH (AHT20)
  float pressure;     // hPa (BMP280)
  int   gasADC;       // nilai analog mentah MQ-2 (AO), 0–4095
  int   flameADC;     // nilai analog mentah Flame Sensor (AO), 0–4095
                      // (nilai RENDAH = api terdeteksi, kebalikan MQ-2)
};

// ====================== SYSTEM STATE ======================
struct SystemState {
  Status tempStatus    = NORMAL;
  Status humStatus     = NORMAL;
  Status presStatus    = NORMAL;
  Status gasStatus     = NORMAL;
  Status flameStatus   = NORMAL;
  Status overallStatus = NORMAL;
  bool   sensorError   = false;  // true jika AHT20/BMP280 gagal baca berturut-turut
  bool   buzzerMuted   = false;  // true jika di-mute via MQTT cmd
};

#endif  // TYPES_H
