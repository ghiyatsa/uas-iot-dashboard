/*
 * =====================================================================================
 *        Skema Proyek: IoT Room Safety & Environment Dashboard (ESP32 Firmware)
 * =====================================================================================
 * 
 * Deskripsi:
 *   Firmware utama berbasis ESP32 DevKitC V1 untuk mendeteksi ancaman lingkungan secara
 *   real-time (kebocoran gas, api/kebakaran) serta memonitor suhu, kelembapan, dan tekanan.
 *   Menggunakan arsitektur Dual-Core (FreeRTOS) untuk memisahkan logika keselamatan (Core 1)
 *   dan tugas jaringan/internet (Core 0).
 * 
 * Hubungan Antar File:
 *   - pins.h          : Deklarasi pin GPIO fisik dan alamat bus I2C.
 *   - config.h        : Berisi parameter timing, threshold sensor, dan konfigurasi NTP/OTA.
 *   - config_secret.h : Kredensial rahasia (WiFi, MQTT Broker, Token Telegram) [Jangan di-commit].
 *   - types.h         : Mendefinisikan model data global (SensorReading, SystemState).
 *   - Sensors.h       : Polling & kalibrasi data sensor fisik (AHT20, BMP280, MQ-2, Flame).
 *   - Actuators.h     : Mengontrol aktuator output fisik (LCD, LED RGB, Active Buzzer).
 *   - ota.ino         : Logika pembaruan firmware wireless menggunakan kelas ArduinoOTA.
 *   - telegram.ino    : Interaksi bot API Telegram untuk notifikasi & membalas pesan /status.
 *   - mqtt_pub.ino    : Pengemasan data ke format JSON dan publikasi ke HiveMQ Cloud Broker.
 * 
 * Detail Arsitektur Dual-Core (FreeRTOS):
 *   - [Core 1] Loop Utama (loop)  : Menangani polling sensor keselamatan & aktuasi non-blocking.
 *   - [Core 0] Network Task (Task): Menangani WiFi, MQTT, Telegram polling, & ArduinoOTA.
 *   - Sinkronisasi Data           : Dilakukan secara aman melalui Mutex (stateMutex).
 * 
 * Penggunaan TLS Socket Independen (Perbaikan Bug Terputus-putus):
 *   - secureClient   : Khusus untuk koneksi MQTT Broker (HiveMQ Cloud).
 *   - telegramClient : Khusus untuk request API HTTPS Telegram Bot.
 * =====================================================================================
 */

#include <Wire.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <soc/rtc_cntl_reg.h>   // Register kontrol RTC untuk menonaktifkan brownout detector
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <esp_task_wdt.h>        // WDT (Watchdog Timer) untuk memantau kelancaran thread
#include <time.h>
#include <ArduinoOTA.h>

#include "types.h"
#include "config.h"
#include "pins.h"
#include "Sensors.h"
#include "Actuators.h"

// ====================== DEKLARASI FUNGSI GLOBAL ======================
void connectWiFi(bool showLCD = false, bool showBlueLED = false);
void connectMQTT(bool showLCD = false, bool showBlueLED = false);
void networkTask(void* param);
void mqttCallback(char* topic, byte* payload, unsigned int length);
void setLEDColor(uint8_t r, uint8_t g, uint8_t b);
void otaSetup();
bool publishHeartbeatManual();

// ====================== OBJEK & KELAS GLOBAL ======================
WiFiClientSecure  secureClient;     // Socket SSL/TLS terdedikasi untuk MQTT
WiFiClientSecure  telegramClient;   // Socket SSL/TLS terdedikasi untuk Telegram Bot
PubSubClient      mqttClient(secureClient);
SensorsManager    sensors;          // Kelas OOP untuk manajemen input sensor
ActuatorsManager  actuators;        // Kelas OOP untuk manajemen output aktuator

// ====================== MUTEX & STATE TRANSAKSI DATA ======================
// stateMutex digunakan untuk mengunci variabel shared agar tidak dibaca & ditulis bersamaan
SemaphoreHandle_t stateMutex = NULL;
SystemState       sharedState;      // State sistem yang dibagi antara Core 1 & Core 0
SensorReading     sharedReading = {0.0f, 0.0f, 0.0f, 0, 4095}; // Data bacaan sensor global

// Variabel cache lokal untuk Core 1 (agar Core 1 tidak perlu sering mengunci mutex)
SensorReading     localReading = {0.0f, 0.0f, 0.0f, 0, 4095};
SystemState       localState;

// Flag penanda konektivitas jaringan online (WiFi terhubung & MQTT terhubung)
volatile bool networkOnline = false;
volatile bool networkBeeping = false; // Flag penanda sedang membunyikan beep koneksi (mencegah override dari Core 1)
volatile bool ledTesting = false;     // Flag penanda sedang menjalankan sequence test LED (mencegah override dari Core 1)

// ====================== SETUP SISTEM (RUN ON CORE 1) ======================
void setup() {
  // 1. Nonaktifkan Brownout Detector
  // Mencegah ESP32 restart otomatis akibat penurunan tegangan sesaat ketika WiFi memancarkan daya puncak
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);

  Serial.begin(115200);

  // Beep startup (Power ON) untuk konfirmasi hardware aktif saat booting
  pinMode(PIN_BUZZER, OUTPUT);
  digitalWrite(PIN_BUZZER, HIGH); delay(100); digitalWrite(PIN_BUZZER, LOW);

  // 2. Re-konfigurasi WDT (Watchdog Timer)
  // Dinaikkan ke 15 detik untuk mengakomodasi jeda waktu TLS handshake yang berat
  const esp_task_wdt_config_t wdt_cfg = { .timeout_ms = 15000, .idle_core_mask = 0, .trigger_panic = true };
  esp_task_wdt_reconfigure(&wdt_cfg);
  
  Serial.println("\n==================================================");
  Serial.println("  IoT Room Safety & Environment Dashboard");
  Serial.println("==================================================");
  Serial.printf("  - Sda:   GPIO %d | Scl:  GPIO %d\n", PIN_SDA, PIN_SCL);
  Serial.printf("  - MQ-2:  GPIO %d | Flame: GPIO %d\n", PIN_MQ2_AO, PIN_FLAME_AO);
  Serial.printf("  - Buzzer:GPIO %d\n", PIN_BUZZER);
  Serial.printf("  - Led RGB: R:%d, G:%d, B:%d\n", PIN_LED_R, PIN_LED_G, PIN_LED_B);
  Serial.println("==================================================");

  // 3. Inisialisasi Jalur I2C
  // Frekuensi diset 100kHz agar sinyal I2C tetap stabil walaupun kabel jumper cukup panjang
  Wire.begin(PIN_SDA, PIN_SCL);
  Wire.setClock(100000);
  Wire.setTimeOut(100);

  // 4. Inisialisasi Komponen Hardware via Kelas Manager
  actuators.begin();
  actuators.writeRaw("Room Safety Sys", "Booting...      ");
  sensors.begin();

  // 5. Inisialisasi Mutex FreeRTOS
  stateMutex = xSemaphoreCreateMutex();

  // 6. Konfigurasi Awal WiFi
  // Set ke STA (Station) mode dan membatasi TxPower ke 11dBm untuk menekan lonjakan arus peak
  WiFi.mode(WIFI_STA);
  WiFi.setTxPower(WIFI_POWER_11dBm);

  // 7. Koneksi Jaringan Awal (Koneksi Blocking saat Booting)
  networkBeeping = true; // Kunci kontrol buzzer di Core 1
  connectWiFi(true, true);
  if (WiFi.status() == WL_CONNECTED) {
    digitalWrite(PIN_BUZZER, HIGH); delay(150); digitalWrite(PIN_BUZZER, LOW); delay(100);
    digitalWrite(PIN_BUZZER, HIGH); delay(150); digitalWrite(PIN_BUZZER, LOW);
  }
  
  secureClient.setInsecure();    // Mengabaikan verifikasi sertifikat root CA untuk menghemat memori & mempercepat TLS
  telegramClient.setInsecure();  // Set insecure untuk telegramClient

  // 8. Sinkronisasi Waktu Riil (NTP Server)
  // Wajib dilakukan agar timestamp pengiriman data valid menggunakan WIB (UTC+7)
  configTime(GMT_OFFSET_SEC, DST_OFFSET_SEC, NTP_SERVER);
  if (WiFi.status() == WL_CONNECTED) {
    actuators.writeRaw("Room Safety Sys", "WiFi: OK | NTP..");
  }
  Serial.print("[NTP] Menunggu sinkronisasi waktu");
  struct tm ti;
  int ntpRetry = 0;
  while (!getLocalTime(&ti, 10) && ntpRetry < 10) {
    delay(500);
    Serial.print(".");
    ntpRetry++;
  }
  if (getLocalTime(&ti, 10)) {
    char timeBuf[32];
    strftime(timeBuf, sizeof(timeBuf), "%Y-%m-%d %H:%M:%S", &ti);
    Serial.printf("\n[NTP] Waktu berhasil disinkronkan: %s WIB\n", timeBuf);
    if (WiFi.status() == WL_CONNECTED) {
      actuators.writeRaw("Room Safety Sys", "NTP: OK         ");
      delay(500);
    }
  } else {
    Serial.println("\n[NTP] Gagal sinkronisasi waktu, NTP akan berjalan secara asinkron.");
    if (WiFi.status() == WL_CONNECTED) {
      actuators.writeRaw("Room Safety Sys", "NTP: SKIP (bg)  ");
      delay(500);
    }
  }

  // 9. Konfigurasi MQTT Client
  mqttClient.setServer(MQTT_HOST, MQTT_PORT);
  mqttClient.setBufferSize(512);     // Buffer dinaikkan agar payload JSON berukuran besar tidak terpotong
  mqttClient.setKeepAlive(60);       // Interval PINGREQ ke broker
  mqttClient.setCallback(mqttCallback);
  connectMQTT(true, true);
  if (mqttClient.connected()) {
    digitalWrite(PIN_BUZZER, HIGH); delay(200); digitalWrite(PIN_BUZZER, LOW);
  }
  networkBeeping = false; // Buka kunci kontrol buzzer untuk Core 1

  // 10. Konfigurasi OTA Update
  otaSetup();

  // 11. Membuat Background Task di Core 0
  // Memindahkan seluruh beban jaringan (WiFi, MQTT, Telegram, OTA) ke Core 0
  // agar Core 1 tetap berjalan lancar untuk logika keselamatan dan aktuator (anti-hang)
  xTaskCreatePinnedToCore(
    networkTask,      /* Fungsi yang dijalankan */
    "NetworkTask",    /* Nama Task */
    10240,            /* Alokasi Stack (10KB untuk kelancaran SSL/TLS & OTA) */
    NULL,             /* Parameter masukan */
    1,                /* Prioritas Task */
    NULL,             /* Task Handle */
    0                 /* Dipin di Core 0 */
  );

  Serial.printf("[INIT] Inisialisasi Selesai. Sisa memori heap: %u bytes\n", ESP.getFreeHeap());
  Serial.println("==========================================");
}

// ====================== LOOP UTAMA CORE 1 ======================
// Berjalan secara konstan di Core 1 khusus untuk memperbarui aktuasi berdasarkan sensor secara non-blocking.
void loop() {
  unsigned long now = millis();

  // Polling sensor dan pembaruan output fisik (LCD, LED, Buzzer) secara berkala
  sensors.process(now, localReading, localState);
  actuators.process(now, localReading, localState);

  // Delay minimal 1ms untuk mencegah watchdog trigger di Core 1
  delay(1);
}

// ====================== BACKGROUND NETWORK TASK (RUN ON CORE 0) ======================
// Mengelola konektivitas jaringan, publikasi MQTT, polling pesan Telegram, dan handling update OTA.
void networkTask(void* param) {
  // Variabel timing tracking untuk membatasi frekuensi eksekusi (cooldown & interval)
  unsigned long lastPublish       = 0;
  unsigned long lastHeartbeat     = 0;
  unsigned long lastTelegramPoll  = 0;
  unsigned long lastWifiAttempt   = 0;
  unsigned long lastMqttAttempt   = 0;
  unsigned long lastNotifTime     = 0;
  unsigned long lastAlertPublish  = 0;

  // Menyimpan status koneksi sebelumnya untuk mendeteksi perubahan transisi status
  bool lastWifiState = (WiFi.status() == WL_CONNECTED);
  bool lastMqttState = mqttClient.connected();
  Status lastPublishedStatus = NORMAL; // Tracker status publikasi terakhir untuk trigger instan

  for (;;) {
    // 1. Handling Update OTA
    // Mendeteksi dan memproses pembaruan firmware wireless yang masuk
    ArduinoOTA.handle();

    unsigned long now = millis();
    bool wifiOk = (WiFi.status() == WL_CONNECTED);
    bool mqttOk = mqttClient.connected();
    networkOnline = wifiOk && mqttOk;

    // 2. Feedback Suara (Beep) untuk Perubahan Konektivitas
    // WiFi Terputus -> 1x beep panjang 800ms
    if (lastWifiState && !wifiOk) {
      networkBeeping = true;
      digitalWrite(PIN_BUZZER, HIGH); delay(800); digitalWrite(PIN_BUZZER, LOW);
      networkBeeping = false;
    }
    // WiFi Terhubung -> 2x beep sedang konfirmasi
    else if (!lastWifiState && wifiOk) {
      networkBeeping = true;
      digitalWrite(PIN_BUZZER, HIGH); delay(150); digitalWrite(PIN_BUZZER, LOW); delay(100);
      digitalWrite(PIN_BUZZER, HIGH); delay(150); digitalWrite(PIN_BUZZER, LOW);
      networkBeeping = false;
    }
    lastWifiState = wifiOk;

    // MQTT Terputus -> 1x beep sedang 450ms (hanya berbunyi jika WiFi masih aktif)
    if (lastMqttState && !mqttOk && wifiOk) {
      networkBeeping = true;
      digitalWrite(PIN_BUZZER, HIGH); delay(450); digitalWrite(PIN_BUZZER, LOW);
      networkBeeping = false;
    }
    // MQTT Terhubung -> 1x beep singkat 200ms
    else if (!lastMqttState && mqttOk) {
      networkBeeping = true;
      digitalWrite(PIN_BUZZER, HIGH); delay(200); digitalWrite(PIN_BUZZER, LOW);
      networkBeeping = false;
    }
    lastMqttState = mqttOk;

    // 3. Logika Auto-reconnect Jaringan secara Asinkron
    if (!wifiOk) {
      if (now - lastWifiAttempt >= WIFI_RECONNECT_MS) {
        lastWifiAttempt = now;
        connectWiFi(false, false);
        configTime(GMT_OFFSET_SEC, DST_OFFSET_SEC, NTP_SERVER); // Re-sync NTP
      }
    } else if (!mqttOk) {
      if (now - lastMqttAttempt >= MQTT_RECONNECT_MS) {
        lastMqttAttempt = now;
        connectMQTT(false, false);
      }
    }

    // 4. MQTT Loop
    // Mengolah antrean pesan masuk dan menjaga koneksi PING ke broker tetap aktif
    if (mqttClient.connected()) {
      mqttClient.loop();
    }

    // 5. Sinkronisasi Mutex Data dari Core 1 ke Core 0
    SensorReading currentReading;
    SystemState currentState;
    
    if (stateMutex && xSemaphoreTake(stateMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
      currentReading = sharedReading;
      currentState = sharedState;
      xSemaphoreGive(stateMutex);
    } else {
      // Jika mutex terkunci, skip siklus ini dan coba lagi nanti
      vTaskDelay(pdMS_TO_TICKS(10));
      continue;
    }

    // 6. Publikasi Data Sensor & Alarm Bahaya via MQTT
    if (mqttClient.connected()) {
      bool triggerImmediate = (currentState.overallStatus == DANGER && lastPublishedStatus != DANGER);

      // Publikasi Telemetri berkala (tiap 500ms) atau instan saat masuk kondisi DANGER
      if (triggerImmediate || (now - lastPublish >= PUBLISH_INTERVAL_MS)) {
        lastPublish = now;
        publishDataManual(currentReading, currentState);
        
        // Publikasi data Alert DANGER (cooldown 30 detik agar tidak membebani broker)
        if (currentState.overallStatus == DANGER && (triggerImmediate || (now - lastAlertPublish >= MQTT_ALERT_COOLDOWN_MS))) {
          lastAlertPublish = now;
          publishAlertManual(currentReading, currentState.overallStatus);
        }
      }

      // Publikasi Heartbeat berkala (default tiap 15 detik) untuk monitoring status online device
      if (now - lastHeartbeat >= HEARTBEAT_INTERVAL_MS) {
        lastHeartbeat = now;
        publishHeartbeatManual();
      }
    }
    lastPublishedStatus = currentState.overallStatus;

    // 7. Notifikasi & Interaksi Telegram Bot
    if (wifiOk) {
      // Mengirimkan notifikasi bahaya (DANGER) ke Telegram (cooldown 60 detik agar tidak kena rate limit)
      if (currentState.overallStatus == DANGER && (now - lastNotifTime >= NOTIF_COOLDOWN_MS)) {
        lastNotifTime = now;
        sendTelegramAlert(currentReading, currentState);
      }

      // Polling dan proses pesan masuk Telegram (tiap 5 detik)
      if (now - lastTelegramPoll >= TELEGRAM_POLL_INTERVAL_MS) {
        lastTelegramPoll = now;
        handleTelegramBot(currentReading, currentState);
      }
    }

    // Delay 50ms di setiap siklus untuk memberikan prioritas ke idle task RTOS di Core 0
    vTaskDelay(pdMS_TO_TICKS(50));
  }
}

// ====================== PROSES KONEKSI WIFI ======================
void connectWiFi(bool showLCD, bool showBlueLED) {
  if (WiFi.status() == WL_CONNECTED) return;

  if (showBlueLED) {
    setLEDColor(0, 0, 255); // LED Biru menyala sebagai indikasi sedang mencari WiFi
  }

  if (showLCD) {
    actuators.writeRaw("Room Safety Sys", "WiFi: Connect...");
  }

  Serial.printf("[WIFI] Menghubungkan ke: %s ", WIFI_SSID);
  
  WiFi.setSleep(false);
  esp_wifi_set_ps(WIFI_PS_NONE); // Matikan mode hemat daya WiFi untuk meminimalkan ping latency
  
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  // Batas toleransi tunggu koneksi WiFi maksimal 10 detik
  unsigned long startAttempt = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startAttempt < 10000) {
    vTaskDelay(pdMS_TO_TICKS(300));
    Serial.print(".");
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println(" -> TERHUBUNG!");
    Serial.printf("[WIFI] IP Address: %s\n", WiFi.localIP().toString().c_str());
    if (showLCD) {
      actuators.writeRaw("Room Safety Sys", "WiFi: OK        ");
      delay(500);
    }
  } else {
    Serial.println(" -> GAGAL!");
    if (showLCD) {
      actuators.writeRaw("Room Safety Sys", "WiFi: FAIL!     ");
      delay(500);
    }
  }
}

// ====================== PROSES KONEKSI MQTT ======================
void connectMQTT(bool showLCD, bool showBlueLED) {
  if (WiFi.status() != WL_CONNECTED) return;
  if (mqttClient.connected()) return;

  if (showBlueLED) {
    setLEDColor(0, 0, 255); // LED Biru menyala saat menghubungkan ke Broker
  }

  if (showLCD) {
    actuators.writeRaw("Room Safety Sys", "NTP: OK | MQTT..");
  }

  secureClient.stop(); // Bersihkan sisa socket TLS lama sebelum memulai koneksi baru

  Serial.print("[MQTT] Menghubungkan ke broker HiveMQ... ");

  // Menggunakan 6 digit terakhir dari MAC Address sebagai clientId unik untuk broker
  String mac = WiFi.macAddress();
  mac.replace(":", "");
  String clientId = "esp32-safety-" + mac.substring(mac.length() - 6);

  // Menyiapkan payload Last Will (LWT) jika koneksi terputus tiba-tiba
  const char* willTopic = TOPIC_HEARTBEAT;
  uint8_t willQos = 1;
  bool willRetain = true;
  const char* willMessage = "{\"status\":\"offline\"}";

  if (mqttClient.connect(clientId.c_str(), MQTT_USERNAME, MQTT_PASSWORD, willTopic, willQos, willRetain, willMessage)) {
    Serial.println("TERHUBUNG!");
    publishHeartbeatManual(); // Kirim status online perdana ke broker dengan retain=true
    // Berlangganan (subscribe) ke sub-topik cmd untuk menerima perintah kontrol jarak jauh
    mqttClient.subscribe("iot/room-safety/cmd/#");
    if (showLCD) {
      actuators.writeRaw("Room Safety Sys", "MQTT: OK | READY");
      delay(1000);
    }
  } else {
    Serial.printf("GAGAL! (rc=%d)\n", mqttClient.state());
    if (showLCD) {
      actuators.writeRaw("Room Safety Sys", "MQTT: FAIL!     ");
      delay(500);
    }
  }
}

// ====================== CALLBACK PESAN MASUK MQTT ======================
// Memproses perintah remote dari Dashboard web yang dipublikasikan ke topik cmd.
void mqttCallback(char* topic, byte* payload, unsigned int length) {
  String topicStr(topic);
  String msg;
  msg.reserve(length);
  for (unsigned int i = 0; i < length; i++) msg += (char)payload[i];

  Serial.printf("[MQTT] Pesan Masuk [%s]: %s\n", topic, msg.c_str());

  // Command 1: Mute/Unmute buzzer alarm secara software
  if (topicStr == TOPIC_CMD_MUTE) {
    bool mute = (msg == "1" || msg == "true" || msg == "on");
    if (stateMutex && xSemaphoreTake(stateMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
      sharedState.buzzerMuted = mute;
      xSemaphoreGive(stateMutex);
    }
    Serial.printf("[CMD] Buzzer %s\n", mute ? "MUTED" : "UNMUTED");

  } 
  // Command 2: Sequence test hardware RGB LED (merah -> hijau -> biru)
  else if (topicStr == TOPIC_CMD_LED) {
    ledTesting = true;
    setLEDColor(255, 255, 255); delay(500);
    setLEDColor(255, 0, 0);     delay(500);
    setLEDColor(0, 255, 0);     delay(500);
    setLEDColor(0, 0, 255);     delay(500);
    ledTesting = false;
    Serial.println("[CMD] LED test selesai.");

  } 
  // Command 3: Mengirim notifikasi alert manual ke Telegram
  else if (topicStr == TOPIC_CMD_TG) {
    SensorReading r;
    SystemState s;
    if (stateMutex && xSemaphoreTake(stateMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
      r = sharedReading;
      s = sharedState;
      xSemaphoreGive(stateMutex);
    }
    sendTelegramAlert(r, s);
    Serial.println("[CMD] Telegram alert manual berhasil dikirim.");
  }
}

// ====================== UTILITY: CETAK TIMESTAMP REAL-TIME ======================
void printTimestamp() {
  struct tm ti;
  if (getLocalTime(&ti, 10)) {
    Serial.printf("[%02d:%02d:%02d] ", ti.tm_hour, ti.tm_min, ti.tm_sec);
  } else {
    // Fallback menggunakan millis jika koneksi NTP gagal
    unsigned long ms      = millis();
    unsigned long seconds = ms / 1000;
    unsigned long minutes = seconds / 60;
    unsigned long hours   = minutes / 60;
    Serial.printf("[%02lu:%02lu:%02lu] ", hours % 24, minutes % 60, seconds % 60);
  }
}

// ====================== UTILITY: PARSING STATUS ENUM ======================
String statusToString(Status s) {
  switch (s) {
    case NORMAL:  return "NORMAL";
    case WARNING: return "WARNING";
    case DANGER:  return "DANGER";
  }
  return "UNKNOWN";
}

// ====================== OUTPUT AKTUAL LED RGB (ANALOG WRITE) ======================
void setLEDColor(uint8_t r, uint8_t g, uint8_t b) {
  analogWrite(PIN_LED_R, r);
  analogWrite(PIN_LED_G, g);
  analogWrite(PIN_LED_B, b);
}
