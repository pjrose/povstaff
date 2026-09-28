#pragma once
#include <memory>
#include <map>
#include <vector>
#include <string>
#include <cstring>
#include <algorithm>
#define FILE_READ "r"
class File {
  struct State {
    std::string name;
    std::vector<uint8_t>* data = nullptr;
    std::map<std::string, std::vector<uint8_t>>* directory = nullptr;
    size_t position = 0;
  };
  std::shared_ptr<State> state;
public:
  File() = default;
  File(const std::string& name, std::vector<uint8_t>* data) : state(std::make_shared<State>()) {
    state->name = name; state->data = data;
  }
  explicit File(std::map<std::string, std::vector<uint8_t>>* dir) : state(std::make_shared<State>()) { state->directory = dir; }
  explicit operator bool() const { return bool(state); }
  bool isDirectory() const { return state && state->directory; }
  const char* name() const { return state ? state->name.c_str() : ""; }
  size_t size() const { return state && state->data ? state->data->size() : 0; }
  void close() { state.reset(); }
  bool seek(size_t position) { if (!state || position > size()) return false; state->position = position; return true; }
  size_t read(uint8_t* out, size_t n) {
    if (!state || !state->data) return 0;
    n = std::min(n, size() - state->position);
    std::memcpy(out, state->data->data() + state->position, n); state->position += n; return n;
  }
  File openNextFile() {
    if (!isDirectory() || state->position >= state->directory->size()) return File();
    auto it = state->directory->begin(); std::advance(it, state->position++);
    return File(it->first, &it->second);
  }
};
