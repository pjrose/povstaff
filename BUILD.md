# Build and flash

The maintained sketch is `code/POV4.1/POV4.1.ino` for **Island Robotics V4.1 / Adafruit QT Py ESP32-S3 with 8 MB flash and no PSRAM**. See [README.md](README.md) for operation and [API.md](API.md) for automation.

## Toolchain

| Component | Version / setting |
| --- | --- |
| Arduino CLI | 1.5.1 |
| ESP32 Arduino core | 3.3.11 |
| Adafruit NeoPixel | 1.15.5 |
| Board | `esp32:esp32:adafruit_qtpy_esp32s3_nopsram` |
| Partition scheme | `default_8MB`: two 0x330000-byte app slots, 0x180000-byte LittleFS |
| USB mode / CDC on boot | Hardware CDC and JTAG (`hwcdc`) / enabled (`cdc`) |
| Erase all flash | Disabled (`none`) |

The ESP32 core supplies networking, storage and OTA libraries. No separate filesystem image or preview assets are needed: the dashboard and preview module are embedded in firmware.

## Build and test

Run from the repository root on Windows:

```powershell
# Install pinned dependencies and build:
.\tools\build.ps1 -Setup
# Subsequent builds:
.\tools\build.ps1
# Host tests (Visual Studio C++ Build Tools and Node.js required):
.\tools\test.cmd
```

The build script finds Arduino CLI on PATH or in the standard Arduino IDE installation. Pass `-ArduinoCli` to use another executable. Output goes to `.build/release/`; **POV4.1.ino.bin** is the application image used for OTA.

Equivalent compile command on other supported hosts:

```sh
arduino-cli compile --fqbn 'esp32:esp32:adafruit_qtpy_esp32s3_nopsram:PartitionScheme=default_8MB,USBMode=hwcdc,CDCOnBoot=cdc,EraseFlash=none' --output-dir .build/release code/POV4.1
```

For Arduino IDE, install the pinned core/libraries, open `code/POV4.1/POV4.1.ino`, and select the board and Tools settings above. Keep the sketch's `.h` and `.cpp` files together.

## First USB flash

Connect a data-capable USB-C cable, identify the current serial port, and pass it to the build script. Replace the example port as needed:

```powershell
.\tools\build.ps1 -Port COM5
```

If connection fails, hold BOOT, press/release RESET, release BOOT, then retry. The initial upload establishes the OTA partition layout. Keeping erase disabled preserves saved Wi-Fi settings; changing partition layouts may invalidate sequence storage.

On a fresh configuration, join the open **POVSTAFFxxxx** network and open [192.168.4.1](http://192.168.4.1/) or [povstaff.local](http://povstaff.local/). There is no login. If client mode was already saved, use the home LAN instead. Initialize storage under **Device → Maintenance & storage recovery** only if needed, then upload BMPs through the dashboard.

## Wireless updates

**Browser:** build with the same board/partition settings, open Firmware Update in the dashboard, and upload `.build/release/POV4.1.ino.bin`. Main LEDs blank during transfer. A completed, validated update reboots onto the saved network. Do not upload merged, partition or bootloader binaries.

**ArduinoOTA:** select the `povstaff` network port in Arduino IDE, or run the official `espota.py` from the pinned ESP32 core:

```sh
python espota.py -i povstaff.local -p 3232 -f .build/release/POV4.1.ino.bin
```

Use `192.168.4.1` for a direct AP connection if mDNS is unavailable, or the staff's current LAN address in client mode. No password is required. Allow UDP 3232 and the TCP connection back to the uploader; `-I <computer-IPv4> -P 32320` selects the host interface and callback port when needed. Filesystem OTA is disabled. Application images must fit the inactive slot; signed images and automatic rollback are not configured.

## Runtime defaults

Normal brightness starts at **5/255** after a **3/255** startup battery indication. Battery operation caps brightness at 64/255, flashlight at 20/255 and estimated LED current at 1,800 mA. A detected USB host caps brightness at 16/255 and estimated LED current at 500 mA, excluding MCU/radio consumption. Hardware CDC detects the USB host without an open serial monitor. Battery voltage at/below 3.20 V blocks normal output; recovery requires 3.35 V. Current and battery percentage are estimates.

Wi-Fi settings persist in NVS and apply at the next boot. Other dashboard controls reset on boot. Uploading images, updating firmware or formatting sequence storage does not erase saved Wi-Fi settings.
