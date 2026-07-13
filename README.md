# IoT Room Safety & Environment Dashboard — Kode Lengkap

Sesuai PRD v1.1. Berisi dua bagian: firmware ESP32 dan dashboard web.

## 📁 Struktur File

```
firmware/
  config.h                  → kredensial WiFi, MQTT, Telegram, threshold sensor
  pins.h                     → pin mapping (sesuai PRD §6.2-6.3)
  room_safety_dashboard.ino  → firmware utama
  i2c_scanner.ino            → sketch bantu untuk cek alamat I2C (jalankan dulu!)

dashboard/
  index.html                 → dashboard web (siap deploy ke PaaS apa saja)
```

## 🔧 Setup Firmware

### 1. Install library (Arduino IDE → Library Manager)
- Adafruit AHTX0
- Adafruit BMP280 Library
- Adafruit Unified Sensor
- LiquidCrystal_I2C (Frank de Brabander)
- PubSubClient (Nick O'Leary)
- ArduinoJson (Benoit Blanchon)

### 2. Install driver CH340
https://www.wch-ic.com/downloads/CH341SER_EXE.html

### 3. Cek alamat I2C dulu
Upload `i2c_scanner.ino`, buka Serial Monitor (115200 baud). Pastikan:
- `0x38` terdeteksi (AHT20)
- `0x76` terdeteksi (BMP280)
- `0x27` atau `0x3F` terdeteksi (LCD)

Jika LCD terdeteksi di `0x3F`, ubah `I2C_ADDR_LCD` di `pins.h`.

### 4. Isi kredensial di `config.h`
- `WIFI_SSID` / `WIFI_PASSWORD`
- `MQTT_HOST` → ambil dari HiveMQ Cloud Console (format: `xxxxx.s1.eu.hivemq.cloud`)
- `MQTT_USERNAME` / `MQTT_PASSWORD` → buat di HiveMQ Console → Access Management
- `TELEGRAM_BOT_TOKEN` → buat bot baru lewat **@BotFather** di Telegram, copy token-nya
- `TELEGRAM_CHAT_ID` → kirim pesan apa saja ke bot, lalu cek
  `https://api.telegram.org/bot<TOKEN>/getUpdates` untuk melihat `chat.id`

### 5. Wiring
Ikuti tabel pin mapping & konvensi warna kabel di PRD §6. Pastikan:
- AHT20+BMP280 pakai **3.3V**, jangan 5V
- MQ-2, LCD, Buzzer pakai **VIN (5V)**
- Tambahkan pull-up 4.7kΩ ke 3.3V di jalur SDA/SCL jika modul belum punya pull-up internal

### 6. Upload firmware utama
Upload `room_safety_dashboard.ino`. Tunggu 30 detik warm-up MQ-2 sebelum hasil gas dianggap valid.

## 🌐 Setup Dashboard

### 1. Cari port WebSocket TLS di HiveMQ Cloud
Masuk ke HiveMQ Cloud Console → cluster Anda → **Connection details**. Browser butuh port **WSS** (bukan port 8883 yang dipakai ESP32) — biasanya port **8884**, path `/mqtt`. Sesuaikan di `MQTT_CONFIG` pada `index.html`.

### 2. Edit kredensial
Buka `dashboard/index.html`, cari blok `MQTT_CONFIG` di bagian `<script>`, isi:
```js
const MQTT_CONFIG = {
  host: 'xxxxxxxx.s1.eu.hivemq.cloud',
  port: 8884,
  path: '/mqtt',
  username: 'dashboard_user',
  password: 'dashboard_password',
  ...
};
```

### 3. Deploy ke PaaS
File `index.html` ini adalah static single-file app — bisa langsung di-deploy ke:
- **Vercel** / **Netlify** (drag & drop folder `dashboard/`)
- **GitHub Pages**
- **Render** (static site)

Tidak perlu backend server karena koneksi MQTT dilakukan langsung dari browser ke HiveMQ Cloud via WebSocket.

⚠️ **Catatan keamanan**: karena kredensial MQTT ada di kode frontend (client-side), siapa pun yang membuka dashboard bisa melihatnya di DevTools. Untuk skala kelas/prototipe ini biasanya cukup, tapi untuk produksi sebaiknya:
- Buat MQTT user **read-only** (hanya subscribe, tidak publish) khusus untuk dashboard
- Atau buat backend proxy yang menyimpan kredensial di server

## 📡 Format Data MQTT

**Topic `iot/room-safety/data`** (setiap 5 detik):
```json
{
  "temperature": 28.5,
  "humidity": 65.2,
  "pressure": 1012.4,
  "gas_adc": 320,
  "gas_digital": false,
  "temp_status": "NORMAL",
  "humidity_status": "NORMAL",
  "pressure_status": "NORMAL",
  "gas_status": "NORMAL",
  "overall_status": "NORMAL",
  "timestamp": 123456
}
```

**Topic `iot/room-safety/alert`** (hanya saat DANGER):
```json
{
  "overall_status": "DANGER",
  "temperature": 28.5,
  "humidity": 65.2,
  "pressure": 1012.4,
  "gas_adc": 2450,
  "message": "Kondisi DANGER terdeteksi, periksa ruangan segera!",
  "timestamp": 123456
}
```

**Topic `iot/room-safety/heartbeat`** (setiap 15 detik, untuk status online/offline):
```json
{ "status": "online", "uptime_ms": 123456 }
```

## ✅ Checklist Sebelum Demo
- [ ] I2C scanner sudah konfirmasi semua alamat
- [ ] MQ-2 sudah warm-up minimal 30 detik sebelum tes gas
- [ ] Kredensial HiveMQ sama di firmware (port 8883) & dashboard (port WSS, biasanya 8884)
- [ ] Bot Telegram sudah dites kirim pesan manual dulu
- [ ] Dashboard sudah dites bisa connect (cek console browser untuk error MQTT)
