# IoT Room Safety & Environment Dashboard

Sistem monitoring keamanan ruangan terintegrasi berbasis **ESP32** (Firmware) dan **Vite + React** (Web Dashboard) menggunakan protokol komunikasi **MQTT (HiveMQ Cloud)** dan notifikasi **Telegram Bot**.

---

## 📁 Struktur Proyek

```
UAS_IoT/
├── room_safety_dashboard/      # FIRMWARE ESP32
│   ├── room_safety_dashboard.ino # Sketch utama & FreeRTOS Dual-Core Loop
│   ├── Sensors.h               # SensorsManager (I2C Polling, Warmup, Health Check)
│   ├── Actuators.h             # ActuatorsManager (LCD, LED RGB, Buzzer alarm)
│   ├── mqtt_pub.ino            # Serialisasi JSON (ArduinoJson) & MQTT Publishers
│   ├── telegram.ino            # Polling & Telegram Alert Notification
│   ├── ota.ino                 # Update firmware Over-the-Air (ArduinoOTA)
│   ├── config.h                # Threshold sensor & Timing system
│   ├── config_secret.h         # [RAHASIA] Kredensial WiFi, MQTT, Telegram
│   ├── pins.h                  # Pinout mapping & konvensi warna kabel
│   └── types.h                 # Struktur data & Enum status
│
├── src/                        # FRONTEND WEB DASHBOARD (React + Vite)
│   ├── components/
│   │   ├── Header.jsx          # Header, status koneksi & badge sensor error
│   │   ├── SensorCard.jsx      # Card sensor modular dengan status color-coding
│   │   ├── HistoryChart.jsx    # Chart Recharts (Tabbed & Export CSV)
│   │   ├── ControlPanel.jsx    # Kontrol jarak jauh (Mute Buzzer, Test LED, TG)
│   │   └── Icons.jsx           # Clean SVG Icons pack (tanpa emoji unicode)
│   ├── hooks/
│   │   └── useMQTT.js          # Custom React Hook: MQTT.js Client (WSS) & Logic state
│   ├── App.jsx                 # Entry point frontend & layout grid
│   ├── App.css                 # Custom Styling System (Responsive & Glassmorphism)
│   └── index.css               # Design Tokens & global reset
│
├── public/                     # ASSET STATIC & PWA
│   ├── manifest.json           # Manifest PWA (App installable di Mobile/Desktop)
│   └── sw.js                   # Service Worker (Cache shell untuk offline support)
├── .env                        # [RAHASIA] Kredensial MQTT Broker untuk Frontend
├── index.html                  # HTML Shell & SW Registration
├── package.json                # Project dependencies & npm scripts
└── vite.config.js              # Konfigurasi bundler Vite
```

---

## 🏗️ Arsitektur Sistem & Multi-threading

Untuk menjamin fungsi keselamatan tidak terganggu oleh latensi jaringan internet/TLS, firmware ESP32 memanfaatkan arsitektur **Dual-Core FreeRTOS**:

1. **Core 1 (Safety Loop / Main Loop)**:
   - Polling sensor AHT20, BMP280, MQ-2, dan Flame secara berkala secara *non-blocking*.
   - Mengendalikan LCD, LED RGB, dan Buzzer dengan pola alarm tertentu sesuai tingkat bahaya.
   - Tetap berjalan normal walaupun koneksi internet terputus.
2. **Core 0 (Network Task)**:
   - Mengurusi koneksi WiFi, MQTT, polling Telegram Bot, dan update OTA secara asinkron.
   - Sinkronisasi data dengan Core 1 secara aman menggunakan **Mutex** (`SemaphoreHandle_t`).
   - Menggunakan **Dua Instance `WiFiClientSecure` Terpisah** (`secureClient` untuk MQTT & `telegramClient` untuk Telegram) guna mencegah tabrakan/pemutusan socket TLS.

---

## 🔧 Setup & Konfigurasi

### A. Setup Firmware (ESP32)

1. **Arduino IDE Board Manager**:
   - Tambahkan URL berikut di File -> Preferences -> Additional Boards Manager URLs:
     `https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json`
   - Buka Tools -> Board -> Boards Manager, cari `esp32` lalu install versi terbaru dari Espressif.
   - Pilih tipe board Anda (contoh: `ESP32 Dev Module` atau `DOIT ESP32 DEVKIT V1`).

2. **Arduino Libraries Required** (Install via Library Manager):
   - **ArduinoJson** (versi 6 atau 7)
   - **PubSubClient** (oleh Nick O'Leary)
   - **UniversalTelegramBot** (oleh Brian Lenehan)
   - **Adafruit AHTX0**
   - **Adafruit BMP280 Library**
   - **LiquidCrystal I2C** (oleh Frank de Brabander)

3. **Konfigurasi Kredensial**:
   - Salin file template:
     `cp room_safety_dashboard/config_secret.h.example room_safety_dashboard/config_secret.h`
   - Buka `room_safety_dashboard/config_secret.h` dan isi kredensial WiFi, broker HiveMQ, dan token Telegram Bot Anda.

4. **Upload Firmware**:
   - Sambungkan ESP32 via kabel USB, pilih port COM, lalu Upload.
   - Setelah sukses upload, buzzer akan mengeluarkan bunyi **beep ganda** sebagai tanda WiFi berhasil tersambung, disusul **beep singkat** setelah MQTT terhubung.

### 💡 Konfigurasi Nirkabel via Captive Portal (Alternatif)
Jika ESP32 gagal terhubung ke router WiFi yang ditentukan saat booting, perangkat otomatis berpindah ke mode **Access Point (AP)** mandiri untuk dikonfigurasi lewat browser HP/Laptop:
* **Nama Hotspot (SSID)**: `ESP32-Safety-Dashboard` (sesuai hostname)
* **Password Hotspot**: `config123`
* **Alamat IP Portal**: `192.168.4.1` (atau otomatis terbuka/redirect saat terhubung)

Di halaman portal ini, Anda dapat memindai jaringan WiFi terdekat, mengganti password WiFi, mengubah alamat broker MQTT, hingga memperbarui token Telegram secara wireless. Semua data disimpan secara permanen di memori flash NVS ESP32.

---

### B. Setup Web Dashboard (Frontend)

1. **Prasyarat**: Pastikan Anda sudah menginstall [Node.js](https://nodejs.org/).
2. **Install Dependencies**:
   ```bash
   npm install
   ```
3. **Konfigurasi Kredensial**:
   - Buka file `.env` di direktori utama, isi sesuai kredensial HiveMQ Cloud Anda (menggunakan port **8884** untuk WebSocket Secure/WSS):
     ```env
     VITE_MQTT_HOST=xxxxxx.s1.eu.hivemq.cloud
     VITE_MQTT_PORT=8884
     VITE_MQTT_USER=username_broker
     VITE_MQTT_PASS=password_broker
     ```
4. **Jalankan Development Server**:
   ```bash
   npm run dev
   ```
   Buka alamat URL lokal (biasanya `http://localhost:5173`) di browser Anda.
5. **Production Build**:
   ```bash
   npm run build
   ```

---

## 📡 Integrasi Topik MQTT

Aplikasi berkomunikasi secara real-time melalui topik-topik MQTT berikut:

### 1. Telemetri & Status (`iot/room-safety/data`)
Dikirim oleh ESP32 setiap 1 detik. Contoh payload JSON:
```json
{
  "t": 28.5,             // Suhu (°C)
  "h": 60.2,             // Kelembapan (%RH)
  "p": 1008.4,           // Tekanan (hPa)
  "g": 666,              // Gas ADC (MQ-2)
  "f": 4095,             // Flame ADC (Active-Low)
  "ts": "NORMAL",        // Status Suhu (NORMAL/WARNING/DANGER)
  "hs": "NORMAL",        // Status Kelembapan
  "ps": "NORMAL",        // Status Tekanan
  "gs": "NORMAL",        // Status Gas
  "fs": "NORMAL",        // Status Flame
  "os": "NORMAL",        // Status Overall
  "se": false,           // Sensor I2C Error Flag (true jika sensor rusak/terputus)
  "bm": false,           // Status Mute Buzzer
  "ms": 123456,          // Uptime ESP32 (millis)
  "ts_unix": 1782390481  // Unix Timestamp dari NTP
}
```

### 2. Notifikasi Bahaya (`iot/room-safety/alert`)
Dikirim instan oleh ESP32 saat pertama kali kondisi masuk ke `DANGER` (cooldown 30s):
```json
{
  "overall_status": "DANGER",
  "temperature": 28.5,
  "humidity": 60.2,
  "pressure": 1008.4,
  "gas_adc": 1850,
  "flame_adc": 4095,
  "message": "Kondisi DANGER terdeteksi, periksa ruangan segera!",
  "ts_unix": 1782390481
}
```

### 3. Keberadaan Device (`iot/room-safety/heartbeat`)
Dikirim oleh ESP32 setiap 15 detik untuk deteksi keaktifan (*Liveness/Keep-Alive*):
```json
{
  "status": "online",
  "uptime_ms": 123456,
  "heap_free": 184512,    // Ukuran sisa RAM ESP32 (heap memory)
  "ts_unix": 1782390481
}
```

### 4. Perintah Jarak Jauh (`iot/room-safety/cmd/#`)
Dikirim oleh Web Dashboard untuk mengontrol ESP32:
* `iot/room-safety/cmd/mute` : Mute alarm buzzer (`1` = Mute, `0` = Unmute).
* `iot/room-safety/cmd/test_led` : Memulai sequence test RGB LED (`1` = Trigger).
* `iot/room-safety/cmd/telegram` : Mengirim notifikasi status ruangan manual ke Telegram (`1` = Trigger).

---

## 💡 Fitur Unggulan & Karakteristik Suara Alarm

* **Suara Alarm Cerdas**:
  * **WiFi Connect**: Beep ganda sedang (2x 150ms).
  * **WiFi Disconnect**: Beep tunggal panjang (1x 800ms).
  * **MQTT Connect**: Beep tunggal pendek (1x 200ms).
  * **MQTT Disconnect**: Beep tunggal sedang (1x 450ms).
  * **Bahaya Kebocoran Gas saja**: Beep berulang lambat (tiap 800ms).
  * **Bahaya Kebakaran saja**: Beep berulang cepat (tiap 200ms).
  * **Bahaya Ganda (Gas & Kebakaran)**: Buzzer berbunyi terus-menerus tanpa jeda (*solid tone*).
* **Sensor Health Check**: Jika terjadi pembacaan `NaN` sebanyak 5x berturut-turut pada bus I2C, sistem menyalakan badge warning `SENSOR ERR` di dashboard.
* **PWA & Push Notification**: Dashboard web dapat diinstall langsung di ponsel/laptop sebagai aplikasi native, lengkap dengan support notifikasi desktop/mobile saat kondisi DANGER terdeteksi.
