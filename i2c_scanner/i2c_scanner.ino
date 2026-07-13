/*
 * i2c_scanner.ino
 * Program untuk memverifikasi alamat I2C dari semua sensor (AHT20, BMP280, LCD)
 * Menggunakan pin I2C semula (SDA=21, SCL=22)
 */

#include <Wire.h>

#define PIN_SDA 21
#define PIN_SCL 22

void setup() {
  Serial.begin(115200);
  while (!Serial); // Tunggu Serial Monitor terbuka
  
  Serial.println("\n==================================================");
  Serial.println("[I2C Scanner] Memulai scanning I2C...");
  Serial.printf("  - SDA: GPIO %d\n", PIN_SDA);
  Serial.printf("  - SCL: GPIO %d\n", PIN_SCL);
  Serial.println("==================================================");

  Wire.begin(PIN_SDA, PIN_SCL);
}

void loop() {
  byte error, address;
  int devicesFound = 0;

  Serial.println("Scanning...");

  for (address = 1; address < 127; address++) {
    Wire.beginTransmission(address);
    error = Wire.endTransmission();

    if (error == 0) {
      Serial.printf("Device ditemukan di alamat: 0x%02X", address);

      if (address == 0x38) {
        Serial.print("  <- [AHT20] Sensor Suhu & Kelembapan");
      } else if (address == 0x76) {
        Serial.print("  <- [BMP280] Sensor Tekanan (Default)");
      } else if (address == 0x77) {
        Serial.print("  <- [BMP280] Sensor Tekanan (Alternatif)");
      } else if (address == 0x27) {
        Serial.print("  <- [LCD PCF8574] Backlight LCD (Default)");
      } else if (address == 0x3F) {
        Serial.print("  <- [LCD PCF8574] Backlight LCD (Alternatif)");
      }

      Serial.println();
      devicesFound++;
    } else if (error == 4) {
      Serial.printf("Error tidak diketahui pada alamat: 0x%02X\n", address);
    }
  }

  if (devicesFound == 0) {
    Serial.println("Tidak ada device I2C ditemukan! Cek kembali wiring SDA/SCL & VCC/GND.");
  } else {
    Serial.printf("Scanning selesai. Ditemukan %d device.\n\n", devicesFound);
  }

  delay(5000); // Scan ulang setiap 5 detik
}
