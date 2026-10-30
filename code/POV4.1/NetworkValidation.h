#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>
namespace NetworkValidation {
inline bool number(const char* text, uint32_t limit, uint32_t& out) {
  if (!text || !*text) return false;
  uint32_t n = 0;
  for (; *text; ++text) {
    if (*text < '0' || *text > '9') return false;
    const uint32_t digit = uint32_t(*text - '0');
    if (digit > limit || n > (limit - digit) / 10) return false;
    n = n * 10 + digit;
  }
  out = n; return true;
}
struct WifiSettings {
  uint32_t magic;
  uint8_t station;
  char ssid[33];
  char password[65];
  uint8_t reserved[1];
};
constexpr uint32_t WIFI_MAGIC = 0x504F5635;
inline bool valid(const WifiSettings& value) {
  if (value.magic != WIFI_MAGIC || value.station > 1 || value.reserved[0] != 0 ||
      !memchr(value.ssid, 0, sizeof(value.ssid)) || !memchr(value.password, 0, sizeof(value.password))) return false;
  const size_t ssid = strlen(value.ssid), password = strlen(value.password);
  if (value.station && ssid == 0) return false;
  if (password && (password < 8 || password > 63)) return false;
  for (size_t i = 0; i < ssid; ++i) if (uint8_t(value.ssid[i]) < 32) return false;
  for (size_t i = 0; i < password; ++i) if (uint8_t(value.password[i]) < 32 || uint8_t(value.password[i]) > 126) return false;
  return true;
}
inline bool complete(size_t received, size_t parserTotal, size_t expected) {
  return received > 0 && received == parserTotal && received == expected;
}
}
