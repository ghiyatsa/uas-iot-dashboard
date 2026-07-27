#ifndef SENSORS_H
#define SENSORS_H

#include <Adafruit_AHTX0.h>
#include <Adafruit_BMP280.h>
#include "types.h"
#include "config.h"
#include "pins.h"

extern SemaphoreHandle_t stateMutex;
extern SensorReading     sharedReading;
extern SystemState       sharedState;
extern volatile bool     networkOnline;
extern String statusToString(Status s);
extern void printTimestamp();

class SensorsManager {
private:
  Adafruit_AHTX0  aht;
  Adafruit_BMP280 bmp;

  bool          mq2Ready;
  bool          flameReady;        // warmup flag untuk flame sensor
  unsigned long bootTime;

  // State sampling analog non-blocking
  unsigned long lastAnalogSampleTime;
  unsigned long lastI2cSampleTime;
  int           analogSampleCount;
  long          gasSampleSum;
  long          flameSampleSum;

  // Latch DANGER untuk stabilitas pembacaan
  unsigned long lastGasDangerTime;
  unsigned long lastFlameDangerTime;

  // Counter konfirmasi DANGER gas — PINDAH dari static ke member
  int           dangerConfirmCounter;

  // Sensor health: hitung berapa kali NaN berturut-turut
  int           i2cNanCount;
  static constexpr int I2C_NAN_LIMIT = 5;  // 5× NaN berturut = sensor error

  Status latchDanger(Status eval, unsigned long &lastTime, unsigned long now, unsigned long latchMs = 10000UL) {
    if (eval == DANGER) {
      lastTime = now;
      return DANGER;
    }
    if (lastTime > 0 && (now - lastTime < latchMs)) return DANGER;
    return eval;
  }

  Status evaluateTemp(float t) {
    if (t < TEMP_DANGER_MIN || t >= TEMP_DANGER_MAX) return DANGER;
    if (t >= TEMP_NORMAL_MIN && t <= TEMP_NORMAL_MAX) return NORMAL;
    if (t >= TEMP_WARNING_MIN && t <= TEMP_WARNING_MAX) return WARNING;
    return WARNING;
  }

  Status evaluateHumidity(float h) {
    if (h >= HUM_NORMAL_MIN && h <= HUM_NORMAL_MAX) return NORMAL;
    if (h >= HUM_WARNING_MIN && h <= HUM_WARNING_MAX) return WARNING;
    return WARNING;
  }

  Status evaluatePressure(float p) {
    if (p < PRES_NORMAL_MIN || p > PRES_NORMAL_MAX) return WARNING;
    return NORMAL;
  }

  Status evaluateGas(int adc) {
    if (adc > GAS_WARNING_MAX) {
      dangerConfirmCounter++;
      if (dangerConfirmCounter >= DANGER_CONFIRM_COUNT) return DANGER;
      return WARNING;
    }
    dangerConfirmCounter = 0;
    if (adc > GAS_NORMAL_MAX) return WARNING;
    return NORMAL;
  }

  Status evaluateFlame(int adc) {
    if (adc < FLAME_WARNING_MIN)  return DANGER;
    if (adc < FLAME_NORMAL_MIN)   return WARNING;
    return NORMAL;
  }

  Status worstStatus(Status a, Status b) {
    if (a == DANGER  || b == DANGER)  return DANGER;
    if (a == WARNING || b == WARNING) return WARNING;
    return NORMAL;
  }

public:
  SensorsManager() : 
    mq2Ready(false), flameReady(false), bootTime(0),
    lastAnalogSampleTime(0), lastI2cSampleTime(0), analogSampleCount(0),
    gasSampleSum(0), flameSampleSum(0),
    lastGasDangerTime(0), lastFlameDangerTime(0),
    dangerConfirmCounter(0), i2cNanCount(0) {}

  void begin() {
    if (!aht.begin()) {
      Serial.println("[INIT] ERROR: AHT20 tidak terdeteksi!");
    } else {
      Serial.println("[INIT] Sensor AHT20: OK");
    }

    if (!bmp.begin(I2C_ADDR_BMP280)) {
      Serial.println("[INIT] ERROR: BMP280 tidak terdeteksi!");
    } else {
      Serial.println("[INIT] Sensor BMP280: OK");
      bmp.setSampling(Adafruit_BMP280::MODE_NORMAL,
                      Adafruit_BMP280::SAMPLING_X2,
                      Adafruit_BMP280::SAMPLING_X16,
                      Adafruit_BMP280::FILTER_X16,
                      Adafruit_BMP280::STANDBY_MS_500);
    }
    bootTime      = millis();
  }

  void process(unsigned long now, SensorReading& localReading, SystemState& localState) {
    // Cek warmup MQ-2
    if (!mq2Ready && (now - bootTime >= MQ2_WARMUP_MS)) {
      mq2Ready = true;
      Serial.println("[INIT] MQ-2 siap digunakan.");
    }

    // Cek warmup Flame sensor — 5 detik cukup untuk stabilisasi
    if (!flameReady && (now - bootTime >= 5000UL)) {
      flameReady = true;
      Serial.println("[INIT] Flame sensor siap digunakan.");
    }

    // ── 1. SAMPLING ANALOG (MQ-2 & FLAME) SETIAP 5ms ──────────────────────────
    if (now - lastAnalogSampleTime >= 5UL) {
      lastAnalogSampleTime = now;
      
      gasSampleSum   += analogRead(PIN_MQ2_AO);
      flameSampleSum += analogRead(PIN_FLAME_AO);
      analogSampleCount++;
      
      if (analogSampleCount >= ADC_SAMPLE_COUNT) {
        localReading.gasADC   = gasSampleSum / ADC_SAMPLE_COUNT;
        localReading.flameADC = flameSampleSum / ADC_SAMPLE_COUNT;
        
        gasSampleSum      = 0;
        flameSampleSum    = 0;
        analogSampleCount = 0;
        
        localState.gasStatus   = latchDanger(mq2Ready   ? evaluateGas(localReading.gasADC)     : NORMAL, lastGasDangerTime,   now);
        localState.flameStatus = latchDanger(flameReady ? evaluateFlame(localReading.flameADC) : NORMAL, lastFlameDangerTime, now);
        
        sync(localReading, localState);
      }
    }

    // ── 2. SAMPLING I2C (AHT20 & BMP280) SETIAP 1000ms ───────────────────────
    if (now - lastI2cSampleTime >= 1000UL) {
      lastI2cSampleTime = now;
      
      sensors_event_t humEvent, tempEvent;
      aht.getEvent(&humEvent, &tempEvent);
      localReading.temperature = tempEvent.temperature;
      localReading.humidity    = humEvent.relative_humidity;
      localReading.pressure    = bmp.readPressure() / 100.0F;

      // Sensor health: deteksi NaN berturut-turut
      if (isnan(localReading.temperature) || isnan(localReading.humidity) || isnan(localReading.pressure)) {
        i2cNanCount++;
        if (i2cNanCount >= I2C_NAN_LIMIT) {
          localState.sensorError = true;
          Serial.printf("[WARN] Sensor I2C error %d× berturut-turut!\n", i2cNanCount);
        }
      } else {
        i2cNanCount            = 0;
        localState.sensorError = false;
      }
      
      localState.tempStatus = evaluateTemp(localReading.temperature);
      localState.humStatus  = evaluateHumidity(localReading.humidity);
      localState.presStatus = evaluatePressure(localReading.pressure);
      
      sync(localReading, localState);
      
      printTimestamp();
      Serial.printf("T:%.1fC | H:%.1f%% | P:%.1fhPa | Gas:%d(%s) | Flame:%d(%s) | Overall:%s | Muted:%s | MQTT:%s\n",
                    localReading.temperature, localReading.humidity, localReading.pressure,
                    localReading.gasADC,   statusToString(localState.gasStatus).c_str(),
                    localReading.flameADC, statusToString(localState.flameStatus).c_str(),
                    statusToString(localState.overallStatus).c_str(),
                    localState.buzzerMuted ? "YES" : "NO",
                    networkOnline ? "ONLINE" : "OFFLINE");
    }
  }

  void sync(const SensorReading& localReading, SystemState& localState) {
    if (stateMutex && xSemaphoreTake(stateMutex, pdMS_TO_TICKS(2)) == pdTRUE) {
      sharedReading.temperature = localReading.temperature;
      sharedReading.humidity    = localReading.humidity;
      sharedReading.pressure    = localReading.pressure;
      sharedReading.gasADC      = localReading.gasADC;
      sharedReading.flameADC    = localReading.flameADC;

      sharedState.tempStatus    = localState.tempStatus;
      sharedState.humStatus     = localState.humStatus;
      sharedState.presStatus    = localState.presStatus;
      sharedState.gasStatus     = localState.gasStatus;
      sharedState.flameStatus   = localState.flameStatus;
      sharedState.sensorError   = localState.sensorError;
      // buzzerMuted diset dari Core 0 (MQTT cmd), ambil dari sharedState
      localState.buzzerMuted    = sharedState.buzzerMuted;

      Status overall = NORMAL;
      overall = worstStatus(overall, localState.tempStatus);
      overall = worstStatus(overall, localState.humStatus);
      overall = worstStatus(overall, localState.presStatus);
      overall = worstStatus(overall, localState.gasStatus);
      overall = worstStatus(overall, localState.flameStatus);

      sharedState.overallStatus = overall;
      localState.overallStatus  = overall;

      xSemaphoreGive(stateMutex);
    }
  }
};

#endif
