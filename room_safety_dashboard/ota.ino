/*
 * ota.ino - Over-the-Air firmware update menggunakan ArduinoOTA.
 * Di-handle di Core 0 bersama networkTask.
 * Upload via Arduino IDE: Tools → Port → IP address ESP32.
 * Password: lihat OTA_PASSWORD di config.h
 */

#include <ArduinoOTA.h>
#include "config.h"

extern void setLEDColor(uint8_t r, uint8_t g, uint8_t b);

void otaSetup() {
  ArduinoOTA.setHostname(OTA_HOSTNAME);
  ArduinoOTA.setPassword(OTA_PASSWORD);

  ArduinoOTA.onStart([]() {
    String type = (ArduinoOTA.getCommand() == U_FLASH) ? "sketch" : "filesystem";
    Serial.println("[OTA] Mulai upload: " + type);
    setLEDColor(0, 0, 128); // biru redup saat OTA
  });

  ArduinoOTA.onEnd([]() {
    Serial.println("\n[OTA] Selesai! Restarting...");
    setLEDColor(0, 255, 0);
  });

  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("[OTA] Progress: %u%%\r", (progress / (total / 100)));
  });

  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("[OTA] Error[%u]: ", error);
    if      (error == OTA_AUTH_ERROR)    Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR)   Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR)     Serial.println("End Failed");
    setLEDColor(255, 0, 0);
  });

  ArduinoOTA.begin();
  Serial.printf("[OTA] Ready. Hostname: %s\n", OTA_HOSTNAME);
}
