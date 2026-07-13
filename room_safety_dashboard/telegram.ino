/*
 * telegram.ino - Optimized Telegram Bot API module.
 * Executes on Core 0 to prevent blocking Core 1.
 * Features custom lightweight string parsing for incoming commands to avoid
 * the memory and CPU overhead of JSON DOM libraries.
 */

#include <WiFiClientSecure.h>
#include "types.h"
#include "config.h"

String statusToString(Status s);
void printTimestamp();

// ====================== HELPER: URL ENCODE ======================
String urlEncode(const String& str) {
  String encoded = "";
  char c, code0, code1;
  for (size_t i = 0; i < str.length(); i++) {
    c = str.charAt(i);
    if (isalnum(c)) {
      encoded += c;
    } else if (c == ' ') {
      encoded += '+';
    } else if (c == '\n') {
      encoded += "%0A";
    } else {
      code1 = (c & 0xf) + '0';
      if ((c & 0xf) > 9) code1 = (c & 0xf) - 10 + 'A';
      c = (c >> 4) & 0xf;
      code0 = c + '0';
      if (c > 9) code0 = c - 10 + 'A';
      encoded += '%';
      encoded += code0;
      encoded += code1;
    }
  }
  return encoded;
}

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

// ====================== HTTP POST KE TELEGRAM ======================
static bool sendTelegramMessage(const String& chat_id, const String& message) {
  WiFiClientSecure tgClient;
  tgClient.setInsecure(); // Hilangkan overhead parsing CA cert untuk menghemat RAM

  String url     = "/bot" + String(TELEGRAM_BOT_TOKEN) + "/sendMessage";
  String payload = "chat_id=" + chat_id +
                   "&parse_mode=HTML" +
                   "&text=" + urlEncode(message);

  if (!tgClient.connect("api.telegram.org", 443)) {
    Serial.println("[TELEGRAM] GAGAL terhubung ke api.telegram.org");
    return false;
  }

  // Kirim HTTP POST request minimalis
  tgClient.print("POST " + url + " HTTP/1.1\r\n" +
                 "Host: api.telegram.org\r\n" +
                 "Content-Type: application/x-www-form-urlencoded\r\n" +
                 "Content-Length: " + String(payload.length()) + "\r\n" +
                 "Connection: close\r\n\r\n" +
                 payload + "\r\n");

  // Tunggu sejenak agar data terkirim sebelum stop client
  unsigned long timeout = millis();
  while (tgClient.connected() && millis() - timeout < 3000) {
    if (tgClient.available()) break;
    delay(10);
  }

  tgClient.stop();
  return true;
}

// ====================== KIRIM ALERT DANGER ======================
void sendTelegramAlert(const SensorReading& r, const SystemState& s) {
  String message = "🚨 <b>ROOM SAFETY ALERT</b> 🚨\n";
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
  bool ok = sendTelegramMessage(String(TELEGRAM_CHAT_ID), message);
  Serial.println(ok ? "TERKIRIM!" : "GAGAL!");
}

// ====================== BALAS /status ======================
void sendTelegramStatus(String chat_id, const SensorReading& r, const SystemState& s) {
  String overallEmoji = "🟢";
  if (s.overallStatus == WARNING) overallEmoji = "🟡";
  if (s.overallStatus == DANGER)  overallEmoji = "🔴";

  String message = "📊 <b>STATUS RUANGAN REAL-TIME</b> 📊\n\n";
  message += "🌡️ <b>Suhu:</b> <code>" + (isnan(r.temperature) ? "0.0" : String(r.temperature, 1)) + " °C</code> (" + statusToString(s.tempStatus) + ")\n";
  message += "💧 <b>Kelembapan:</b> <code>" + (isnan(r.humidity) ? "0.0" : String(r.humidity, 1)) + " %</code> (" + statusToString(s.humStatus) + ")\n";
  message += "🌪️ <b>Tekanan:</b> <code>" + (isnan(r.pressure) ? "0" : String((int)r.pressure)) + " hPa</code> (" + statusToString(s.presStatus) + ")\n";
  message += "☁️ <b>Gas MQ-2:</b> <code>" + String(r.gasADC) + "</code> (" + statusToString(s.gasStatus) + ")\n";
  message += "🔥 <b>Api (Flame):</b> <code>" + String(r.flameADC) + "</code> (" + statusToString(s.flameStatus) + ")\n\n";
  message += overallEmoji + " <b>Status Keseluruhan:</b> <b>" + statusToString(s.overallStatus) + "</b>\n";
  message += "⏰ <b>Uptime ESP32:</b> <code>" + buildUptimeString() + "</code>";

  printTimestamp();
  Serial.printf("[TELEGRAM] Membalas /status ke chat:%s... ", chat_id.c_str());
  bool ok = sendTelegramMessage(chat_id, message);
  Serial.println(ok ? "TERKIRIM!" : "GAGAL!");
}

// ====================== BALAS /help ======================
void sendTelegramHelp(String chat_id) {
  String message = "🤖 <b>ESP32 Room Safety Bot</b> 🤖\n\n";
  message += "Berikut perintah yang tersedia:\n";
  message += "💬 <code>/status</code> - Cek kondisi sensor real-time\n";
  message += "💬 <code>/help</code>   - Tampilkan pesan bantuan ini";

  printTimestamp();
  Serial.printf("[TELEGRAM] Membalas /help ke chat:%s... ", chat_id.c_str());
  bool ok = sendTelegramMessage(chat_id, message);
  Serial.println(ok ? "TERKIRIM!" : "GAGAL!");
}

// ====================== POLLING BOT (getUpdates) ======================
// Parsing manual hemat memori (zero allocation DOM) untuk efisiensi CPU maksimal
void handleTelegramBot(const SensorReading& r, const SystemState& s) {
  static long lastTelegramUpdateId = 0;

  WiFiClientSecure tgClient;
  tgClient.setInsecure();

  String url = "/bot" + String(TELEGRAM_BOT_TOKEN) + "/getUpdates?limit=5";
  if (lastTelegramUpdateId > 0) {
    url += "&offset=" + String(lastTelegramUpdateId + 1);
  }

  if (!tgClient.connect("api.telegram.org", 443)) return;

  // Gunakan HTTP/1.0 untuk menghindari transfer-encoding chunked yang lambat diparse
  tgClient.println("GET " + url + " HTTP/1.0");
  tgClient.println("Host: api.telegram.org");
  tgClient.println("Connection: close");
  tgClient.println();

  // Skip HTTP headers
  while (tgClient.connected()) {
    String line = tgClient.readStringUntil('\n');
    if (line == "\r") break;
  }

  // Set timeout agar tidak hang jika server lambat menutup koneksi
  tgClient.setTimeout(5000);
  String response = tgClient.readString();
  tgClient.stop();

  if (response.length() == 0) return;

  // ── CUSTOM LIGHTWEIGHT STRING SCANNER ───────────────────────────────────
  int idx = 0;
  while ((idx = response.indexOf("\"update_id\":", idx)) != -1) {
    idx += 12;
    int endIdx = response.indexOf(",", idx);
    if (endIdx == -1) break;
    long updateId = response.substring(idx, endIdx).toInt();
    lastTelegramUpdateId = updateId;

    // Cari teks perintah di update ini
    int textIdx = response.indexOf("\"text\":\"", idx);
    if (textIdx == -1) break;
    textIdx += 8;
    int textEndIdx = response.indexOf("\"", textIdx);
    if (textEndIdx == -1) break;
    String text = response.substring(textIdx, textEndIdx);

    // Cari ID obrolan (chat ID)
    int chatIdx = response.indexOf("\"chat\":{", idx);
    if (chatIdx == -1) break;
    int chatIdIdx = response.indexOf("\"id\":", chatIdx);
    if (chatIdIdx == -1) break;
    chatIdIdx += 5;
    int chatIdEndIdx = response.indexOf(",", chatIdIdx);
    if (chatIdEndIdx == -1) break;
    String chatId = response.substring(chatIdIdx, chatIdEndIdx);
    chatId.trim();

    // Jalankan eksekusi command
    if (text == "/status") {
      sendTelegramStatus(chatId, r, s);
    } else if (text == "/start" || text == "/help") {
      sendTelegramHelp(chatId);
    }
    
    idx = textEndIdx; // Lompati ke pemindaian update berikutnya
  }
}
