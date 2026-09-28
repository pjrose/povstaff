#include "Sequences.h"
#include "Config.h"
#include "SequenceFormat.h"
#include <LittleFS.h>
#include <cstring>

namespace Sequences {
namespace {
Entry catalogue[Config::MAX_FILES];
uint8_t image[Config::MAX_IMAGE_BYTES];
SequenceFormat::BMP imageInfo;
bool fsReady = false, imageReady = false;
size_t imageCount = 0;
int selected = -1;
uint32_t imageRevision = 0, selectedAt = 0;
uint32_t intervalSeconds = Config::DEFAULT_SEQUENCE_SECONDS;
String lastError;

bool metadata(const char* path, SequenceFormat::BMP& result, String& reason) {
  File f = LittleFS.open(path, FILE_READ);
  if (!f || f.isDirectory()) { reason = "Cannot open image"; return false; }
  uint8_t header[54];
  const size_t got = f.read(header, sizeof(header));
  const char* failure = SequenceFormat::parseBMP(header, got, f.size(), Config::PIXELS,
                                                Config::MAX_IMAGE_BYTES, result);
  if (failure) { reason = failure; return false; }
  reason = "";
  return true;
}
}
bool mounted() { return fsReady; }
const char* error() { return lastError.c_str(); }
bool begin() {
  fsReady = LittleFS.begin(false); // NEVER erase user data on mount failure.
  if (!fsReady) { lastError = "LittleFS unavailable. Initialize storage explicitly in Settings."; return false; }
  LittleFS.remove("/.upload.tmp"); // discard interrupted, unpublished upload
  return reload();
}
bool format() {
  imageReady = false; imageCount = 0; selected = -1; ++imageRevision;
  LittleFS.end(); fsReady = false;
  if (!LittleFS.format()) { lastError = "Filesystem format failed"; return false; }
  return begin();
}
bool validName(const String& name) {
  // A String can contain embedded NUL; do not validate only its prefix.
  return name.length() == strlen(name.c_str()) && SequenceFormat::validName(name.c_str());
}
bool validateBMP(const char* path, String& reason) {
  if (!fsReady) { reason = "Storage unavailable"; return false; }
  SequenceFormat::BMP result;
  return metadata(path, result, reason);
}
bool reload() {
  if (!fsReady) return false;
  char previous[49] = {};
  if (selected >= 0 && size_t(selected) < imageCount) strcpy(previous, catalogue[selected].name);
  imageReady = false; imageCount = 0; selected = -1; ++imageRevision;
  lastError = "";
  File root = LittleFS.open("/");
  if (!root || !root.isDirectory()) { lastError = "Cannot list storage"; return false; }
  File f = root.openNextFile();
  unsigned skipped = 0;
  while (f) {
    String name = f.name();
    if (name.startsWith("/")) name.remove(0, 1);
    const bool candidate = !f.isDirectory() && validName(name);
    const uint32_t bytes = uint32_t(f.size());
    f.close();
    if (candidate) {
      SequenceFormat::BMP info;
      String reason;
      if (imageCount < Config::MAX_FILES && metadata(("/" + name).c_str(), info, reason)) {
        Entry& e = catalogue[imageCount++];
        strcpy(e.name, name.c_str()); e.bytes = bytes; e.width = info.width; e.height = info.height;
      } else ++skipped;
    }
    f = root.openNextFile();
  }
  root.close();
  for (size_t i = 1; i < imageCount; ++i) {
    Entry temp = catalogue[i]; size_t j = i;
    while (j > 0 && strcmp(catalogue[j-1].name, temp.name) > 0) {
      catalogue[j] = catalogue[j-1]; --j;
    }
    catalogue[j] = temp;
  }
  if (imageCount) {
    size_t target = 0;
    for (size_t i = 0; i < imageCount; ++i) if (strcmp(previous, catalogue[i].name) == 0) target = i;
    if (!select(target)) return false;
  }
  if (skipped) lastError = String(skipped) + " invalid or excess BMP file(s) skipped";
  return true;
}
size_t count() { return imageCount; }
const Entry& entry(size_t index) {
  static const Entry empty = {};
  return index < imageCount ? catalogue[index] : empty;
}
int currentIndex() { return selected; }
const char* currentName() { return selected >= 0 ? catalogue[selected].name : ""; }
bool select(size_t index) {
  if (!fsReady || index >= imageCount) return false;
  const String path = String("/") + catalogue[index].name;
  SequenceFormat::BMP info;
  String reason;
  if (!metadata(path.c_str(), info, reason)) { lastError = reason; return false; }
  File f = LittleFS.open(path, FILE_READ);
  imageReady = false; selected = -1; ++imageRevision;
  if (!f || !f.seek(info.offset) || f.read(image, info.bytes) != info.bytes) {
    lastError = "Image read failed"; return false;
  }
  imageInfo = info; imageReady = true; selected = int(index); selectedAt = millis();
  lastError = "";
  return true;
}
bool next(int direction) {
  if (!imageCount) return false;
  int target = selected < 0 ? 0 : (selected + (direction < 0 ? int(imageCount) - 1 : 1)) % int(imageCount);
  return select(size_t(target));
}
void setDuration(uint32_t seconds) { intervalSeconds = seconds > 3600 ? 3600 : seconds; selectedAt = millis(); }
uint32_t duration() { return intervalSeconds; }
void restartPlaybackClock() { selectedAt = millis(); }
void tick(uint32_t now) {
  if (imageCount > 1 && intervalSeconds && now - selectedAt >= intervalSeconds * 1000U) {
    selectedAt = now;
    next();
  }
}
bool readLine(uint32_t line, uint8_t* rgb) {
  if (!imageReady || !imageInfo.height) { memset(rgb, 0, Config::PIXELS * 3); return false; }
  line %= imageInfo.height;
  const uint32_t row = imageInfo.topDown ? line : imageInfo.height - 1U - line;
  const uint8_t* bgr = image + row * imageInfo.stride;
  for (size_t x = 0; x < Config::PIXELS; ++x) {
    rgb[3*x] = bgr[3*x+2]; rgb[3*x+1] = bgr[3*x+1]; rgb[3*x+2] = bgr[3*x];
  }
  return true;
}
uint16_t lines() { return imageReady ? imageInfo.height : 0; }
uint32_t revision() { return imageRevision; }
size_t totalBytes() { return fsReady ? LittleFS.totalBytes() : 0; }
size_t usedBytes() { return fsReady ? LittleFS.usedBytes() : 0; }
String filesJSON() {
  String json;
  json.reserve(256 + imageCount * 128);
  json = "{\"files\":[";
  for (size_t i = 0; i < imageCount; ++i) {
    if (i) json += ',';
    const Entry& e = catalogue[i];
    json += "{\"index\":" + String(i) + ",\"name\":\"" + e.name + "\",\"bytes\":" + String(e.bytes);
    json += ",\"width\":" + String(e.width) + ",\"height\":" + String(e.height);
    json += ",\"active\":"; json += int(i) == selected ? "true}" : "false}";
  }
  json += "],\"currentIndex\":" + String(selected) + ",\"currentName\":\"" + currentName();
  json += "\",\"duration\":" + String(intervalSeconds) + ",\"count\":" + String(imageCount);
  // lastError consists only of fixed firmware messages and decimal counts.
  json += ",\"error\":\"" + lastError + "\"}";
  return json;
}
}
