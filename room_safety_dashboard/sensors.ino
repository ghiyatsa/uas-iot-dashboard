/*
 * sensors.ino - Non-blocking sensor reading and threshold evaluation.
 * Utilizes a time-sliced state machine for analog multi-sampling (5ms intervals),
 * avoiding delay() calls to maintain Core 1 responsiveness.
 */

#include <Adafruit_AHTX0.h>
#include <Adafruit_BMP280.h>

Adafruit_AHTX0    aht;
Adafruit_BMP280   bmp;
extern SemaphoreHandle_t stateMutex;
extern SensorReading     sharedReading;
extern SystemState       sharedState;
extern SensorReading     localReading;
extern SystemState       localState;
extern bool              mq2Ready;
extern volatile bool     networkOnline;

// Timer and state variable for analog non-blocking sampling
static unsigned long lastAnalogSampleTime = 0;
static unsigned long lastI2cSampleTime = 0;
static int           analogSampleCount = 0;
static long          gasSampleSum = 0;
static long          flameSampleSum = 0;

static unsigned long lastGasDangerTime = 0;
static unsigned long lastFlameDangerTime = 0;

// Inline Danger Latching function
inline Status latchDanger(Status eval, unsigned long &lastTime, unsigned long now, unsigned long latchMs = 10000UL) {
  if (eval == DANGER) {
    lastTime = now;
    return DANGER;
  }
  if (lastTime > 0 && (now - lastTime < latchMs)) return DANGER;
  return eval;
}

// Forward Declarations
Status evaluateTemp(float t);
Status evaluateHumidity(float h);
Status evaluatePressure(float p);
Status evaluateGas(int adc);
Status evaluateFlame(int adc);
Status worstStatus(Status a, Status b);

// ====================== INIT SENSORS ======================
void initSensors() {
  // AHT20
  if (!aht.begin()) {
    Serial.println("[INIT] ERROR: AHT20 tidak terdeteksi!");
  } else {
    Serial.println("[INIT] Sensor AHT20: OK");
  }

  // BMP280
  if (!bmp.begin(I2C_ADDR_BMP280)) {
    Serial.println("[INIT] ERROR: BMP280 tidak terdeteksi!");
  } else {
    Serial.println("[INIT] Sensor BMP280: OK");
    // Konfigurasi sampling cepat dengan filter noise internal hardware
    bmp.setSampling(Adafruit_BMP280::MODE_NORMAL,
                    Adafruit_BMP280::SAMPLING_X2,
                    Adafruit_BMP280::SAMPLING_X16,
                    Adafruit_BMP280::FILTER_X16,
                    Adafruit_BMP280::STANDBY_MS_500);
  }
}

// ====================== PROCESS SENSORS (NON-BLOCKING) ======================
void processSensorsNonBlocking(unsigned long now) {
  
  // ── 1. SAMPLING ANALOG (MQ-2 & FLAME) SETIAP 5ms ──────────────────────────
  if (now - lastAnalogSampleTime >= 5UL) {
    lastAnalogSampleTime = now;
    
    gasSampleSum   += analogRead(PIN_MQ2_AO);
    flameSampleSum += analogRead(PIN_FLAME_AO);
    analogSampleCount++;
    
    // Setelah terkumpul ADC_SAMPLE_COUNT (10) data
    if (analogSampleCount >= ADC_SAMPLE_COUNT) {
      localReading.gasADC   = gasSampleSum / ADC_SAMPLE_COUNT;
      localReading.flameADC = flameSampleSum / ADC_SAMPLE_COUNT;
      
      // Reset accumulator
      gasSampleSum = 0;
      flameSampleSum = 0;
      analogSampleCount = 0;
      
      // Evaluasi status gas & api dengan latching
      localState.gasStatus   = latchDanger(mq2Ready ? evaluateGas(localReading.gasADC) : NORMAL, lastGasDangerTime, now);
      localState.flameStatus = latchDanger(evaluateFlame(localReading.flameADC), lastFlameDangerTime, now);
      
      // Timeout 2ms: berikan waktu singkat jika Core 0 sedang lock mutex
      if (stateMutex && xSemaphoreTake(stateMutex, pdMS_TO_TICKS(2)) == pdTRUE) {
        sharedReading.gasADC   = localReading.gasADC;
        sharedReading.flameADC = localReading.flameADC;
        sharedState.gasStatus   = localState.gasStatus;
        sharedState.flameStatus = localState.flameStatus;
        
        // Hitung overall status berdasarkan data terbaru
        Status overall = NORMAL;
        overall = worstStatus(overall, localState.tempStatus);
        overall = worstStatus(overall, localState.humStatus);
        overall = worstStatus(overall, localState.presStatus);
        overall = worstStatus(overall, localState.gasStatus);
        overall = worstStatus(overall, localState.flameStatus);
        sharedState.overallStatus = overall;
        localState.overallStatus = overall;
        
        xSemaphoreGive(stateMutex);
      }
    }
  }

  // ── 2. SAMPLING I2C (AHT20 & BMP280) SETIAP 1000ms ───────────────────────
  if (now - lastI2cSampleTime >= 1000UL) {
    lastI2cSampleTime = now;
    
    // AHT20: suhu & kelembapan
    sensors_event_t humEvent, tempEvent;
    aht.getEvent(&humEvent, &tempEvent);
    localReading.temperature = tempEvent.temperature;
    localReading.humidity    = humEvent.relative_humidity;
    
    // BMP280: tekanan (hPa)
    localReading.pressure = bmp.readPressure() / 100.0F;
    
    // Evaluasi status
    localState.tempStatus = evaluateTemp(localReading.temperature);
    localState.humStatus  = evaluateHumidity(localReading.humidity);
    localState.presStatus = evaluatePressure(localReading.pressure);
    
    // Update Shared State ke Core 0
    if (stateMutex && xSemaphoreTake(stateMutex, pdMS_TO_TICKS(2)) == pdTRUE) {
      sharedReading.temperature = localReading.temperature;
      sharedReading.humidity    = localReading.humidity;
      sharedReading.pressure    = localReading.pressure;
      sharedState.tempStatus    = localState.tempStatus;
      sharedState.humStatus     = localState.humStatus;
      sharedState.presStatus    = localState.presStatus;
      
      // Hitung overall status
      Status overall = NORMAL;
      overall = worstStatus(overall, localState.tempStatus);
      overall = worstStatus(overall, localState.humStatus);
      overall = worstStatus(overall, localState.presStatus);
      overall = worstStatus(overall, localState.gasStatus);
      overall = worstStatus(overall, localState.flameStatus);
      sharedState.overallStatus = overall;
      localState.overallStatus = overall;
      
      xSemaphoreGive(stateMutex);
    }
    
    // Log diagnostik lokal satu baris ke Serial
    printTimestamp();
    Serial.printf("T:%.1fC | H:%.1f%% | P:%.1fhPa | Gas:%d(%s) | Flame:%d(%s) | Overall:%s | MQTT:%s\n",
                  localReading.temperature, localReading.humidity, localReading.pressure,
                  localReading.gasADC,   statusToString(localState.gasStatus).c_str(),
                  localReading.flameADC, statusToString(localState.flameStatus).c_str(),
                  statusToString(localState.overallStatus).c_str(),
                  networkOnline ? "ONLINE" : "OFFLINE");
  }
}

// ====================== EVALUASI THRESHOLD ======================
Status evaluateTemp(float t) {
  if (t < TEMP_DANGER_MIN)                               return DANGER;  // di bawah 0°C = beku
  if (t >= TEMP_NORMAL_MIN && t <= TEMP_NORMAL_MAX)      return NORMAL;
  if (t >= TEMP_WARNING_MIN && t <= TEMP_WARNING_MAX)    return WARNING;
  return WARNING; // di luar zona warning (terlalu panas) tetap WARNING
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
  static int dangerConfirmCounter = 0;
  
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
