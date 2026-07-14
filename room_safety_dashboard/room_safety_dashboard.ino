/*
 * room_safety_dashboard.ino - Main entry point for the ESP32 safety dashboard firmware.
 * Fully optimized for performance: multi-core architecture (FreeRTOS), 
 * non-blocking sensor sampling, manual JSON string serialization.
 */

#include <Wire.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <soc/rtc_cntl_reg.h>  // untuk disable brownout detector
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <esp_task_wdt.h>       // untuk feed WDT saat koneksi blocking

#include "types.h"
#include "config.h"
#include "pins.h"

// ====================== FORWARD DECLARATIONS ======================
void connectWiFi(bool showLCD = false, bool showBlueLED = false);
void connectMQTT(bool showLCD = false, bool showBlueLED = false);
void processSensorsNonBlocking(unsigned long now);
void updateActuatorsNonBlocking(unsigned long now);
void networkTask(void* param);
void mqttCallback(char* topic, byte* payload, unsigned int length);

// ====================== OBJEK GLOBAL ======================
WiFiClientSecure  secureClient;
PubSubClient      mqttClient(secureClient);

// ====================== STATE GLOBAL (SHARED) ======================
// State yang diakses oleh kedua core disinkronisasikan menggunakan mutex
SemaphoreHandle_t stateMutex = NULL;
SystemState       sharedState;
SensorReading     sharedReading = {0.0f, 0.0f, 0.0f, 0, 4095};

// Cache bacaan sensor lokal di Core 1
SensorReading     localReading = {0.0f, 0.0f, 0.0f, 0, 4095};
SystemState       localState;

// Flag warm-up MQ-2 & timing
bool mq2Ready = false;
unsigned long bootTime = 0;

// Connection state tracking
volatile bool networkOnline = false;

// ====================== SETUP ======================
void setup() {
  // Matikan brownout detector — mencegah reset saat WiFi radio menarik arus peak
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);

  Serial.begin(115200);
  Serial.println("\n==================================================");
  Serial.println("  IoT Room Safety & Environment Dashboard (OPTIMIZED)");
  Serial.println("==================================================");
  Serial.printf("  - Sda:   GPIO %d | Scl:  GPIO %d\n", PIN_SDA, PIN_SCL);
  Serial.printf("  - MQ-2:  GPIO %d | Flame: GPIO %d\n", PIN_MQ2_AO, PIN_FLAME_AO);
  Serial.printf("  - Buzzer:GPIO %d\n", PIN_BUZZER);
  Serial.printf("  - Led RGB: R:%d, G:%d, B:%d\n", PIN_LED_R, PIN_LED_G, PIN_LED_B);
  Serial.println("==================================================");

  // Inisialisasi Bus I2C pada clock 100kHz (stabil)
  Wire.begin(PIN_SDA, PIN_SCL);
  Wire.setClock(100000);
  Wire.setTimeOut(100);

  // Inisialisasi display dan sensor (di display.ino & sensors.ino)
  initDisplay();
  initSensors();

  // Setup pin mode untuk semua aktuator sebelum digunakan
  pinMode(PIN_BUZZER, OUTPUT);
  pinMode(PIN_LED_R, OUTPUT);
  pinMode(PIN_LED_G, OUTPUT);
  pinMode(PIN_LED_B, OUTPUT);
  digitalWrite(PIN_BUZZER, LOW);
  
  // Status INIT = biru
  analogWrite(PIN_LED_R, 0);
  analogWrite(PIN_LED_G, 0);
  analogWrite(PIN_LED_B, 255);

  // Mutex untuk thread safety
  stateMutex = xSemaphoreCreateMutex();

  // Set mode WiFi & TxPower sebelum koneksi agar hemat arus peak
  WiFi.mode(WIFI_STA);
  WiFi.setTxPower(WIFI_POWER_11dBm);

  // Koneksi awal WiFi & MQTT secara blocking saat boot agar siap pakai
  connectWiFi(true, true);
  secureClient.setInsecure(); // Menghilangkan overhead parsing cert CA untuk mempercepat TLS
  mqttClient.setServer(MQTT_HOST, MQTT_PORT);
  
  // Optimasi buffer client MQTT
  mqttClient.setBufferSize(512);
  mqttClient.setKeepAlive(60);
  mqttClient.setCallback(mqttCallback);
  connectMQTT(true, true);

  bootTime = millis();

  // Buat Network Task di Core 0 (Isolated dari loop utama Core 1)
  // Stack size 8KB cukup untuk TLS handshake & Telegram API
  xTaskCreatePinnedToCore(
    networkTask,      /* Fungsi Task */
    "NetworkTask",    /* Nama Task */
    8192,             /* Stack size */
    NULL,             /* Parameter */
    1,                /* Prioritas rendah agar tidak starving CPU */
    NULL,             /* Task handle */
    0                 /* Core 0 */
  );

  Serial.printf("[INIT] Setup Selesai. Free heap: %u bytes\n", ESP.getFreeHeap());
  Serial.println("==========================================");
}

// ====================== LOOP UTAMA (CORE 1) ======================
// Berjalan khusus untuk Safety & Actuation: membaca sensor dan memperbarui alarm fisik.
// Loop ini tidak akan terblokir oleh urusan jaringan/TLS.
void loop() {
  unsigned long now = millis();

  // Cek warm-up MQ-2
  if (!mq2Ready && (now - bootTime >= MQ2_WARMUP_MS)) {
    mq2Ready = true;
    Serial.println("[INIT] MQ-2 siap digunakan.");
  }

  // 1. Baca sensor secara non-blocking
  processSensorsNonBlocking(now);

  // 2. Perbarui aktuator lokal (LED RGB, Buzzer, LCD 16x2) secara non-blocking
  updateActuatorsNonBlocking(now);

  // 3. Batasi frekuensi loop untuk menghemat daya & siklus CPU
  delay(1);
}

// ====================== TASK JARINGAN (CORE 0) ======================
// Mengurusi semua transaksi internet (WiFi, MQTT, Telegram).
void networkTask(void* param) {
  unsigned long lastPublish       = 0;
  unsigned long lastHeartbeat     = 0;
  unsigned long lastTelegramPoll  = 0;
  unsigned long lastWifiAttempt   = 0;
  unsigned long lastMqttAttempt   = 0;
  unsigned long lastNotifTime     = 0;
  unsigned long lastAlertPublish  = 0; // Cooldown publishAlert agar tidak flood setiap detik

  for (;;) {
    unsigned long now = millis();
    bool wifiOk = (WiFi.status() == WL_CONNECTED);
    bool mqttOk = mqttClient.connected();
    networkOnline = wifiOk && mqttOk;

    // ── KONEKSI WiFi & MQTT ──────────────────────────────────────────────────
    if (!wifiOk) {
      if (now - lastWifiAttempt >= WIFI_RECONNECT_MS) {
        lastWifiAttempt = now;
        connectWiFi(false, false);
      }
    } else if (!mqttOk) {
      if (now - lastMqttAttempt >= MQTT_RECONNECT_MS) {
        lastMqttAttempt = now;
        connectMQTT(false, false);
      }
    }

    // ── MQTT LOOP ───────────────────────────────────────────────────────────
    if (mqttClient.connected()) {
      mqttClient.loop();
    }

    // Copy data sensor dari Core 1 secara aman menggunakan mutex
    SensorReading currentReading;
    SystemState currentState;
    
    if (stateMutex && xSemaphoreTake(stateMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
      currentReading = sharedReading;
      currentState = sharedState;
      xSemaphoreGive(stateMutex);
    } else {
      // Jika mutex sibuk, tunda pengerjaan network di siklus ini
      vTaskDelay(pdMS_TO_TICKS(10));
      continue;
    }

    // ── PERIODIC PUBLISH (MQTT) ─────────────────────────────────────────────
    if (mqttClient.connected()) {
      // Telemetri berkala (setiap 1 detik)
      if (now - lastPublish >= PUBLISH_INTERVAL_MS) {
        lastPublish = now;
        publishDataManual(currentReading, currentState);
        
        // Kirim alert MQTT saat DANGER, dengan cooldown 30 detik agar tidak flood broker
        if (currentState.overallStatus == DANGER && (now - lastAlertPublish >= 30000UL)) {
          lastAlertPublish = now;
          publishAlertManual(currentReading, currentState.overallStatus);
        }
      }

      // Heartbeat berkala (setiap 15 detik)
      if (now - lastHeartbeat >= HEARTBEAT_INTERVAL_MS) {
        lastHeartbeat = now;
        publishHeartbeatManual();
      }
    }

    // ── TELEGRAM BOT API ────────────────────────────────────────────────────
    if (wifiOk) {
      // 1. Kirim notifikasi bahaya (DANGER) asinkron dengan cooldown 60s
      if (currentState.overallStatus == DANGER && (now - lastNotifTime >= NOTIF_COOLDOWN_MS)) {
        lastNotifTime = now;
        sendTelegramAlert(currentReading, currentState);
      }

      // 2. Polling pesan masuk Telegram (setiap 5 detik)
      if (now - lastTelegramPoll >= 5000UL) {
        lastTelegramPoll = now;
        handleTelegramBot(currentReading, currentState);
      }
    }

    // Tunda task jaringan untuk memberi kesempatan idle task di Core 0
    vTaskDelay(pdMS_TO_TICKS(50));
  }
}

// ====================== HANDLERS & HELPERS ==========================
void connectWiFi(bool showLCD, bool showBlueLED) {
  if (WiFi.status() == WL_CONNECTED) return;

  if (showBlueLED) {
    analogWrite(PIN_LED_R, 0);
    analogWrite(PIN_LED_G, 0);
    analogWrite(PIN_LED_B, 255);
  }

  if (showLCD) {
    lcdWriteRaw("Connecting WiFi", "Please Wait     ");
  }

  Serial.printf("[WIFI] Menghubungkan ke: %s ", WIFI_SSID);
  
  // Nonaktifkan WiFi sleep mode untuk transmisi berkecepatan tinggi & latensi rendah
  WiFi.setSleep(false);
  esp_wifi_set_ps(WIFI_PS_NONE);
  
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  unsigned long startAttempt = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startAttempt < 10000) {
    vTaskDelay(pdMS_TO_TICKS(300)); // beri kesempatan IDLE0
    esp_task_wdt_reset();           // feed WDT selama polling
    Serial.print(".");
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println(" -> TERHUBUNG!");
    Serial.printf("[WIFI] IP: %s\n", WiFi.localIP().toString().c_str());
    if (showLCD) {
      lcdWriteRaw("WiFi Connected!", WiFi.localIP().toString().c_str());
      delay(1000);
    }
  } else {
    Serial.println(" -> GAGAL!");
    if (showLCD) {
      lcdWriteRaw("WiFi Conn FAIL!", "Retrying in bg  ");
      delay(1000);
    }
  }
}

void connectMQTT(bool showLCD, bool showBlueLED) {
  if (WiFi.status() != WL_CONNECTED) return;
  if (mqttClient.connected()) return;

  if (showBlueLED) {
    analogWrite(PIN_LED_R, 0);
    analogWrite(PIN_LED_G, 0);
    analogWrite(PIN_LED_B, 255);
  }

  if (showLCD) {
    lcdWriteRaw("Connecting MQTT", "Please Wait     ");
  }

  secureClient.stop(); // Bersihkan sisa TLS socket lama

  Serial.print("[MQTT] Menghubungkan ke broker HiveMQ... ");

  // Buat Client ID unik berdasarkan MAC address
  String mac = WiFi.macAddress();
  mac.replace(":", "");
  String clientId = "esp32-safety-" + mac.substring(mac.length() - 6);

  // Feed WDT sebelum TLS handshake (bisa makan >3 detik → starve IDLE0)
  esp_task_wdt_reset();

  if (mqttClient.connect(clientId.c_str(), MQTT_USERNAME, MQTT_PASSWORD)) {
    Serial.println("TERHUBUNG!");
    mqttClient.subscribe("iot/room-safety/cmd/#");
    if (showLCD) {
      lcdWriteRaw("MQTT Connected!", "Ready           ");
      delay(1000);
    }
  } else {
    Serial.printf("GAGAL! (rc=%d)\n", mqttClient.state());
    if (showLCD) {
      lcdWriteRaw("MQTT Conn FAIL!", "Retrying in bg  ");
      delay(1000);
    }
  }

  esp_task_wdt_reset(); // Feed lagi setelah selesai
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  // Hanya log pesan masuk (tidak parsing JSON untuk optimasi siklus CPU)
  Serial.printf("[MQTT] Pesan Masuk [%s]: ", topic);
  for (unsigned int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();
}

void printTimestamp() {
  unsigned long ms      = millis();
  unsigned long seconds = ms / 1000;
  unsigned long minutes = seconds / 60;
  unsigned long hours   = minutes / 60;
  Serial.printf("[%02lu:%02lu:%02lu] ", hours % 24, minutes % 60, seconds % 60);
}

String statusToString(Status s) {
  switch (s) {
    case NORMAL:  return "NORMAL";
    case WARNING: return "WARNING";
    case DANGER:  return "DANGER";
  }
  return "UNKNOWN";
}
