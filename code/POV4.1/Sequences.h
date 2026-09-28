#pragma once
#include <Arduino.h>
#include <FS.h>
namespace Sequences {
struct Entry { char name[49]; uint32_t bytes; uint16_t width; uint16_t height; };
bool begin();
bool mounted();
const char* error();
bool format(); // explicit user confirmation only
bool reload();
size_t count();
const Entry& entry(size_t index);
int currentIndex();
const char* currentName();
bool select(size_t index);
bool next(int direction = 1);
void setDuration(uint32_t seconds); // 0 = hold; 1..3600
uint32_t duration();
void tick(uint32_t now);
void restartPlaybackClock(); // give the selected image a full interval on POV entry
// RGB output in logical top-to-bottom BMP order, wraps line index.
bool readLine(uint32_t line, uint8_t* rgb);
uint16_t lines();
uint32_t revision(); // changes when selection/content changes
bool validName(const String& name);
bool validateBMP(const char* path, String& reason);
String filesJSON();
size_t totalBytes();
size_t usedBytes();
}
