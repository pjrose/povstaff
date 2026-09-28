#pragma once
#include <Arduino.h>

// The staff AP, dashboard and firmware uploads require no password.
// Home Wi-Fi credentials/mode are saved through the dashboard and read
// ONLY at boot. The home Wi-Fi password is never included in status JSON.
namespace Config {
constexpr char VERSION[] = "5.1.1";
constexpr char HOSTNAME[] = "povstaff";
constexpr uint16_t PIXELS = 72;
constexpr uint8_t STRIPS = 4;
constexpr uint8_t VSENSE_PIN = 9;
constexpr uint8_t DEFAULT_BRIGHTNESS = 24;
constexpr uint8_t STARTUP_BRIGHTNESS = 3;
constexpr uint8_t MAX_BRIGHTNESS = 64;
constexpr uint8_t FLASHLIGHT_MAX_BRIGHTNESS = 20;
constexpr float MAX_LED_CURRENT_MA = 1800.0f;
constexpr float USB_LED_CURRENT_MA = 500.0f; // LED estimate including quiescent pixels; excludes MCU/radio
constexpr uint8_t USB_MAX_BRIGHTNESS = 16;
constexpr float BATTERY_MAX_VOLTAGE = 4.30f;
constexpr float USB_MIN_VOLTAGE = 4.45f;
constexpr float USB_MAX_VOLTAGE = 5.50f;
constexpr float BATTERY_MULTIPLIER = 2.0f;
constexpr float BATTERY_OFFSET = 0.0f;
constexpr float CRITICAL_VOLTAGE = 3.20f;
constexpr uint32_t STA_TIMEOUT_MS = 10000;
constexpr uint32_t FRAME_INTERVAL_US = 10000;
constexpr uint32_t MAX_IMAGE_BYTES = 64000;
constexpr uint16_t MAX_FILES = 50;
constexpr uint32_t DEFAULT_SEQUENCE_SECONDS = 15;
}
