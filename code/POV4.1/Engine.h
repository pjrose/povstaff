#pragma once
#include <Arduino.h>
namespace Engine {
void begin(); // deterministic early black; call before FS/network
void update(); // cooperative startup, IMU, battery, animation, heartbeat
void maintenance(bool active); // blackout during upload/OTA, resume on release
bool setMode(const String& name);
bool setPattern(const String& name);
void setLocked(bool locked);
void setBrightness(uint8_t value);
void setColor(uint32_t rgb);
bool setRenderer(const String& name); // baseline or dither; always baseline at boot
void showBattery();
String statusJSON(); // JSON object with mode, battery, IMU, power info
}
