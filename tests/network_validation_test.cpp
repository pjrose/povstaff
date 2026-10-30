#include "../code/POV4.1/NetworkValidation.h"
#include <cassert>
#include <cstdio>
#include <cstring>
using namespace NetworkValidation;
int main() {
  uint32_t n = 99;
  assert(number("0", 64, n) && n == 0);
  assert(number("64", 64, n) && n == 64);
  assert(!number("65", 64, n));
  assert(!number("-1", 64, n));
  assert(!number("1junk", 64, n));
  assert(!number("", 64, n));
  assert(!number("4294967296", UINT32_MAX, n));
  assert(number("4294967295", UINT32_MAX, n) && n == UINT32_MAX);
  assert(!number("9", 1, n));
  WifiSettings saved{}; saved.magic = WIFI_MAGIC;
  assert(valid(saved)); // Empty factory configuration is AP.
  const WifiSettings boot = saved;
  saved.station = 1;
  assert(!valid(saved)); // Never select client without an SSID.
  strcpy(saved.ssid, "Home WiFi");
  assert(valid(saved)); // Explicit open-network password is accepted.
  strcpy(saved.password, "short"); assert(!valid(saved));
  strcpy(saved.password, "correct horse battery staple"); assert(valid(saved));
  assert(boot.station == 0 && boot.ssid[0] == 0); // Saved changes leave boot snapshot intact.
  auto corrupt = saved; corrupt.magic = 0; assert(!valid(corrupt));
  corrupt = saved; corrupt.station = 2; assert(!valid(corrupt));
  corrupt = saved; memset(corrupt.ssid, 'x', sizeof(corrupt.ssid)); assert(!valid(corrupt));
  corrupt = saved; memset(corrupt.password, 'x', sizeof(corrupt.password)); assert(!valid(corrupt));
  corrupt = saved; corrupt.password[0] = '\n'; assert(!valid(corrupt));
  corrupt = saved; corrupt.reserved[0] = 1; assert(!valid(corrupt));
  assert(complete(1024, 1024, 1024));
  assert(!complete(0, 0, 0));
  assert(!complete(1023, 1024, 1024));
  assert(!complete(1024, 1023, 1024));
  assert(!complete(1024, 1024, 1025));
  puts("network validation: passed");
}
