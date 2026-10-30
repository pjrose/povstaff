#pragma once
#include "FS.h"
struct HostLittleFS {
  std::map<std::string, std::vector<uint8_t>> files;
  bool mountFailure = false;
  unsigned formats = 0;
  bool begin(bool autoFormat) { if (autoFormat) ++formats; return !mountFailure; }
  void end() {}
  bool format() { ++formats; files.clear(); mountFailure = false; return true; }
  bool remove(const char* path) { return files.erase(path) != 0; }
  File open(const char* path, const char* = FILE_READ) {
    if (mountFailure) return File();
    if (std::string(path) == "/") return File(&files);
    auto it = files.find(path); return it == files.end() ? File() : File(it->first, &it->second);
  }
  File open(const String& path, const char* mode = FILE_READ) { return open(path.c_str(), mode); }
  size_t totalBytes() const { return 0x180000; }
  size_t usedBytes() const { size_t n=0; for (const auto& file : files) n += file.second.size(); return n; }
};
extern HostLittleFS LittleFS;
