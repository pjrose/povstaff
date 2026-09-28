#include "StaffNetwork.h"
#include "Config.h"
#include "Dashboard.h"
#include "Preview.h"
#include "Engine.h"
#include "Sequences.h"
#include "NetworkValidation.h"
#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <ArduinoOTA.h>
#include <Update.h>
#include <Preferences.h>
#include <LittleFS.h>
#include <esp_ota_ops.h>

namespace StaffNetwork {
namespace {
WebServer server(80);
NetworkValidation::WifiSettings saved{}, boot{};
String apName;
bool apActive = false, everConnected = false, arduinoActive = false, wasConnected = false;
uint32_t wifiStarted = 0, rebootAt = 0, otaStarted = 0;
constexpr uint32_t TRANSFER_TIMEOUT = 15000, TRANSFER_LIMIT = 180000;
constexpr const char* STAGE = "/.upload.tmp";
struct Transfer {
  bool pending = false, firmware = false, authorized = false, ended = false, updating = false;
  unsigned parts = 0;
  size_t received = 0, expected = 0;
  uint32_t lastData = 0, started = 0;
  String name, error;
  File file;
} transfer;

String quote(const String& input) {
  String out = "\"";
  for (size_t i = 0; i < input.length(); ++i) {
    const uint8_t c = input[i];
    if (c == '"' || c == '\\') { out += '\\'; out += char(c); }
    else if (c < 32) { char encoded[7]; snprintf(encoded, sizeof(encoded), "\\u%04x", c); out += encoded; }
    else out += char(c);
  }
  return out + '"';
}
void json(int code, const String& body) {
  server.sendHeader("Cache-Control", "no-store");
  server.sendHeader("X-Content-Type-Options", "nosniff");
  server.send(code, "application/json", body);
}
void fail(const String& reason, int code = 400) { json(code, "{\"ok\":false,\"error\":" + quote(reason) + "}"); }
void ok() { json(200, "{\"ok\":true}"); }
bool guard(bool mutation = false) {
  if (mutation && server.header("X-POV-Request") != "1") { fail("Missing request token", 403); return false; }
  if (mutation && (arduinoActive || rebootAt)) { fail("Firmware update or reboot in progress", 409); return false; }
  return true;
}
bool argument(const char* key, uint32_t limit, uint32_t& value) {
  if (!server.hasArg(key) || !NetworkValidation::number(server.arg(key).c_str(), limit, value)) {
    fail(String("Invalid ") + key); return false;
  }
  return true;
}
void cleanup() {
  if (transfer.file) transfer.file.close();
  if (transfer.updating) { Update.abort(); transfer.updating = false; }
  if (!transfer.firmware && Sequences::mounted()) LittleFS.remove(STAGE);
  if (!arduinoActive && !rebootAt) Engine::maintenance(false);
}
void transferFailure(const String& reason) {
  if (!transfer.error.length()) transfer.error = reason;
  Serial.printf("Transfer rejected: %s\n", reason.c_str());
  cleanup();
}
void resetTransfer() { cleanup(); transfer = Transfer{}; }
void upload(bool firmware) {
  // Raw single-file bodies avoid the core multipart parser's unbounded
  // malformed-boundary path. Reject multipart at its first file callback.
  if (server.header("Content-Type") != "application/octet-stream") {
    if (transfer.pending) resetTransfer();
    server.client().stop();
    return;
  }
  HTTPRaw& part = server.raw();
  if (part.status == RAW_START) {
    // The core's default 5-second read timeout is paid twice by a partial
    // chunk then EOF. Bound recovery from a lost phone connection to ~3 s.
    server.client().setTimeout(1500);
    if (!transfer.pending) {
      transfer = Transfer{};
      transfer.pending = true; transfer.firmware = firmware;
      transfer.started = transfer.lastData = millis();
      transfer.authorized = server.header("X-POV-Request") == "1";
    }
    ++transfer.parts;
    if (!transfer.authorized) {
      transferFailure("Request token required");
      fail(transfer.error, 403);
      server.client().stop(); return;
    }
    if (transfer.parts != 1 || firmware != transfer.firmware) { transferFailure("Upload exactly one file per request"); return; }
    if (arduinoActive || rebootAt) { transferFailure("Another update is in progress"); return; }
    transfer.name = WebServer::urlDecode(server.header("X-POV-Filename"));
    uint32_t expected = 0;
    const esp_partition_t* partition = firmware ? esp_ota_get_next_update_partition(nullptr) : nullptr;
    const uint32_t limit = firmware ? (partition ? partition->size : 0) : Config::MAX_IMAGE_BYTES + 4096U;
    if (!NetworkValidation::number(server.header("X-POV-Size").c_str(), limit, expected) || !expected ||
        server.clientContentLength() <= 0 || size_t(server.clientContentLength()) != expected) {
      transferFailure("Provide the exact file size and Content-Length"); server.client().stop(); return;
    }
    transfer.expected = expected;
    if (firmware) {
      String lower = transfer.name; lower.toLowerCase();
      if (!lower.endsWith(".bin") || !partition) {
        transferFailure("Choose a firmware .bin and provide its exact byte size"); return;
      }
      Engine::maintenance(true);
      if (!Update.begin(expected, U_FLASH)) { transferFailure("Cannot begin firmware update"); return; }
      transfer.updating = true;
    } else {
      if (!Sequences::mounted()) { transferFailure("Initialize storage in Settings first"); return; }
      if (!Sequences::validName(transfer.name)) { transferFailure("Use 1-44 letters, digits, hyphens or underscores followed by .bmp"); return; }
      if (Sequences::count() >= Config::MAX_FILES && !LittleFS.exists(("/" + transfer.name).c_str())) {
        transferFailure("Storage holds at most 50 sequences"); return;
      }
      Engine::maintenance(true);
      LittleFS.remove(STAGE);
      transfer.file = LittleFS.open(STAGE, FILE_WRITE);
      if (!transfer.file) transferFailure("Cannot create upload staging file");
    }
    return;
  }
  if (!transfer.pending) return;
  transfer.lastData = millis();
  if (part.status == RAW_ABORTED) { transferFailure("Upload interrupted"); return; }
  if (transfer.error.length() || !transfer.authorized) return;
  if (uint32_t(millis() - transfer.started) > TRANSFER_LIMIT) {
    transferFailure("Upload exceeded time limit"); server.client().stop(); return;
  }
  if (part.status == RAW_WRITE) {
    const size_t limit = transfer.expected;
    if (transfer.ended || part.currentSize > limit || transfer.received > limit - part.currentSize) {
      transferFailure("Upload exceeds allowed file size"); return;
    }
    const size_t written = firmware ? Update.write(part.buf, part.currentSize) : transfer.file.write(part.buf, part.currentSize);
    if (written != part.currentSize) {
      transferFailure(firmware ? "Firmware write failed; current firmware remains active" : "Write failed; original file preserved");
      return;
    }
    transfer.received += written;
  } else if (part.status == RAW_END) {
    if (transfer.ended || !transfer.received || transfer.received != part.totalSize ||
        !NetworkValidation::complete(transfer.received, part.totalSize, transfer.expected)) {
      transferFailure("Incomplete upload"); return;
    }
    transfer.ended = true;
    if (!firmware) {
      transfer.file.flush();
      if (transfer.file.getWriteError() || transfer.file.size() != transfer.received) {
        transferFailure("Storage write failed"); return;
      }
      transfer.file.close();
    }
    // Publication and Update.end wait for the final request handler and exact
    // body length. An aborted request must never publish data.
  }
}
void finishUpload(bool firmware) {
  if (!guard(true)) { resetTransfer(); return; }
  if (!transfer.pending || transfer.firmware != firmware || !transfer.authorized ||
      transfer.parts != 1 || !transfer.ended || transfer.error.length()) {
    const String error = transfer.error.length() ? transfer.error : "Incomplete single-file upload";
    resetTransfer(); fail(error); return;
  }
  if (firmware) {
    if (!transfer.updating || !Update.end(false) || !Update.isFinished()) {
      resetTransfer(); fail("Firmware image validation failed"); return;
    }
    transfer.updating = false;
    rebootAt = millis() + 1000;
    transfer = Transfer{};
    json(200, "{\"ok\":true,\"rebooting\":true}");
  } else {
    String reason;
    if (!Sequences::validateBMP(STAGE, reason)) { resetTransfer(); fail(reason); return; }
    // LittleFS rename replaces atomically: do not unlink the old destination.
    if (!LittleFS.rename(STAGE, ("/" + transfer.name).c_str())) {
      resetTransfer(); fail("Cannot publish upload; original file preserved"); return;
    }
    resetTransfer();
    if (!Sequences::reload()) { fail(Sequences::error(), 500); return; }
    ok();
  }
}
void saveWifi() {
  if (!guard(true)) return;
  const String mode = server.arg("mode");
  if (mode != "ap" && mode != "sta") { fail("Mode must be ap or sta"); return; }
  auto candidate = saved;
  candidate.station = mode == "sta";
  if (!candidate.station && server.arg("forget") == "1") {
    memset(candidate.ssid, 0, sizeof(candidate.ssid));
    memset(candidate.password, 0, sizeof(candidate.password));
  }
  if (candidate.station) {
    const String ssid = server.arg("ssid"), password = server.arg("password");
    if (!server.hasArg("ssid") || !server.hasArg("password") || ssid.length() > 32 || password.length() > 63 ||
        strlen(ssid.c_str()) != ssid.length() || strlen(password.c_str()) != password.length()) {
      fail("Provide an SSID and password (blank for an open network)"); return;
    }
    memset(candidate.ssid, 0, sizeof(candidate.ssid));
    memset(candidate.password, 0, sizeof(candidate.password));
    ssid.toCharArray(candidate.ssid, sizeof(candidate.ssid));
    password.toCharArray(candidate.password, sizeof(candidate.password));
  }
  if (!NetworkValidation::valid(candidate)) { fail("Invalid Wi-Fi settings: SSID 1-32 bytes; password blank or 8-63 printable characters"); return; }
  Preferences preferences;
  if (!preferences.begin("povstaff", false)) { fail("Cannot open settings storage", 500); return; }
  const bool written = preferences.putBytes("wifi", &candidate, sizeof(candidate)) == sizeof(candidate);
  preferences.end();
  if (!written) { fail("Could not save Wi-Fi settings", 500); return; }
  saved = candidate;
  json(200, "{\"ok\":true,\"message\":\"Saved. Power cycle the staff to apply Wi-Fi settings.\"}");
}
void startAP() {
  if (apActive) return;
  // A failed STA can continuously scan/change channels in AP+STA mode and
  // make recovery clients unable to associate. Fallback owns the radio until
  // the next boot; only a power cycle/reboot retries the saved client choice.
  WiFi.setAutoReconnect(false);
  if (boot.station) WiFi.disconnect(false, false);
  WiFi.mode(WIFI_AP);
  apActive = WiFi.softAP(apName.c_str());
  if (apActive) Serial.printf("Wi-Fi AP %s: http://%s/\n", apName.c_str(), WiFi.softAPIP().toString().c_str());
  else Serial.println("Wi-Fi AP startup failed; retrying");
}
void routes() {
  const char* headers[] = {"X-POV-Request", "Content-Type", "X-POV-Filename", "X-POV-Size"}; server.collectHeaders(headers, 4);
  server.on("/", HTTP_GET, []() {
    if (!guard()) return;
    server.sendHeader("Cache-Control", "no-store");
    server.sendHeader("X-Frame-Options", "DENY");
    server.send_P(200, "text/html; charset=utf-8", DASHBOARD_HTML);
  });
  server.on("/api/status", HTTP_GET, []() {
    if (!guard()) return;
    String value = "{\"version\":" + quote(Config::VERSION) + ",\"uptime\":" + String(millis()/1000) +
      ",\"heap\":" + String(ESP.getFreeHeap()) + ",\"engine\":" + Engine::statusJSON() +
      ",\"network\":" + statusJSON() + ",\"storage\":{\"mounted\":" + (Sequences::mounted() ? "true" : "false") +
      ",\"used\":" + String(Sequences::usedBytes()) + ",\"total\":" + String(Sequences::totalBytes()) +
      ",\"count\":" + String(Sequences::count()) + ",\"error\":" + quote(Sequences::error()) + "}}";
    json(200, value);
  });
  server.on("/api/files", HTTP_GET, []() { if (guard()) json(200, Sequences::filesJSON()); });
  server.on("/preview.js", HTTP_GET, []() {
    server.sendHeader("Cache-Control", "no-store");
    server.sendHeader("X-Content-Type-Options", "nosniff");
    server.send_P(200, "text/javascript; charset=utf-8", PREVIEW_JS);
  });
  server.on("/api/renderer", HTTP_POST, []() {
    if (!guard(true)) return;
    if (!Engine::setRenderer(server.arg("name"))) fail("Unknown renderer");
    else ok();
  });
  server.on("/api/files/content", HTTP_GET, []() {
    if (!guard()) return;
    const String name = server.arg("name");
    if (!Sequences::validName(name)) { fail("Invalid file name"); return; }
    String reason;
    const String path = "/" + name;
    if (!Sequences::validateBMP(path.c_str(), reason)) { fail(reason, 404); return; }
    File file = LittleFS.open(path, FILE_READ);
    if (!file) { fail("Image unavailable", 404); return; }
    server.sendHeader("Cache-Control", "no-store");
    server.sendHeader("X-Content-Type-Options", "nosniff");
    server.setContentLength(file.size());
    server.send(200, "image/bmp", "");
    auto client = server.client();
    client.setTimeout(1000);
    uint8_t buffer[1024];
    size_t remaining = file.size();
    const uint32_t started = millis();
    // One bounded download; keep rendering between network writes. Animation
    // and the twelve-entry preview cache live entirely in the browser.
    while (remaining && client.connected() && uint32_t(millis()-started) < 5000) {
      const size_t bytes = file.read(buffer, remaining < sizeof(buffer) ? remaining : sizeof(buffer));
      if (!bytes || client.write(buffer, bytes) != bytes) break;
      remaining -= bytes;
      Engine::update();
      delay(0);
    }
    if (remaining) client.stop();
  });
  server.on("/api/wifi", HTTP_POST, saveWifi);
  server.on("/api/mode", HTTP_POST, []() { if (!guard(true)) return; if (!Engine::setMode(server.arg("name"))) fail("Unknown mode"); else ok(); });
  server.on("/api/pattern", HTTP_POST, []() { if (!guard(true)) return; if (!Engine::setPattern(server.arg("name"))) fail("Unknown pattern"); else ok(); });
  server.on("/api/brightness", HTTP_POST, []() { uint32_t value; if (!guard(true) || !argument("value", Config::MAX_BRIGHTNESS, value)) return; Engine::setBrightness(value); ok(); });
  server.on("/api/color", HTTP_POST, []() {
    if (!guard(true)) return;
    String text = server.arg("value"); if (text.startsWith("#")) text.remove(0,1);
    if (text.length() != 6) { fail("Color must be six hexadecimal digits"); return; }
    uint32_t color = 0;
    for (unsigned i = 0; i < 6; ++i) {
      char c = text[i]; uint8_t digit;
      if (c >= '0' && c <= '9') digit = c-'0'; else if (c >= 'a' && c <= 'f') digit = c-'a'+10;
      else if (c >= 'A' && c <= 'F') digit = c-'A'+10; else { fail("Invalid color"); return; }
      color = (color << 4) | digit;
    }
    Engine::setColor(color); ok();
  });
  server.on("/api/lock", HTTP_POST, []() { uint32_t value; if (!guard(true) || !argument("value", 1, value)) return; Engine::setLocked(value); ok(); });
  server.on("/api/battery/show", HTTP_POST, []() { if (guard(true)) { Engine::showBattery(); ok(); } });
  server.on("/api/sequences/select", HTTP_POST, []() { uint32_t index; if (!guard(true) || !argument("index", Config::MAX_FILES-1, index)) return; if (!Sequences::select(index)) fail("Sequence unavailable"); else ok(); });
  server.on("/api/sequences/next", HTTP_POST, []() { if (!guard(true)) return; String d = server.arg("direction"); if (d != "1" && d != "-1") fail("Direction must be 1 or -1"); else if (!Sequences::next(d == "1" ? 1 : -1)) fail("No sequences available"); else ok(); });
  server.on("/api/sequences/duration", HTTP_POST, []() { uint32_t seconds; if (!guard(true) || !argument("seconds", 3600, seconds)) return; Sequences::setDuration(seconds); ok(); });
  server.on("/api/sequences/reload", HTTP_POST, []() { if (!guard(true)) return; if (!Sequences::reload()) fail(Sequences::error()); else ok(); });
  server.on("/api/files", HTTP_DELETE, []() {
    if (!guard(true)) return;
    const String name = server.arg("name");
    if (!Sequences::validName(name)) { fail("Invalid file name"); return; }
    if (!Sequences::mounted() || !LittleFS.remove(("/"+name).c_str())) { fail("Cannot delete file"); return; }
    Sequences::reload(); ok();
  });
  server.on("/api/files/upload", HTTP_POST, []() { finishUpload(false); }, []() { upload(false); });
  server.on("/api/ota", HTTP_POST, []() { finishUpload(true); }, []() { upload(true); });
  server.on("/api/storage/format", HTTP_POST, []() {
    if (!guard(true)) return;
    if (server.arg("confirm") != "ERASE") { fail("Explicit ERASE confirmation required"); return; }
    Engine::maintenance(true); const bool result = Sequences::format(); Engine::maintenance(false);
    if (result) ok(); else fail(Sequences::error(), 500);
  });
  server.on("/api/reboot", HTTP_POST, []() { if (!guard(true)) return; ok(); Engine::maintenance(true); rebootAt = millis()+750; });
  server.onNotFound([]() { if (guard()) fail("Not found", 404); });
}
}
void begin() {
  saved = {}; saved.magic = NetworkValidation::WIFI_MAGIC;
  Preferences preferences;
  if (preferences.begin("povstaff", true)) {
    NetworkValidation::WifiSettings candidate{};
    if (preferences.getBytesLength("wifi") == sizeof(candidate) &&
        preferences.getBytes("wifi", &candidate, sizeof(candidate)) == sizeof(candidate) && NetworkValidation::valid(candidate)) saved = candidate;
    preferences.end();
  }
  boot = saved;
  Serial.printf("Wi-Fi boot mode: %s\n", boot.station ? "client (AP fallback enabled)" : "staff access point");
  char name[20]; const uint64_t mac = ESP.getEfuseMac();
  snprintf(name, sizeof(name), "POVSTAFF%02X%02X", unsigned((mac >> 32)&255), unsigned((mac >> 40)&255)); apName = name;
  WiFi.persistent(false); WiFi.setHostname(Config::HOSTNAME); WiFi.setSleep(false);
  wifiStarted = millis();
  if (boot.station) { WiFi.mode(WIFI_STA); WiFi.setAutoReconnect(true); WiFi.begin(boot.ssid, boot.password); }
  else startAP();
  routes(); server.begin();
  ArduinoOTA.setHostname(Config::HOSTNAME);
  ArduinoOTA.setTimeout(3000);
  ArduinoOTA.setRebootOnSuccess(false);
  ArduinoOTA.onStart([]() {
    // Core 3.3.11 calls onStart after Update.begin but before any write.
    // abort is mandatory: returning alone would allow filesystem writes.
    if (ArduinoOTA.getCommand() != U_FLASH || transfer.pending || rebootAt) { Serial.println("ArduinoOTA rejected: firmware-only or busy"); Update.abort(); return; }
    Serial.println("ArduinoOTA firmware update started");
    arduinoActive = true; otaStarted = millis(); Engine::maintenance(true);
  });
  ArduinoOTA.onProgress([](unsigned int, unsigned int) {
    if (arduinoActive && uint32_t(millis()-otaStarted) > TRANSFER_LIMIT) Update.abort();
  });
  ArduinoOTA.onError([](ota_error_t error) { Serial.printf("ArduinoOTA error %u\n", unsigned(error)); if (arduinoActive) { arduinoActive = false; Engine::maintenance(false); } });
  ArduinoOTA.onEnd([]() { if (arduinoActive) { arduinoActive = false; rebootAt = millis()+750; } });
  ArduinoOTA.begin(); // Registers povstaff.local and Arduino upload service.
  MDNS.addService("http", "tcp", 80);
}
void update() {
  const uint32_t now = millis();
  const bool connected = WiFi.status() == WL_CONNECTED;
  if (connected != wasConnected) {
    if (connected) Serial.printf("Wi-Fi client connected: http://%s/\n", WiFi.localIP().toString().c_str());
    else Serial.println("Wi-Fi client disconnected; enabling staff AP");
    wasConnected = connected;
  }
  if (boot.station) {
    if (WiFi.status() == WL_CONNECTED) everConnected = true;
    else if (everConnected || uint32_t(now-wifiStarted) >= Config::STA_TIMEOUT_MS) startAP();
  } else if (!apActive) startAP();
  if (transfer.pending && (uint32_t(now-transfer.lastData) > TRANSFER_TIMEOUT || uint32_t(now-transfer.started) > TRANSFER_LIMIT)) {
    server.client().stop(); resetTransfer();
  }
  server.handleClient();
  // WebServer is synchronous; do not let the two OTA transports share Update.
  if (!transfer.pending && !rebootAt) ArduinoOTA.handle();
  if (transfer.pending && !server.client().connected()) resetTransfer();
  if (rebootAt && int32_t(millis()-rebootAt) >= 0) ESP.restart();
}
bool hasClient() { return WiFi.status() == WL_CONNECTED || (apActive && WiFi.softAPgetStationNum() > 0); }
String statusJSON() {
  const bool connected = WiFi.status() == WL_CONNECTED;
  return String("{\"mode\":") + quote(boot.station ? "sta" : "ap") + ",\"savedMode\":" + quote(saved.station ? "sta" : "ap") +
    ",\"savedSSID\":" + quote(saved.ssid) + ",\"pendingReboot\":" + (memcmp(&saved, &boot, sizeof(saved)) ? "true" : "false") +
    ",\"connected\":" + (connected ? "true" : "false") + ",\"ssid\":" + quote(connected ? WiFi.SSID() : String(boot.ssid)) +
    ",\"rssi\":" + (connected ? String(WiFi.RSSI()) : String("null")) +
    ",\"ip\":" + quote(connected ? WiFi.localIP().toString() : (apActive ? WiFi.softAPIP().toString() : String("0.0.0.0"))) +
    ",\"apSSID\":" + quote(apName) + ",\"apIP\":" + quote(apActive ? WiFi.softAPIP().toString() : String("")) +
    ",\"hostname\":\"povstaff.local\",\"fallback\":" + (boot.station && apActive ? "true" : "false") + "}";
}
}
