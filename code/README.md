# Firmware source

Open `POV4.1/POV4.1.ino` for the Island Robotics V4.1 staff with an 8 MB Adafruit QT Py ESP32-S3.

| Module | Responsibility |
| --- | --- |
| `Engine` / `EngineMath` | Startup, motion, supply monitoring, rendering and output limits |
| `Patterns` | Procedural modes, including motion-driven walking pulses |
| `Sequences` / `SequenceFormat` | Validated BMP storage, playlist and row decoding |
| `StaffNetwork` / `NetworkValidation` | AP/client Wi-Fi, dashboard API, uploads and OTA |
| `Dashboard` / `Preview` | Embedded dashboard and browser canvas previews |
| `LSM6` | IMU driver |
| `Config` | Hardware constants and runtime defaults |

See [Build and flash](../BUILD.md), [Dashboard usage](../README.md) and [API reference](../API.md).
