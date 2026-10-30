#pragma once
// Minimal host adapter for testing real Sequences.cpp without an ESP32.
#include <cstdint>
#include <string>
#include <type_traits>
using std::size_t;
class String {
  std::string value;
public:
  String() = default;
  String(const char* s) : value(s ? s : "") {}
  String(const std::string& s) : value(s) {}
  template<class T, typename std::enable_if<std::is_arithmetic<T>::value, int>::type = 0>
  String(T n) : value(std::to_string(n)) {}
  const char* c_str() const { return value.c_str(); }
  size_t length() const { return value.size(); }
  bool startsWith(const char* s) const { return value.rfind(s, 0) == 0; }
  void remove(size_t offset, size_t n) { value.erase(offset, n); }
  void reserve(size_t n) { value.reserve(n); }
  String& operator+=(const String& s) { value += s.value; return *this; }
  String& operator+=(char c) { value += c; return *this; }
  friend String operator+(const String& a, const String& b) { return String(a.value + b.value); }
};
extern uint32_t testMillis;
inline uint32_t millis() { return testMillis; }
