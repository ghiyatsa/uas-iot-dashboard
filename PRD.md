# Product Requirement Document (PRD)
## IoT-Based Room Safety & Environment Dashboard for Real-Time Monitoring and Early Hazard Detection

---

### Informasi Proyek & Tim

* **Judul Proyek**: IoT-Based Room Safety & Environment Dashboard for Real-Time Monitoring and Early Hazard Detection
* **Kelompok**: Tim Kelompok 2
* **Anggota Tim**:
  1. Said Al-Ghiyats (230170162) - **Ketua Tim**
  2. Putri Alifi Nurfadilla (230170128) - **Anggota**
  3. Nurfauziah (240170003) - **Anggota**
  4. Naghiya kamila rahmah (240170216) - **Anggota**
  5. Anisah (240170040) - **Anggota**
  6. Ariifah Yaasir Harahap (240170219) - **Anggota**

---

### 1. Pendahuluan
Dokumen Kebutuhan Produk (PRD) ini menjelaskan spesifikasi sistem, perangkat keras, perangkat lunak, indikator status, serta integrasi eksternal untuk proyek **IoT-Based Room Safety & Environment Dashboard**. Sistem ini dirancang untuk mendeteksi bahaya lingkungan dini (seperti kebocoran gas dan kebakaran) serta memantau parameter lingkungan ruangan secara real-time demi keselamatan penghuni dan aset.

### 2. Tujuan Sistem
* **Pemantauan Real-time**: Mengukur suhu, kelembapan, tekanan udara, konsentrasi gas/asap, dan mendeteksi keberadaan api secara terus-menerus.
* **Deteksi Dini Bahaya (Early Hazard Detection)**: Memproses data sensor untuk mendeteksi kondisi berbahaya (seperti kebocoran gas atau api) dan memberikan respons instan di lokasi.
* **Dashboard Terpusat**: Menyajikan data historis dan real-time secara visual di web untuk kemudahan pemantauan jarak jauh.
* **Notifikasi Instan**: Mengirimkan pemberitahuan darurat otomatis langsung ke aplikasi Telegram pengguna saat terjadi kondisi bahaya.

---

### 3. Arsitektur Sistem
Sistem ini menggunakan arsitektur IoT tiga lapis (*Three-Tier IoT Architecture*):
```mermaid
graph TD
    subgraph Edge Layer (ESP32 DevKitC V1)
        AHT20[AHT20 Temp & Hum] -->|I2C 0x38| ESP32[ESP32 MCU]
        BMP280[BMP280 Pressure] -->|I2C 0x77| ESP32
        MQ2[MQ-2 Gas Sensor] -->|Analog GPIO 32| ESP32
        FLAME[Flame Sensor] -->|Analog GPIO 33| ESP32
        ESP32 -->|I2C 0x27| LCD[LCD 16x2]
        ESP32 -->|PWM GPIO 25, 26, 27| RGB[LED RGB]
        ESP32 -->|GPIO 23| BUZZER[Buzzer Aktif]
    end

    subgraph Network & Cloud Layer
        ESP32 -->|WiFi & MQTT TLS 8883| HiveMQ[HiveMQ Cloud Broker]
        ESP32 -->|HTTPS API| Telegram[Telegram Bot API]
    end

    subgraph Presentation Layer
        Dashboard[React + Vite Web App] -->|WSS Port 8884| HiveMQ
        Telegram -->|Notifikasi & Command /status| User[Pengguna / Telegram Client]
    end
```

---

### 4. Spesifikasi Perangkat Keras (Hardware Specifications)
* **Mikrokontroler**: ESP32 DevKitC V1
* **Sensor Lingkungan**:
  * **AHT20**: Sensor suhu (°C) dan kelembapan (%RH) berbasis protokol I2C di alamat `0x38`.
  * **BMP280**: Sensor tekanan barometrik (hPa) berbasis protokol I2C di alamat `0x77`.
  * **MQ-2**: Sensor gas dan asap (koneksi analog AO ke pin `GPIO 32`).
  * **Flame Sensor**: Sensor pendeteksi api (koneksi analog AO ke pin `GPIO 33`, logika aktif-rendah/active-low).
* **Aktuator & Output Fisik**:
  * **LCD 16x2**: Menampilkan status dan parameter sensor secara lokal dengan modul I2C Backpack di alamat `0x27`.
  * **Buzzer Aktif**: Output suara alarm yang terhubung ke pin `GPIO 23`.
  * **LED RGB**: Indikator status visual multi-warna menggunakan pin PWM (`GPIO 25` - Red, `GPIO 26` - Green, `GPIO 27` - Blue).

---

### 5. Konfigurasi Pinout & Warna Kabel
Untuk memudahkan proses perakitan dan pelacakan kesalahan (debugging), proyek ini menerapkan standarisasi warna kabel (*Wire Color Code*) sebagai berikut:

| Nama Jalur / Sensor | Pin ESP32 | Warna Kabel | Deskripsi / Fungsi |
| :--- | :--- | :--- | :--- |
| **VCC / 5V / 3.3V** | VIN / 3V3 | **Merah (Red)** | Jalur Tegangan Positif |
| **GND** | GND | **Hitam (Black)** | Jalur Ground Bersama |
| **I2C SDA** | GPIO 21 | **Hijau (Green)** | Komunikasi Data I2C (AHT20, BMP280, LCD) |
| **I2C SCL** | GPIO 22 | **Kuning (Yellow)**| Komunikasi Clock I2C (AHT20, BMP280, LCD) |
| **Gas MQ-2 (AO)** | GPIO 32 | **Ungu (Purple)** | Sinyal Analog Konsentrasi Gas |
| **Flame Sensor (AO)**| GPIO 33 | **Putih (White)** | Sinyal Analog Deteksi Api |
| **Buzzer** | GPIO 23 | **Cokelat (Brown)**| Kontrol Suara Alarm |
| **LED RGB - Red** | GPIO 25 | **Oranye (Orange)**| Jalur PWM LED Merah |
| **LED RGB - Green** | GPIO 26 | **Abu-abu (Grey)** | Jalur PWM LED Hijau |
| **LED RGB - Blue** | GPIO 27 | **Biru (Blue)** | Jalur PWM LED Biru |

---

### 6. Matriks Logika Status & Respon Aktuator
Sistem mengevaluasi kondisi ruangan ke dalam tiga tingkat status: **NORMAL**, **WARNING**, dan **DANGER**. Status keseluruhan (*overall status*) mengadopsi tingkat keparahan tertinggi (*worst-of-all*) dari seluruh sensor.

#### 6.1 Batas Ambang Parameter (Thresholds)
* **Suhu (°C)**:
  * `NORMAL`: 20.0°C – 25.0°C (zona nyaman)
  * `WARNING`: Di luar batas normal, namun masih dalam batas aman 18.0°C – 28.0°C, atau di luar itu (sensor ini tidak memicu status `DANGER`).
* **Kelembapan (%RH)**:
  * `NORMAL`: 40% – 60%
  * `WARNING`: Di luar batas normal, namun masih dalam batas aman 30% – 70%, atau di luar itu.
* **Tekanan (hPa)**:
  * `NORMAL`: 950 hPa – 1020 hPa
  * `WARNING`: Di luar batas normal.
* **Gas MQ-2 (Nilai ADC 0–4095)**:
  * `NORMAL`: ≤ 1000 ADC
  * `WARNING`: 1001 – 2000 ADC
  * `DANGER`: > 2000 ADC
* **Flame Sensor (Nilai ADC 0–4095 - Active-Low)**:
  * `NORMAL`: ≥ 3000 ADC (tidak ada api)
  * `WARNING`: 1500 – 2999 ADC (api lemah atau jarak jauh)
  * `DANGER`: < 1500 ADC (api kuat atau sangat dekat)

#### 6.2 Perilaku Aktuator Berdasarkan Status

| Status Keseluruhan | Indikator LED RGB | Perilaku Buzzer | Tampilan Layar LCD 16x2 |
| :--- | :--- | :--- | :--- |
| **NORMAL** | **Hijau Solid** | Mati (LOW) | Baris 0: `T:<Suhu>C H:<Hum>% NRM`<br>Baris 1: `P:<Tekanan>  ONLINE/OFFLINE` |
| **WARNING** | **Kuning Blink lambat** (interval 500ms) | Mati (LOW) | Baris 0: `T:<Suhu>C H:<Hum>% WRN`<br>Baris 1: `P:<Tekanan>  ONLINE/OFFLINE` |
| **DANGER (Gas)** | **Oranye Blink cepat** (interval 200ms) | Nada detektor gas lambat (400ms ON / 400ms OFF) | Baris 0: `! GAS DETECTED !`<br>Baris 1: `OPEN VENTILATION` |
| **DANGER (Api)** | **Merah Blink cepat** (interval 200ms) | Nada alarm kebakaran cepat (100ms ON / 100ms OFF) | Baris 0: `!! FIRE ALARM !!`<br>Baris 1: ` EVACUATE NOW!! ` |
| **DANGER (Gas + Api)**| **Merah & Oranye Bergantian** (setiap 150ms) | Menyala terus-menerus (HIGH) | Baris 0: `!! FIRE ALARM !!`<br>Baris 1: ` EVACUATE NOW!! ` |

---

### 7. Rekayasa Stabilitas & Fitur Keamanan Firmware
Untuk menjamin keandalan sistem dalam jangka panjang di lingkungan nyata, firmware dilengkapi dengan beberapa mekanisme khusus:
1. **Pencegahan Resiko Brownout (Brownout Detector Disable)**:
   Menggunakan `WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0)` saat inisialisasi untuk menonaktifkan brownout detector, guna mencegah chip melakukan reset mendadak saat modul WiFi menarik lonjakan arus puncak (~300mA) dari catu daya yang kurang stabil.
2. **Pemanasan Awal MQ-2 (MQ-2 Warm-up Delay)**:
   Menerapkan penundaan evaluasi gas selama **30 detik** (`MQ2_WARMUP_MS`) setelah perangkat menyala. Hal ini memberi waktu bagi elemen pemanas sensor gas untuk mencapai suhu stabil sebelum hasil bacaan dinilai valid, guna menghindari alarm palsu (*false alarm*) saat booting.
3. **Filter Kebisingan Analog (Analog Multi-Sampling)**:
   Setiap pembacaan analog untuk sensor gas dan api mengambil rata-rata dari **10 sampel** (`ADC_SAMPLE_COUNT`) dengan interval 5ms untuk menyaring fluktuasi noise kelistrikan frekuensi tinggi.
4. **Konfirmasi Bahaya Ganda (Gas Danger Confirmation Count)**:
   Status `DANGER` untuk gas hanya akan terpicu jika konsentrasi gas melampaui ambang batas berbahaya sebanyak **2 kali berturut-turut** (`DANGER_CONFIRM_COUNT`). Hal ini mencegah kebisingan sinyal sesaat memicu alarm palsu.
5. **Penguncian Status Bahaya (Danger Status Latching)**:
   Fungsi `latchDanger` menahan status `DANGER` selama minimal **10 detik** setelah pemicu bahaya hilang. Ini memastikan alarm tidak mati-nyala secara cepat (*flickering*) ketika nilai sensor berfluktuasi tepat di garis batas ambang.
6. **Penanganan Re-koneksi Tanpa Hambatan (Non-blocking Reconnection)**:
   Koneksi ulang WiFi (`WIFI_RECONNECT_MS` = 30 detik) dan MQTT (`MQTT_RECONNECT_MS` = 10 detik) berjalan secara asinkron di dalam *main loop* menggunakan logika berbasis `millis()`. Hal ini menjamin pembacaan sensor lokal dan aksi keselamatan (RGB, LCD, Buzzer) tetap berjalan lancar walaupun koneksi internet terputus.

---

### 8. Spesifikasi Integrasi & Protokol IoT

#### 8.1 Protokol MQTT (Message Queuing Telemetry Transport)
* **Broker**: HiveMQ Cloud (menggunakan enkripsi SSL/TLS pada port `8883` untuk ESP32, dan Secure WebSockets `8884` untuk Dashboard Web).
* **Identitas Klien (Client ID)**: Menggunakan ID unik berbasis MAC Address perangkat (contoh: `esp32-room-safety-A1B2C3`) untuk mencegah terjadinya konflik koneksi.
* **Topik MQTT**:
  * `iot/room-safety/data`: Mengirim data telemetri berkala (JSON) setiap 1 detik.
  * `iot/room-safety/alert`: Mengirim pesan darurat (JSON) saat status berada di tingkat `DANGER`.
  * `iot/room-safety/heartbeat`: Mengirim tanda aktif (heartbeat) setiap 15 detik.
  * `iot/room-safety/cmd/#`: Menerima perintah kontrol dari luar.

**Skema Payload Telemetri (`iot/room-safety/data`)**:
```json
{
  "t": 24.5,
  "h": 52.3,
  "p": 1011.2,
  "g": 485,
  "ts": "NORMAL",
  "hs": "NORMAL",
  "ps": "NORMAL",
  "gs": "NORMAL",
  "os": "NORMAL",
  "ms": 452000
}
```

#### 8.2 Integrasi Telegram Bot (Asynchronous Notifications)
* **API Komunikasi**: Telegram Bot API menggunakan HTTPS Secure Client.
* **Arsitektur Multi-Core (FreeRTOS Task)**:
  Karena proses jabat tangan (*handshake*) HTTPS Telegram dapat memakan waktu 5 hingga 15 detik, proses ini didelegasikan ke tugas asinkron (`telegramTask`) yang disematkan secara eksklusif ke **Core 0** ESP32. Hal ini memastikan loop utama pemantauan sensor pada **Core 1** tidak terganggu (*blocking*), sehingga koneksi MQTT tidak terputus karena terabaikannya batas *keep-alive*.
* **Fitur Utama**:
  * **Notifikasi Otomatis**: Mengirim notifikasi darurat secara instan jika mendeteksi status `DANGER` (dilengkapi proteksi banjir pesan dengan cooldown 60 detik).
  * **Interaksi Bot**: Menanggapi perintah chat `/status` untuk meminta laporan sensor real-time dan `/help` atau `/start` untuk panduan penggunaan.

---

### 9. Spesifikasi Dashboard Web
Dashboard dikembangkan menggunakan **React.js** dan di-build dengan bundler **Vite.js** untuk performa tinggi. Aplikasi ini berjalan sepenuhnya di sisi klien (*client-side static app*).

* **Konektivitas**: Terhubung langsung ke HiveMQ Cloud menggunakan modul `mqtt` via Secure WebSockets (port `8884`, path `/mqtt`).
* **Fitur Utama Antarmuka (UI)**:
  1. **Header Panel**: Menunjukkan indikator status koneksi MQTT Broker, status keaktifan perangkat fisik (Device Online/Offline berbasis heartbeat), dan status keselamatan keseluruhan (*Overall Room Status*).
  2. **Grid Kartu Sensor (Sensor Card Grid)**: 4 buah kartu interaktif yang memperlihatkan nilai real-time dan status dari Suhu, Kelembapan, Tekanan, dan Gas (ADC).
  3. **Grafik Riwayat (History Chart)**: Grafik dinamis interaktif yang melacak tren historis parameter sensor dari waktu ke waktu. Pengguna dapat memilih parameter yang ingin difokuskan dengan mengeklik kartu sensor yang sesuai.
  4. **Spanduk Peringatan (Alert Banner)**: Banner merah menyala dengan animasi darurat yang otomatis muncul ketika menerima kiriman data dari topik `iot/room-safety/alert`.
* **Desain UI/UX**:
  * Menggunakan palet warna modern (Hijau untuk Normal, Kuning untuk Warning, dan Merah untuk Danger).
  * Desain responsif (*Responsive Design*) yang menyesuaikan tata letak saat dibuka melalui komputer desktop, tablet, maupun ponsel pintar.
  * Efek visual dinamis seperti perubahan latar belakang halaman menjadi merah berkedip saat status ruangan beralih ke `DANGER`.
