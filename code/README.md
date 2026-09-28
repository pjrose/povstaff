# POV Staff firmware

The maintained sketch is `POV4.1/POV4.1.ino`, implementing the version 5 breaking replacement for Island Robotics V4.1 hardware and the 8 MB Adafruit QT Py ESP32-S3.

See [BUILD.md](../BUILD.md) for pinned dependencies, board/partition settings, USB flashing, Arduino IDE setup and both OTA methods. See [README.md](../README.md) for dashboard operation and [MIGRATION.md](../MIGRATION.md) for changes from stock.

No `pov-esp32` or `ESPxWebFlMgr` installation is needed. Load light sequences through the dashboard.

Installed release **5.1.1** retains the open staff AP, no dashboard login and password-free ArduinoOTA. Optional home Wi-Fi credentials remain saved in NVS and apply on the next boot. The current hardware setup is left in saved client mode with all eight images retained. The USB port observed after hardware CDC re-enumeration is COM5.

The dashboard previews BMPs locally in the browser, with up to 12 decoded images cached in memory and no generated LittleFS assets. Its geometry follows the documented two identical 72-pixel, 50 cm arms around a 41 cm unlit grip. All three views passed live browser checks on 5.1.1; Android hardware testing remains outstanding.

Version 5.0.4 adds USB-host detection and USB supply classification, with brightness capped at 16/255 and estimated LED current at 500 mA whenever a USB host is detected. Output telemetry and live dashboard checks passed with the serial monitor closed; physical LED observation and electrical-current measurement remain outstanding. Battery voltage protection remains enabled.
