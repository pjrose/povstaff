#pragma once
// Portable, allocation-free input validation shared by firmware and host tests.
#include <stdint.h>
#include <stddef.h>
#include <string.h>
namespace SequenceFormat {
constexpr size_t NAME_LIMIT = 48;
inline bool validName(const char* name) {
  const size_t n = strlen(name);
  if (n < 5 || n > NAME_LIMIT) return false;
  for (size_t i = 0; i < n - 4; ++i) {
    const char c = name[i];
    if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
          (c >= '0' && c <= '9') || c == '-' || c == '_')) return false;
  }
  return name[n-4] == '.' && (name[n-3] == 'b' || name[n-3] == 'B') &&
         (name[n-2] == 'm' || name[n-2] == 'M') && (name[n-1] == 'p' || name[n-1] == 'P');
}
inline uint16_t le16(const uint8_t* p) { return uint16_t(p[0]) | (uint16_t(p[1]) << 8); }
inline uint32_t le32(const uint8_t* p) {
  return uint32_t(p[0]) | (uint32_t(p[1]) << 8) | (uint32_t(p[2]) << 16) | (uint32_t(p[3]) << 24);
}
struct BMP {
  uint32_t offset = 0, stride = 0, bytes = 0;
  uint16_t width = 0, height = 0;
  bool topDown = false;
};
inline const char* parseBMP(const uint8_t* h, size_t headerSize, size_t fileSize,
                           uint16_t pixels, uint32_t maxBytes, BMP& out) {
  out = BMP{};
  if (headerSize < 54 || fileSize < 54) return "Truncated BMP header";
  if (h[0] != 'B' || h[1] != 'M') return "Expected a BMP image";
  const uint32_t dib = le32(h + 14), offset = le32(h + 10);
  if (dib < 40 || dib > 124 || offset < 14 + dib || offset > 4096) return "Unsupported BMP header";
  if (le16(h + 26) != 1 || le16(h + 28) != 24 || le32(h + 30) != 0)
    return "Use uncompressed 24-bit RGB BMP";
  const uint32_t width = le32(h + 18), rawHeight = le32(h + 22);
  const bool topDown = (rawHeight & 0x80000000UL) != 0;
  const uint32_t height = topDown ? (~rawHeight + 1U) : rawHeight;
  if (width != pixels) return "Image width must be 72 pixels";
  if (height == 0 || height > 65535) return "Invalid BMP height";
  const uint32_t stride = (width * 3U + 3U) & ~3U;
  if (stride == 0 || height > maxBytes / stride) return "Image pixel data exceeds 64000 bytes";
  const uint32_t bytes = stride * height;
  if (offset > fileSize || bytes > fileSize - offset) return "Truncated BMP pixel data";
  const uint32_t declared = le32(h + 2);
  if (declared && (declared > fileSize || declared < offset + bytes)) return "Invalid BMP file size";
  if (fileSize > maxBytes + 4096U) return "BMP file too large";
  out.offset = offset; out.stride = stride; out.bytes = bytes;
  out.width = uint16_t(width); out.height = uint16_t(height); out.topDown = topDown;
  return nullptr;
}
}
