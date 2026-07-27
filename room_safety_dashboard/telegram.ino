/*
 * telegram.ino - Optimized Telegram Bot API module using UniversalTelegramBot.
 * Executes on Core 0 to prevent blocking Core 1.
 */

#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include "types.h"
#include "config.h"

extern WiFiClientSecure telegramClient;

// Objek bot Telegram global menggunakan token rahasia dari config_secret.h
UniversalTelegramBot bot(TELEGRAM_BOT_TOKEN, telegramClient);

String statusToString(Status s);
void printTimestamp();

// ====================== HELPER: FORMAT UPTIME ======================
String buildUptimeString() {
  unsigned long ms      = millis();
  unsigned long seconds = ms / 1000;
  unsigned long minutes = seconds / 60;
  unsigned long hours   = minutes / 60;
  char buf[16];
  snprintf(buf, sizeof(buf), "%02lu:%02lu:%02lu",
           hours % 24, minutes % 60, seconds % 60);
  return String(buf);
}

// ====================== KIRIM ALERT DANGER ======================
void sendTelegramAlert(const SensorReading& r, const SystemState& s) {
  String message;
  message.reserve(512);
  message = "🚨 <b>ROOM SAFETY ALERT</b> 🚨\n";
  message += "⚠️ <b>Kondisi DANGER Terdeteksi!</b> ⚠️\n\n";
  message += "📊 <b>Data Sensor Real-time:</b>\n";
  message += "🌡️ <b>Suhu:</b> <code>" + (isnan(r.temperature) ? "0.0" : String(r.temperature, 1)) + " °C</code> (" + statusToString(s.tempStatus) + ")\n";
  message += "💧 <b>Kelembapan:</b> <code>" + (isnan(r.humidity) ? "0.0" : String(r.humidity, 1)) + " %</code> (" + statusToString(s.humStatus) + ")\n";
  message += "🌪️ <b>Tekanan:</b> <code>" + (isnan(r.pressure) ? "0" : String((int)r.pressure)) + " hPa</code> (" + statusToString(s.presStatus) + ")\n";
  message += "☁️ <b>Gas MQ-2:</b> <code>" + String(r.gasADC) + "</code> (" + statusToString(s.gasStatus) + ")\n";
  message += "🔥 <b>Api (Flame):</b> <code>" + String(r.flameADC) + "</code> (" + statusToString(s.flameStatus) + ")\n\n";
  message += "🔴 <b>Status Keseluruhan:</b> <b>" + statusToString(s.overallStatus) + "</b>\n";
  message += "⏰ <b>Uptime ESP32:</b> <code>" + buildUptimeString() + "</code>\n\n";
  message += "<i>Mohon segera periksa ruangan Anda!</i>";

  printTimestamp();
  Serial.print("[TELEGRAM] Mengirim notifikasi alert... ");
  bool ok = bot.sendMessage(TELEGRAM_CHAT_ID, message, "HTML");
  Serial.println(ok ? "TERKIRIM!" : "GAGAL!");
}

// ====================== BALAS /status ======================
void sendTelegramStatus(String chatId, const SensorReading& r, const SystemState& s) {
  String overallEmoji = "🟢";
  if (s.overallStatus == WARNING) overallEmoji = "🟡";
  if (s.overallStatus == DANGER)  overallEmoji = "🔴";

  String message;
  message.reserve(512);
  message = "📊 <b>STATUS RUANGAN REAL-TIME</b> 📊\n\n";
  message += "🌡️ <b>Suhu:</b> <code>" + (isnan(r.temperature) ? "0.0" : String(r.temperature, 1)) + " °C</code> (" + statusToString(s.tempStatus) + ")\n";
  message += "💧 <b>Kelembapan:</b> <code>" + (isnan(r.humidity) ? "0.0" : String(r.humidity, 1)) + " %</code> (" + statusToString(s.humStatus) + ")\n";
  message += "🌪️ <b>Tekanan:</b> <code>" + (isnan(r.pressure) ? "0" : String((int)r.pressure)) + " hPa</code> (" + statusToString(s.presStatus) + ")\n";
  message += "☁️ <b>Gas MQ-2:</b> <code>" + String(r.gasADC) + "</code> (" + statusToString(s.gasStatus) + ")\n";
  message += "🔥 <b>Api (Flame):</b> <code>" + String(r.flameADC) + "</code> (" + statusToString(s.flameStatus) + ")\n\n";
  message += overallEmoji + " <b>Status Keseluruhan:</b> <b>" + statusToString(s.overallStatus) + "</b>\n";
  message += "⏰ <b>Uptime ESP32:</b> <code>" + buildUptimeString() + "</code>";

  printTimestamp();
  Serial.printf("[TELEGRAM] Membalas /status ke chat:%s... ", chatId.c_str());
  bool ok = bot.sendMessage(chatId, message, "HTML");
  Serial.println(ok ? "TERKIRIM!" : "GAGAL!");
}

// ====================== BALAS /help ======================
void sendTelegramHelp(String chatId) {
  String message;
  message.reserve(256);
  message = "🤖 <b>ESP32 Room Safety Bot</b> 🤖\n\n";
  message += "Berikut perintah yang tersedia:\n";
  message += "💬 <code>/status</code> - Cek kondisi sensor real-time\n";
  message += "💬 <code>/help</code>   - Tampilkan pesan bantuan ini";

  printTimestamp();
  Serial.printf("[TELEGRAM] Membalas /help ke chat:%s... ", chatId.c_str());
  bool ok = bot.sendMessage(chatId, message, "HTML");
  Serial.println(ok ? "TERKIRIM!" : "GAGAL!");
}

// ====================== POLLING BOT (getUpdates) ======================
void handleTelegramBot(const SensorReading& r, const SystemState& s) {
  // Ambil update baru menggunakan library UniversalTelegramBot secara aman
  int numNewMessages = bot.getUpdates(bot.last_message_received + 1);

  while (numNewMessages) {
    for (int i = 0; i < numNewMessages; i++) {
      String chatId = String(bot.messages[i].chat_id);
      String text = bot.messages[i].text;

      if (text == "/status") {
        sendTelegramStatus(chatId, r, s);
      } else if (text == "/start" || text == "/help") {
        sendTelegramHelp(chatId);
      }
    }
    numNewMessages = bot.getUpdates(bot.last_message_received + 1);
  }
}
