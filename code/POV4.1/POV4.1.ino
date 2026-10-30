/* POV Staff 5 — Island Robotics V4.1 / Adafruit QT Py ESP32-S3 8MB.
 * MIT license. See BUILD.md for the pinned toolchain and first USB flash.
 */
#include "Config.h"
#include "Engine.h"
#include "Sequences.h"
#include "StaffNetwork.h"
#include <esp_system.h>

void setup() {
  // Own the LED pins and send black before initializing storage or radio.
  Engine::begin();
  Serial.begin(115200);
  Serial.printf("\nPOV Staff %s | reset=%d | heap=%lu\n", Config::VERSION,
                int(esp_reset_reason()), (unsigned long)ESP.getFreeHeap());
  const bool storageOK = Sequences::begin();
  Serial.printf("LittleFS: %s | images=%u | %s\n", storageOK ? "ready" : "error",
                unsigned(Sequences::count()), Sequences::error());
  StaffNetwork::begin();
  Serial.printf("Ready | heap=%lu | brightness=%u/%u | estimated LED limit=%.0f mA\n",
                (unsigned long)ESP.getFreeHeap(), Config::DEFAULT_BRIGHTNESS,
                Config::MAX_BRIGHTNESS, Config::MAX_LED_CURRENT_MA);
}

void loop() {
  Engine::update();
  StaffNetwork::update();
  Engine::update();
  delay(1); // Give the radio/RTOS a scheduling opportunity; no mode owns loop().
}
