# IoT Room Safety & Environment Dashboard (ESP32)

Firmware berbasis ESP32 untuk mendeteksi bahaya lingkungan (kebocoran gas, kebakaran) serta memantau kondisi ruangan (suhu, kelembapan, tekanan udara) secara real-time. Firmware ini dioptimalkan menggunakan Pemrograman Berorientasi Objek (OOP) C++ dan arsitektur multi-core (FreeRTOS) untuk keandalan maksimal.

---

## 🏗️ Arsitektur Sistem

Untuk memastikan keamanan ruangan tidak terganggu oleh latensi jaringan internet, firmware ini memanfaatkan arsitektur dual-core ESP32 dengan **FreeRTOS**:

```
                  ┌──────────────────────────────────────────┐
                  │               Dual-Core ESP32            │
                  └────────────────────┬─────────────────────┘
                                       │
            ┌──────────────────────────┴──────────────────────────┐
            ▼ (Core 1)                                            ▼ (Core 0)
   ┌─────────────────┐                                   ┌─────────────────┐
   │   Safety Loop   │                                   │  Network Task   │
   ├─────────────────┤                                   ├─────────────────┤
   │ ─ SensorsManager│ <──────── [Shared State] ────────> │ ─ WiFi Keep-Alv │
   │   (AHT20, BMP280│           (Mutex-Protected)       │ ─ MQTT Pub      │
   │    MQ2, Flame)  │                                   │   (ArduinoJson) │
   │ ─ ActuatorsMngr │                                   │ ─ Telegram Bot  │
   │   (LCD, LED, Buz)                                   │   (Bot Library) │
   └─────────────────┘                                   └─────────────────┘
```

1. **Core 1 (Loop Utama)**: Mengambil data dari kelas `SensorsManager` dan memperbarui aktuator menggunakan kelas `ActuatorsManager`. Proses berjalan tanpa *delay* yang memblokir.
2. **Core 0 (Task Jaringan)**: Berjalan secara asinkron untuk mengurus koneksi TLS, pengiriman data MQTT dengan library `ArduinoJson`, dan pembacaan pesan Telegram menggunakan library `UniversalTelegramBot`.

---

## 🔌 Skema Pinout

| Komponen | Pin ESP32 | Warna Kabel (Rekomendasi) | Keterangan |
|---|---|---|---|
| **VCC / V5** | 5V / 3.3V | Merah | Power Line |
| **GND** | GND | Hitam | Ground |
| **I2C SDA** | GPIO 21 | Hijau | Komunikasi LCD, AHT20, BMP280 |
| **I2C SCL** | GPIO 22 | Kuning | Komunikasi LCD, AHT20, BMP280 |
| **MQ-2 AO** | GPIO 32 | Ungu | Input Analog Sensor Gas |
| **Flame AO**| GPIO 33 | Putih | Input Analog Sensor Api (Active-Low) |
| **Buzzer**  | GPIO 23 | Cokelat | Output Aktuator Alarm Suara |
| **RGB LED R**| GPIO 25 | Oranye | Red PWM Pin |
| **RGB LED G**| GPIO 26 | Abu-abu | Green PWM Pin |
| **RGB LED B**| GPIO 27 | Biru | Blue PWM Pin |

---

## 📦 Library Dependencies

Instal library berikut sebelum melakukan kompilasi melalui Library Manager Arduino IDE atau PlatformIO:
- **ArduinoJson** (versi 6 atau 7)
- **UniversalTelegramBot** (oleh Brian Lenehan, versi 1.3.0 atau lebih baru)
- **Adafruit AHTX0**
- **Adafruit BMP280 Library**
- **LiquidCrystal I2C**
- **PubSubClient** (oleh Nick O'Leary)

---

## ⚙️ Cara Konfigurasi Kredensial

1. Salin template kredensial rahasia:
   ```bash
   cp room_safety_dashboard/config_secret.h.example room_safety_dashboard/config_secret.h
   ```
2. Buka `room_safety_dashboard/config_secret.h` dan isi kredensial Anda:
   - SSID & Password WiFi
   - Host, Username, dan Password MQTT HiveMQ Cloud
   - Token Bot Telegram & Chat ID tujuan notifikasi

---

## 🤖 Perintah Telegram Bot

* `/status` - Menampilkan kondisi pembacaan sensor dan status ruangan secara real-time.
* `/help` / `/start` - Menampilkan panduan bantuan bot.

---

## 📡 Integrasi MQTT

Data telemetri dikirim secara efisien menggunakan serialisasi JSON ke topik berikut:
* `iot/room-safety/data`: Telemetri berkala (default tiap 1 detik).
* `iot/room-safety/alert`: Notifikasi kondisi bahaya (DANGER).
* `iot/room-safety/heartbeat`: Sinyal tanda perangkat aktif (tiap 15 detik).
