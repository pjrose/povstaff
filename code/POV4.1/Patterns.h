#pragma once
#include <stdint.h>
#include <stddef.h>
namespace Patterns {
struct RGB { float r,g,b; };
struct Context { float seconds; float speed; uint32_t color; };
using Render = void (*)(RGB* pixels, size_t count, const Context& context);
struct Definition { const char* name; Render render; };
RGB hsv(float hue, float saturation=1, float value=1);
const Definition* find(const char* name);
// New effects need only a render function and one registry entry in Patterns.cpp.
// Renderers write linear RGB [0,1]; the engine owns gamma, brightness and current.
}
